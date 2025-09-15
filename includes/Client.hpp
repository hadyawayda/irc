#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <set>

class Server;

class Client {
    int _fd;
    bool _registered;
    bool _pass_ok;
    std::string _nick, _user, _real;
    std::string _inbuf, _outbuf;
    std::set<std::string> _channels; // lowercased names

public:
    Client(int fd);
    ~Client();

    int fd() const;
    const std::string& nick() const;
    const std::string& user() const;
    const std::string& real() const;
    bool isRegistered() const;
    bool passOk() const;

    void setPassOk(bool v);
    void setNick(const std::string& n);
    void setUser(const std::string& u, const std::string& r);
    void tryRegister(Server& s);

    std::string& inbuf();
    std::string& outbuf();

    const std::set<std::string>& channels() const;
    void joinChannel(const std::string& name);
    void leaveChannel(const std::string& name);
};

#endif
