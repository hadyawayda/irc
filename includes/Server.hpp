#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include <string>
#include <poll.h>

class Client;
class Channel;
class Bot;
class FileTransfer;

class Server {
public:
    Server(const std::string& port, const std::string& password);
    ~Server();

    const std::string& serverName() const;

    void sendToClient(int fd, const std::string& msg);
    void broadcast(const std::string& chan, const std::string& msg, int except_fd);

    Channel* getOrCreateChannel(const std::string& name);
    Channel* findChannel(const std::string& name);
    Client*  findClientByNick(const std::string& nick);

    void sendServerAs(const std::string& nickFrom, const std::string& commandLine);

    // NEW
    void ensureOpIfNone(Channel* ch);
    void maybeDeleteChannelIfEmpty(Channel* ch);

    void run();

    // public state used by other components
    std::map<int, Client*>             _clients;
    std::map<std::string, Channel*>    _channels;
    std::string                        _password;
    std::string                        _servername;

    Bot*           _bot;
    FileTransfer*  _ft;

private:
    int _listen_fd;
    std::vector<struct pollfd> _pfds;

    void setupSocket(const std::string& port);
    void closeAndCleanup();
    void addPollfd(int fd, short events);
    void setPollEvents(int fd, short events);

    void handleNewConnection();
    void handleClientRead(int fd);
    void handleClientWrite(int fd);
    void removeClient(int fd);
};

#endif // SERVER_HPP
