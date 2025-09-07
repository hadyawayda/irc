#ifndef BOT_HPP
# define BOT_HPP

# include <set>
# include <string>

class	Server;
class	Client;

class Bot
{
	Server &_srv;
	std::string _nick;                // shown in all bot messages
	std::set<std::string> _ops_lower; // allow-list (lowercased)
  public:
	Bot(Server &s, const std::string &nick);

	const std::string &nick() const;

	// Called by server when a brand-new channel is created
	void onChannelCreated(const std::string &chan);

	// Called by command handler to let the bot react to user messages
	void onPrivmsg(const Client &from, const std::string &target,
		const std::string &text);

  private:
	static std::string toLower(const std::string &s);
	bool isChannel(const std::string &s) const;
	void say(const std::string &where, const std::string &text);
		// sends PRIVMSG as the bot
};

#endif
