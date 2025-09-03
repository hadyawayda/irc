#ifndef SERVER_HPP
# define SERVER_HPP

# include <map>
# include <poll.h>
# include <string>
# include <vector>

class	Client;
class	Channel;
class	CommandHandler;

class Server
{
	int _listen_fd;
	std::string _password;
	std::string _servername;
	std::map<int, Client *> _clients;           // by fd
	std::map<std::string, Channel *> _channels; // by lowercased name
	std::vector<struct pollfd> _pfds;

  public:
	Server(const std::string &port, const std::string &password);
	~Server();

	void run();

	// helpers used by components
	void sendToClient(int fd, const std::string &msg);
	void broadcast(const std::string &chan, const std::string &msg,
		int except_fd);
	Channel *getOrCreateChannel(const std::string &name);
	Channel *findChannel(const std::string &name);
	Client *findClientByNick(const std::string &nick);
	const std::string &serverName() const;

  private:
	void setupSocket(const std::string &port);
	void addPollfd(int fd, short events);
	void setPollEvents(int fd, short events);
	void removeClient(int fd);
	void handleNewConnection();
	void handleClientRead(int fd);
	void handleClientWrite(int fd);
	void closeAndCleanup();

	friend class Client;
	friend class CommandHandler;
};

#endif
