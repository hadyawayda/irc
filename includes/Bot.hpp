#ifndef BOT_HPP
# define BOT_HPP

<<<<<<< Updated upstream
# include <set>
# include <string>
=======
/**
 * @file Bot.hpp
 * @brief Lightweight helper bot that reacts to PRIVMSGs and channel events.
 *
 * The Bot is optional and lives inside the server process. It listens to
 * PRIVMSG traffic via CommandHandler hooks and provides small utilities such
 * as help, ping, reminders, polls, dice roll, eight-ball, and uptime.
 *
 * Implementation favors straightforward STL containers to keep the project
 * portable and C++98-friendly.
 */

#include <string>
#include <set>
#include <map>
#include <vector>
#include <ctime>
>>>>>>> Stashed changes

class	Server;
class	Client;

<<<<<<< Updated upstream
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
=======
class Bot {
public:
    /**
     * @brief Create a bot bound to a server with a fixed nickname.
     * @param s    The hosting Server instance.
     * @param nick Nickname the bot will use for messages.
     */
    Bot(Server& s, const std::string& nick);

    /** @return Bot nickname. */
    const std::string& nick() const;

    // Called by Server when a new channel structure is created.
    /**
     * @brief Hook invoked when the Server creates a new Channel object.
     * The bot may auto-greet or schedule tasks for that channel.
     */
    void onChannelCreated(const std::string& chan);

    // Called by CommandHandler whenever a PRIVMSG is sent (channel or PM).
    /**
     * @brief Process a PRIVMSG directed to a channel or the bot directly.
     *
     * Parses commands prefixed with the bot's nick or a known keyword, and
     * updates internal state accordingly (e.g., reminders/polls).
     */
    void onPrivmsg(const Client& from, const std::string& target, const std::string& text);

private:
    Server&      _srv;
    std::string  _nick;

    // ---- runtime state for richer features ----
    std::time_t  _startedAt;

    struct Reminder {
        std::string where;  // channel or pm target
        std::string who;    // requester nick
        std::string text;   // message
        std::time_t due;    // due time (epoch)
    };
    std::vector<Reminder> _reminders;

    struct Poll {
        int                      id;
        std::string              channelKey; // lower-case channel key
        std::string              question;
        std::vector<std::string> options;    // 1-based display
        std::map<std::string,int> votes;     // voterNick(lower) -> option index (1..N)
        bool                     open;
        std::time_t              createdAt;
    };
    std::map<int, Poll> _polls;
    int _nextPollId;

    // last seen per channel: lower(channel) -> lower(nick) -> last epoch
    std::map<std::string, std::map<std::string, std::time_t> > _lastSeen;

    // ---- helpers ----
    /** Lower-case utility (ASCII). */
    static std::string toLower(const std::string& s);
    /** @return true if s looks like a channel (e.g., starts with '#'). */
    bool isChannel(const std::string& s) const;
    /** Send a plain text line to a channel or user target. */
    void say(const std::string& where, const std::string& text);
    /** Quick responses to casual phrases to make the bot feel alive. */
    void smallTalk(const std::string& where, const std::string& who, const std::string& text);
    /** Check reminders and deliver any that are due; called periodically. */
    void checkReminders();
    /** Small, deterministic RNG wrapper for bounded integer in [0,max). */
    static long rngi(long maxExclusive);

    // ---- commands (non-admin, useful) ----
    /** Show help for available bot commands. */
    void doHelp(const std::string& where, const std::string& who, const std::string& arg);
    /** Show version/about information. */
    void doAbout(const std::string& where);
    /** Repeat back the provided argument. */
    void doEcho(const std::string& where, const std::string& arg);
    /** Respond with 'pong' and latency if applicable. */
    void doPing(const std::string& where);
    /** List visible users in the channel (if supported). */
    void doWho(const std::string& where);
    /** Show channel modes (+i, +t, +k, +l) in a friendly way. */
    void doModes(const Client& from, const std::string& where);
    /** Roll dice like '2d6+1' or a simple number for [0..n). */
    void doRoll(const std::string& where, const std::string& arg);
    /** Magic eight-ball style random response. */
    void do8ball(const std::string& where, const std::string& arg);
    /** Compact list of supported commands. */
    void doCommands(const std::string& where);
    /** Choose randomly among options separated by '|'. */
    void doChoose(const std::string& where, const std::string& arg);
    /** Report when a nick was last seen in the channel. */
    void doSeen(const std::string& where, const std::string& arg);
    /** Schedule a reminder after a given duration to post a message. */
    void doRemind(const std::string& where, const std::string& who, const std::string& arg);
    /** Create/manage a simple poll with multiple options. */
    void doPoll(const std::string& where, const std::string& who, const std::string& arg);
    /** Evaluate a simple arithmetic expression (safe subset). */
    void doCalc(const std::string& where, const std::string& arg);
    /** Report bot uptime since server start. */
    void doUptime(const std::string& where);

    // parsing helpers
    /**
     * @brief Parse a human-friendly duration like "2h30m10s", "45m", "10s", or "1h".
     * @param s          Input string
     * @param secondsOut Output seconds on success
     * @return true on success; false if the format is invalid
     */
    static bool parseDuration(const std::string& s, long& secondsOut);       // "2h30m10s", "45m", "10s", "1h"
    /** Format a number of seconds as a compact human duration. */
    static std::string formatDuration(long secs);
    /** Split a string into parts around '|', trimming spaces. */
    static bool splitByBar(const std::string& s, std::vector<std::string>& parts); // "a | b | c"
>>>>>>> Stashed changes
};

#endif
