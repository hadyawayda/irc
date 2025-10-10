#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "CommandHandler.hpp"
#include "Utils.hpp"

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

// Construct the server: initialize containers, create the listening socket,
// and instantiate helper subsystems (bot and file transfer).
Server::Server(const std::string& port, const std::string& password)
: _listen_fd(-1), _password(password), _servername("ircserv"),
  _bot(0), _ft(0) // NEW
{
    setupSocket(port);
    // NEW: create subsystems
    _bot = new Bot(*this, "helperbot");
    _ft  = new FileTransfer(*this);
}

// Destructor: close sockets and free owned objects.
Server::~Server() {
    closeAndCleanup();
    // NEW
    delete _bot; _bot = 0;
    delete _ft;  _ft = 0;
}

const std::string& Server::serverName() const { return _servername; }

// Create a non-blocking listening socket bound to the requested port and
// add it to the poll() set for connection readiness notifications.
void Server::setupSocket(const std::string& port) {
    struct addrinfo hints; std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* res = 0;
    int err = getaddrinfo(NULL, port.c_str(), &hints, &res);
    if (err != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(err) << std::endl;
        std::exit(1);
    }

    int fd = -1;
    for (struct addrinfo* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(fd, p->ai_addr, p->ai_addrlen) == 0) {
            if (listen(fd, 128) == 0) {
                _listen_fd = fd;
                break;
            }
        }
        close(fd); fd = -1;
    }
    freeaddrinfo(res);

    if (_listen_fd < 0) {
        std::cerr << "Failed to bind/listen" << std::endl;
        std::exit(1);
    }

    // set non-blocking
    fcntl(_listen_fd, F_SETFL, O_NONBLOCK);

    addPollfd(_listen_fd, POLLIN);
}

// Track an fd with the desired poll events (e.g., POLLIN or POLLIN|POLLOUT).
void Server::addPollfd(int fd, short events) {
    struct pollfd p; p.fd = fd; p.events = events; p.revents = 0;
    _pfds.push_back(p);
}

// Update the desired events mask for an existing pollfd entry.
void Server::setPollEvents(int fd, short events) {
    for (size_t i = 0; i < _pfds.size(); ++i) if (_pfds[i].fd == fd) {
        _pfds[i].events = events;
        return;
    }
}

// Main event loop: poll for events, accept new clients, read lines, and write
// outbound buffers. This loop is single-threaded and runs until process exit.
void Server::run() {
    CommandHandler dispatcher(*this);
    while (true) {
        int ret = poll(&_pfds[0], _pfds.size(), -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            std::perror("poll"); break;
        }
        for (size_t i = 0; i < _pfds.size(); ++i) {
            int fd = _pfds[i].fd;
            short re = _pfds[i].revents;
            if (!re) continue;

            if (fd == _listen_fd && (re & POLLIN)) {
                handleNewConnection();
            } else {
                if (re & POLLIN) handleClientRead(fd);
                if (re & POLLOUT) handleClientWrite(fd);
                if (re & (POLLHUP | POLLERR | POLLNVAL)) removeClient(fd);
            }
        }
        // compact pollfd vector (remove closed fds)
        std::vector<struct pollfd> newpfds;
        for (size_t i = 0; i < _pfds.size(); ++i) {
            if (_pfds[i].fd != -1) newpfds.push_back(_pfds[i]);
        }
        _pfds.swap(newpfds);
    }
}

// Accept a pending connection, set it non-blocking, and create a Client
// object. Send a brief notice guiding the user to authenticate.
void Server::handleNewConnection() {
    struct sockaddr_storage ss; socklen_t slen = sizeof(ss);
    int cfd = accept(_listen_fd, (struct sockaddr*)&ss, &slen);
    if (cfd < 0) return;
    fcntl(cfd, F_SETFL, O_NONBLOCK);
    _clients[cfd] = new Client(cfd);
    addPollfd(cfd, POLLIN);
    sendToClient(cfd, ":ircserv NOTICE * :Welcome to ft_irc. Please authenticate: PASS <password>\r\n");
}

// Queue a message for a client and mark the fd POLLOUT so it will flush.
void Server::sendToClient(int fd, const std::string& msg) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client* c = it->second;
    c->outbuf().append(msg);

    for (size_t i = 0; i < _pfds.size(); ++i) if (_pfds[i].fd == fd) {
        _pfds[i].events = POLLIN | POLLOUT;
        break;
    }
}

// Attempt to flush as much of the client's out buffer as the kernel accepts.
// On error, disconnect the client.
void Server::handleClientWrite(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client* c = it->second;
    std::string& ob = c->outbuf();
    if (ob.empty()) {
        setPollEvents(fd, POLLIN);
        return;
    }
    ssize_t n = ::send(fd, ob.data(), ob.size(), 0);
    if (n > 0) {
        ob.erase(0, n);
    }
    if (n < 0 && (errno != EWOULDBLOCK && errno != EAGAIN)) {
        removeClient(fd);
        return;
    }
    if (ob.empty()) setPollEvents(fd, POLLIN);
}

