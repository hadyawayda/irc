#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "CommandHandler.hpp"
#include "Utils.hpp"
#include "Bot.hpp"
#include "FileTransfer.hpp"

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

Server::Server(const std::string& port, const std::string& password)
: _clients(),
  _channels(),
  _password(password),
  _servername("ircserv"),
  _bot(0),
  _ft(0),
  _listen_fd(-1)         // must appear AFTER _ft to match declaration order
{
    setupSocket(port);
    _bot = new Bot(*this, "helperbot");
    _ft  = new FileTransfer(*this);
}

Server::~Server() {
    closeAndCleanup();
    delete _bot; _bot = 0;
    delete _ft;  _ft  = 0;
}

const std::string& Server::serverName() const { return _servername; }

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

    fcntl(_listen_fd, F_SETFL, O_NONBLOCK);
    addPollfd(_listen_fd, POLLIN);
}

void Server::addPollfd(int fd, short events) {
    struct pollfd p; p.fd = fd; p.events = events; p.revents = 0;
    _pfds.push_back(p);
}

void Server::setPollEvents(int fd, short events) {
    for (size_t i = 0; i < _pfds.size(); ++i) if (_pfds[i].fd == fd) {
        _pfds[i].events = events;
        return;
    }
}

void Server::run() {
    while (true) {
        int ret = poll(&_pfds[0], _pfds.size(), -1);
        if (ret < 0) {
            continue;
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
        std::vector<struct pollfd> newpfds;
        for (size_t i = 0; i < _pfds.size(); ++i) {
            if (_pfds[i].fd != -1) newpfds.push_back(_pfds[i]);
        }
        _pfds.swap(newpfds);
    }
}

void Server::handleNewConnection() {
    struct sockaddr_storage ss; socklen_t slen = sizeof(ss);
    int cfd = accept(_listen_fd, (struct sockaddr*)&ss, &slen);
    if (cfd < 0) return;
    fcntl(cfd, F_SETFL, O_NONBLOCK);
    _clients[cfd] = new Client(cfd);
    addPollfd(cfd, POLLIN);
    sendToClient(cfd, ":ircserv NOTICE * :Welcome to ft_irc. Please authenticate: PASS <password>\r\n");
}

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
    if (n > 0) ob.erase(0, n);
    if (n < 0) {
        removeClient(fd);
        return;
    }
    if (ob.empty()) setPollEvents(fd, POLLIN);
}

void Server::handleClientRead(int fd) {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) { removeClient(fd); return; }
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

Channel* Server::getOrCreateChannel(const std::string& name) {
    std::string key = toLower(name);
    std::map<std::string, Channel*>::iterator it = _channels.find(key);
    if (it != _channels.end()) return it->second;
    Channel* ch = new Channel(name);
    _channels[key] = ch;
    if (_bot) _bot->onChannelCreated(name);
    return ch;
}

Channel* Server::findChannel(const std::string& name) {
    std::string key = toLower(name);
    std::map<std::string, Channel*>::iterator it = _channels.find(key);
    if (it != _channels.end()) return it->second;
    return 0;
}

Client* Server::findClientByNick(const std::string& nick) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (toLower(it->second->nick()) == toLower(nick)) return it->second;
    }
    return 0;
}

void Server::sendServerAs(const std::string& nickFrom, const std::string& commandLine) {
    Client* c = findClientByNick(nickFrom);
    if (!c) return;
    CommandHandler dispatcher(*this);
    std::string line = commandLine;
    if (line.size() < 2 || line.substr(line.size()-2) != "\r\n") line += "\r\n";
    line.erase(line.size()-2);
    dispatcher.handleLine(*c, line);
}

void Server::broadcast(const std::string& chan, const std::string& msg, int except_fd) {
    Channel* c = findChannel(chan);
    if (!c) return;
    const std::set<int>& mem = c->members();
    for (std::set<int>::const_iterator it = mem.begin(); it != mem.end(); ++it) {
        if (*it == except_fd) continue;
        sendToClient(*it, msg);
    }
}

// ---- when a member leaves a channel (PART/QUIT/KICK) ----
void Server::onMemberLeftChannel(Channel* ch, const std::string& lower_key, const std::string& nickJustLeft) {
    (void)lower_key;
    (void)nickJustLeft;
    if (!ch) return;
    // If no operators remain, auto-promote first member
    autoReopIfNone(ch);
    // If empty, delete channel
    maybeDeleteChannel(toLower(ch->name()));
}

void Server::maybeDeleteChannel(const std::string& lower_key) {
    std::map<std::string, Channel*>::iterator it = _channels.find(lower_key);
    if (it == _channels.end()) return;
    Channel* ch = it->second;
    if (ch->members().empty()) {
        delete ch;
        _channels.erase(it);
    }
}

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

void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client* c = it->second;

    // copy lower-case channel keys because we'll mutate structures
    std::vector<std::string> keys;
    for (std::set<std::string>::const_iterator sit = c->channels().begin(); sit != c->channels().end(); ++sit)
        keys.push_back(*sit);

    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        Channel* ch = findChannel(key);
        if (ch) {
            ch->removeMember(fd);
            ch->removeOp(c->nick());
            broadcast(ch->name(), ":" + c->nick() + " QUIT :Client disconnected\r\n", fd);
            onMemberLeftChannel(ch, key, c->nick());
        }
    }

    close(fd);
    for (size_t i = 0; i < _pfds.size(); ++i) if (_pfds[i].fd == fd) _pfds[i].fd = -1;

    delete c;
    _clients.erase(it);
}

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
