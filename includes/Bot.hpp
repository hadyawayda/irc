#ifndef BOT_HPP
#define BOT_HPP

#include <string>
#include <set>
#include <map>
#include <vector>
#include <ctime>

class Server;
class Client;

class Bot {
public:
    Bot(Server& s, const std::string& nick);

    const std::string& nick() const;

    // Called by Server when a new channel structure is created.
    void onChannelCreated(const std::string& chan);

    // Called by CommandHandler whenever a PRIVMSG is sent (channel or PM).
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
    static std::string toLower(const std::string& s);
    bool isChannel(const std::string& s) const;
    void say(const std::string& where, const std::string& text);
    void smallTalk(const std::string& where, const std::string& who, const std::string& text);
    void checkReminders();
    static long rngi(long maxExclusive);

    // ---- commands (non-admin, useful) ----
    void doHelp(const std::string& where, const std::string& who, const std::string& arg);
    void doAbout(const std::string& where);
    void doEcho(const std::string& where, const std::string& arg);
    void doPing(const std::string& where);
    void doWho(const std::string& where);
    void doModes(const Client& from, const std::string& where);
    void doRoll(const std::string& where, const std::string& arg);
    void do8ball(const std::string& where, const std::string& arg);
    void doCommands(const std::string& where);
    void doChoose(const std::string& where, const std::string& arg);
    void doSeen(const std::string& where, const std::string& arg);
    void doRemind(const std::string& where, const std::string& who, const std::string& arg);
    void doPoll(const std::string& where, const std::string& who, const std::string& arg);
    void doCalc(const std::string& where, const std::string& arg);
    void doUptime(const std::string& where);

    // parsing helpers
    static bool parseDuration(const std::string& s, long& secondsOut);       // "2h30m10s", "45m", "10s", "1h"
    static std::string formatDuration(long secs);
    static bool splitByBar(const std::string& s, std::vector<std::string>& parts); // "a | b | c"
};

#endif // BOT_HPP