// Read available bytes into the client's input buffer, split complete lines
// by CRLF, and dispatch each line to CommandHandler.
void Server::handleClientRead(int fd) {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        removeClient(fd);
        return;
    }
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client* c = it->second;
    c->inbuf().append(buf, n);

    size_t pos;
    CommandHandler dispatcher(*this);
    while ((pos = c->inbuf().find("\r\n")) != std::string::npos) {
        std::string line = c->inbuf().substr(0, pos);
        c->inbuf().erase(0, pos + 2);
        dispatcher.handleLine(*c, line);
    }
}

// Find a channel by case-insensitive name or create it (and notify the bot).
Channel* Server::getOrCreateChannel(const std::string& name) {
    std::string key = toLower(name);
    std::map<std::string, Channel*>::iterator it = _channels.find(key);
    if (it != _channels.end()) return it->second;
    Channel* ch = new Channel(name);
    _channels[key] = ch;
    // NEW: have the bot “join” (announce + help)
    if (_bot) _bot->onChannelCreated(name);
    return ch;
}

<<<<<<< Updated upstream

=======
// Lookup a channel by name; return NULL if missing.
>>>>>>> Stashed changes
Channel* Server::findChannel(const std::string& name) {
    std::string key = toLower(name);
    std::map<std::string, Channel*>::iterator it = _channels.find(key);
    if (it != _channels.end()) return it->second;
    return 0;
}

// Linear search for a client by case-insensitive nickname.
Client* Server::findClientByNick(const std::string& nick) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (toLower(it->second->nick()) == toLower(nick)) return it->second;
    }
    return 0;
}

// Convenience: run a server-injected command as if 'nickFrom' sent it.
void Server::sendServerAs(const std::string& nickFrom, const std::string& commandLine) {
    // Find the client issuing the command
    Client* c = findClientByNick(nickFrom);
    if (!c) return; // silently ignore if not present
    // Reuse CommandHandler on behalf of this client
    CommandHandler dispatcher(*this);
    std::string line = commandLine;
    // Ensure \r\n termination
    if (line.size() < 2 || line.substr(line.size()-2) != "\r\n") line += "\r\n";
    // The command handler expects lines without CRLF
    line.erase(line.size()-2);
    dispatcher.handleLine(*c, line);
}

// Send a prepared message to all channel members, optionally skipping one fd.
void Server::broadcast(const std::string& chan, const std::string& msg, int except_fd) {
    Channel* c = findChannel(chan);
    if (!c) return;
    const std::set<int>& mem = c->members();
    for (std::set<int>::const_iterator it = mem.begin(); it != mem.end(); ++it) {
        if (*it == except_fd) continue;
        sendToClient(*it, msg);
    }
}

<<<<<<< Updated upstream
=======
// ---- when a member leaves a channel (PART/QUIT/KICK) ----
// Handle state after a member leaves: auto-reop if needed, and delete the
// channel if it is now empty.
void Server::onMemberLeftChannel(Channel* ch, const std::string& lower_key, const std::string& nickJustLeft) {
    (void)lower_key;
    (void)nickJustLeft;
    if (!ch) return;
    // If no operators remain, auto-promote first member
    autoReopIfNone(ch);
    // If empty, delete channel
    maybeDeleteChannel(toLower(ch->name()));
}

// If the channel has no members, free it and remove from the map.
void Server::maybeDeleteChannel(const std::string& lower_key) {
    std::map<std::string, Channel*>::iterator it = _channels.find(lower_key);
    if (it == _channels.end()) return;
    Channel* ch = it->second;
    if (ch->members().empty()) {
        delete ch;
        _channels.erase(it);
    }
}

// Ensure at least one operator exists by promoting the first found member if
// the channel currently has none.
void Server::autoReopIfNone(Channel* ch) {
    if (!ch) return;
    if (ch->members().empty()) return;
    if (ch->hasAnyOp()) return;

    // Promote the first member we can find (by fd order)
    const std::set<int>& mem = ch->members();
    for (std::set<int>::const_iterator it = mem.begin(); it != mem.end(); ++it) {
        std::map<int, Client*>::iterator cit = _clients.find(*it);
        if (cit == _clients.end() || !cit->second) continue;
        Client* m = cit->second;
        ch->addOp(m->nick());
        std::string line = ":" + _servername + " MODE " + ch->name() + " +o " + m->nick() + "\r\n";
        broadcast(ch->name(), line, -1);
        break;
    }
}

// Disconnect a client: broadcast QUIT to channels, remove membership and ops,
// close the socket, and free the Client object.
>>>>>>> Stashed changes
void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client* c = it->second;

    for (std::set<std::string>::const_iterator sit = c->channels().begin(); sit != c->channels().end(); ++sit) {
        Channel* ch = findChannel(*sit);
        if (ch) {
            ch->removeMember(fd);
            broadcast(*sit, ":" + c->nick() + " QUIT :Client disconnected\r\n", fd);
        }
    }

    close(fd);
    for (size_t i = 0; i < _pfds.size(); ++i) if (_pfds[i].fd == fd) _pfds[i].fd = -1;

    delete c;
    _clients.erase(it);
}

// Close the listening socket and free all Clients and Channels. Called on
// orderly shutdown and from the destructor.
void Server::closeAndCleanup() {
    if (_listen_fd != -1) close(_listen_fd);
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;
    }
    _clients.clear();
    for (std::map<std::string, Channel*>::iterator ct = _channels.begin(); ct != _channels.end(); ++ct) {
        delete ct->second;
    }
    _channels.clear();
}
