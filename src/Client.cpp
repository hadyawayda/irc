#include "Client.hpp"
#include "Server.hpp"

// Construct a client wrapper for an accepted TCP connection. Initially the
// client is not registered (must PASS, NICK, and USER).
Client::Client(int fd)
: _fd(fd), _registered(false), _pass_ok(false) {}

Client::~Client() {}

int Client::fd() const { return _fd; }
const std::string& Client::nick() const { return _nick; }
const std::string& Client::user() const { return _user; }
const std::string& Client::real() const { return _real; }
bool Client::isRegistered() const { return _registered; }
bool Client::passOk() const { return _pass_ok; }

void Client::setPassOk(bool v) { _pass_ok = v; }
void Client::setNick(const std::string& n) { _nick = n; }
void Client::setUser(const std::string& u, const std::string& r) { _user = u; _real = r; }
std::string& Client::inbuf() { return _inbuf; }
std::string& Client::outbuf() { return _outbuf; }

const std::set<std::string>& Client::channels() const { return _channels; }
void Client::joinChannel(const std::string& name) { _channels.insert(name); }
void Client::leaveChannel(const std::string& name) { _channels.erase(name); }

// Attempt to complete registration and send welcome numerics if PASS, NICK,
// and USER were all provided. This is called after any relevant update.
void Client::tryRegister(Server& s) {
    if (!_registered && _pass_ok && !_nick.empty() && !_user.empty()) {
        _registered = true;
        s.sendToClient(_fd, ":ircserv 001 " + _nick + " :Welcome to ft_irc " + _nick + "\r\n");
        s.sendToClient(_fd, ":ircserv NOTICE " + _nick + " :You're registered! Try: JOIN #room\r\n");
    }
}
