// ... existing includes ...
#include <map>
#include <vector>
#include <poll.h>
#include <string>

#include "Bot.hpp"
#include "FileTransfer.hpp"

class Client;
class Channel;
class CommandHandler;
class Bot;
class FileTransfer;

class Server {
    int _listen_fd;
    std::string _password;
    std::string _servername;
    std::map<int, Client*> _clients;
    std::map<std::string, Channel*> _channels;
    std::vector<struct pollfd> _pfds;

    Bot*          _bot;
    FileTransfer* _ft;

public:
    Server(const std::string& port, const std::string& password);
    ~Server();

    void run();

    void sendToClient(int fd, const std::string& msg);
    void broadcast(const std::string& chan, const std::string& msg, int except_fd);
    Channel* getOrCreateChannel(const std::string& name);
    Channel* findChannel(const std::string& name);
    Client* findClientByNick(const std::string& nick);
    const std::string& serverName() const;

    // helper for the Bot to inject server-validated commands (string must end with \r\n)
    void sendServerAs(const std::string& nickFrom, const std::string& commandLine);

private:
    void setupSocket(const std::string& port);
    void addPollfd(int fd, short events);
    void setPollEvents(int fd, short events);
    void removeClient(int fd);
    void handleNewConnection();
    void handleClientRead(int fd);
    void handleClientWrite(int fd);
    void closeAndCleanup();

    friend class Client;
    friend class CommandHandler;
    friend class Bot;
    friend class FileTransfer;
};
