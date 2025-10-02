#ifndef BOT_HPP
#define BOT_HPP

#include <string>
#include <set>

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
    Server& _srv;
    std::string _nick;
    std::set<std::string> _ops_lower;

    // low helpers
    static std::string toLower(const std::string& s);
    bool isChannel(const std::string& s) const;
    void say(const std::string& where, const std::string& text);

    // commands
    void doHelp(const std::string& where, const std::string& who, const std::string& arg);
    void doEcho(const std::string& where, const std::string& arg);
    void doPing(const std::string& where);
    void doTopic(const Client& from, const std::string& where, const std::string& arg);
    void doOp(const Client& from, const std::string& where, const std::string& arg, bool op);
    void doKick(const Client& from, const std::string& where, const std::string& arg);
    void doWho(const std::string& where);
    void doModes(const Client& from, const std::string& where);
    void doRoll(const std::string& where, const std::string& arg);
    void do8ball(const std::string& where, const std::string& arg);
    void doAbout(const std::string& where);
    void smallTalk(const std::string& where, const std::string& who, const std::string& text);
};

#endif // BOT_HPP
