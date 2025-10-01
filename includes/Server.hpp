#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <set>
#include <vector>

struct pollfd;

class Client;
class Channel;
class Bot;
class FileTransfer;

class Server {
public:
    Server(const std::string& port, const std::string& password);
    ~Server();

    const std::string& serverName() const;

    void run();

    void sendToClient(int fd, const std::string& msg);
    void broadcast(const std::string& chan, const std::string& msg, int except_fd);

    Channel* getOrCreateChannel(const std::string& name);
    Channel* findChannel(const std::string& name);
    Client*  findClientByNick(const std::string& nick);

    void sendServerAs(const std::string& nickFrom, const std::string& commandLine);

    // Exposed (your CommandHandler already accesses these directly)
    Bot*          _bot;
    FileTransfer* _ft;

    // Also accessed by handlers (for NAMES build)
    std::map<int, Client*> _clients;

    // Password (read by CommandHandler::cmdPASS)
    std::string _password;

private:
    int _listen_fd;
    std::string _servername;

    std::vector<struct pollfd> _pfds;
    std::map<std::string, Channel*> _channels; // key = lowercased name

private:
    void setupSocket(const std::string& port);
    void addPollfd(int fd, short events);
    void setPollEvents(int fd, short events);

    void handleNewConnection();
    void handleClientWrite(int fd);
    void handleClientRead(int fd);

    void removeClient(int fd);
    void closeAndCleanup();

    // NEW helpers
public:
    void maybeDeleteChannel(const std::string& name);
    void promoteOpIfNone(Channel& ch);
};

#endif // SERVER_HPP
