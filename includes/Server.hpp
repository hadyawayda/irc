#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <set>
#include <vector>
#include <poll.h>

class Client;
class Channel;
class CommandHandler;
class Bot;
class FileTransfer;

class Server {
public:
    Server(const std::string& port, const std::string& password);
    ~Server();

    const std::string& serverName() const;

    void sendToClient(int fd, const std::string& msg);
    void broadcast(const std::string& chan, const std::string& msg, int except_fd);
    void sendServerAs(const std::string& nickFrom, const std::string& commandLine);

    Channel* getOrCreateChannel(const std::string& name);
    Channel* findChannel(const std::string& name);
    Client*  findClientByNick(const std::string& nick);

    void run();

    // Helpers used by commands / cleanup
    void removeClient(int fd);

    // ---- new helpers for features/fixes ----
    void maybeDeleteChannel(const std::string& lower_key);
    void autoReopIfNone(Channel* ch);
    void onMemberLeftChannel(Channel* ch, const std::string& lower_key, const std::string& nickJustLeft);

    // Exposed state for bot/ft (kept simple for this project)
    std::map<int, Client*>              _clients;
    std::map<std::string, Channel*>     _channels;
    std::string                         _password;
    std::string                         _servername;
    Bot*                                _bot;
    FileTransfer*                       _ft;

private:
    void setupSocket(const std::string& port);
    void addPollfd(int fd, short events);
    void setPollEvents(int fd, short events);

    void handleNewConnection();
    void handleClientRead(int fd);
    void handleClientWrite(int fd);
    void closeAndCleanup();

    int _listen_fd;
    std::vector<struct pollfd> _pfds;
};

#endif
