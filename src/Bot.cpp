#include "Bot.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Utils.hpp"

#include <sstream>
#include <cstdlib>
#include <ctime>

static int rng() { static bool s=false; if(!s){ std::srand((unsigned)std::time(0)); s=true; } return std::rand(); }

Bot::Bot(Server& s, const std::string& nick)
: _srv(s), _nick(nick)
{
    // privileged bot requesters (lowercased)
    _ops_lower.insert("admin");
    _ops_lower.insert("operator");
}

const std::string& Bot::nick() const { return _nick; }

std::string Bot::toLower(const std::string& s) {
    std::string o(s);
    for (size_t i=0;i<o.size();++i) { char c=o[i]; if (c>='A'&&c<='Z') o[i]=char(c-'A'+'a'); }
    return o;
}

bool Bot::isChannel(const std::string& s) const {
    return !s.empty() && (s[0] == '#' || s[0] == '&');
}

void Bot::say(const std::string& where, const std::string& text) {
    _srv.broadcast(where, ":" + _nick + " PRIVMSG " + where + " :" + text + "\r\n", -1);
}

void Bot::onChannelCreated(const std::string& chan) {
    // Bot "joins" and drops a quick hint; this often fires before first member joins.
    _srv.broadcast(chan, ":" + _nick + " JOIN " + chan + "\r\n", -1);
    say(chan, "hi, I'm " + _nick + " — try !help");
}

void Bot::onPrivmsg(const Client& from, const std::string& target, const std::string& text) {
    if (text.empty()) return;

    // casual small talk if bot is mentioned by name without a '!' command
    if (text[0] != '!') {
        std::string low = toLower(text);
        if (low.find(toLower(_nick)) != std::string::npos) {
            smallTalk(isChannel(target) ? target : from.nick(), from.nick(), text);
        }
        return;
    }
    // parse command and arg
    std::string where = isChannel(target) ? target : from.nick();
    std::string cmd, arg; size_t sp = text.find(' ');
    if (sp == std::string::npos) cmd = text.substr(1);
    else { cmd = text.substr(1, sp-1); arg = text.substr(sp+1); }

    std::string lcmd = toLower(cmd);

    if (lcmd == "help")            doHelp(where, from.nick(), arg);
    else if (lcmd == "about")      doAbout(where);
    else if (lcmd == "ping")       doPing(where);
    else if (lcmd == "echo")       doEcho(where, arg);
    else if (lcmd == "topic")      doTopic(from, where, arg);
    else if (lcmd == "op")         doOp(from, where, arg, true);
    else if (lcmd == "deop")       doOp(from, where, arg, false);
    else if (lcmd == "kick")       doKick(from, where, arg);
    else if (lcmd == "who")        doWho(where);
    else if (lcmd == "modes")      doModes(from, where);
    else if (lcmd == "roll")       doRoll(where, arg);
    else if (lcmd == "8ball")      do8ball(where, arg);
    else                           say(where, "unknown command '" + cmd + "'. Try !help");
}

// ----- commands -----

void Bot::doHelp(const std::string& where, const std::string& who, const std::string& arg) {
    if (arg.empty()) {
        say(where, "!help [cmd] | !about | !ping | !echo <text> | !who | !modes | !roll [XdY] | !8ball <q> | !topic <text> | !op <nick> | !deop <nick> | !kick <nick> [reason]");
        return;
    }
    std::string l = toLower(arg);
    if (l == "roll")      say(where, "Usage: !roll [XdY] — rolls X dice of Y sides (default 1d6).");
    else if (l == "8ball")say(where, "Usage: !8ball <question> — reveals the truth ⭐.");
    else if (l == "modes")say(where, "Usage: !modes — shows current channel modes (i/t/k/l).");
    else if (l == "who")  say(where, "Usage: !who — lists users in the channel, '@' marks operators.");
    else if (l == "topic")say(where, "Usage: !topic <text> — requests a topic change (server enforces +t).");
    else if (l == "op")   say(where, "Usage: !op <nick> — requests +o (server enforces perms).");
    else if (l == "deop") say(where, "Usage: !deop <nick> — requests -o (server enforces perms).");
    else if (l == "kick") say(where, "Usage: !kick <nick> [reason] — requests a kick (server enforces perms).");
    else if (l == "echo") say(where, "Usage: !echo <text> — repeats <text>.");
    else if (l == "ping") say(where, "Usage: !ping — pong.");
    else                  say(where, "No help for '" + arg + "'. Try !help.");
}

void Bot::doAbout(const std::string& where) {
    say(where, "I'm a friendly helper bot: ops can !op/!deop/!kick, everyone can !topic/!who/!modes/!roll/!8ball/!echo. Say my name and 'hi' and I'll greet you.");
}

void Bot::doEcho(const std::string& where, const std::string& arg) {
    say(where, arg.empty() ? "(nothing to echo)" : arg);
}

void Bot::doPing(const std::string& where) { say(where, "pong"); }

void Bot::doTopic(const Client& from, const std::string& where, const std::string& arg) {
    if (!isChannel(where)) { say(where, "Use in a channel."); return; }
    if (arg.empty()) { say(where, "Usage: !topic <new topic>"); return; }
    _srv.sendServerAs(from.nick(), "TOPIC " + where + " :" + arg + "\r\n");
}

void Bot::doOp(const Client& from, const std::string& where, const std::string& arg, bool op) {
    if (!isChannel(where)) { say(where, "Use in a channel."); return; }
    if (arg.empty()) { say(where, std::string("Usage: !") + (op ? "op" : "deop") + " <nick>"); return; }
    if (_ops_lower.find(toLower(from.nick())) == _ops_lower.end()) { say(where, from.nick() + ": not authorized."); return; }
    _srv.sendServerAs(from.nick(), std::string("MODE ") + where + (op ? " +o " : " -o ") + arg + "\r\n");
}

void Bot::doKick(const Client& from, const std::string& where, const std::string& arg) {
    if (!isChannel(where)) { say(where, "Use in a channel."); return; }
    if (_ops_lower.find(toLower(from.nick())) == _ops_lower.end()) { say(where, from.nick() + ": not authorized."); return; }
    std::string victim = arg; std::string reason;
    size_t sp2 = arg.find(' ');
    if (sp2 != std::string::npos) { victim = arg.substr(0, sp2); reason = arg.substr(sp2+1); }
    if (victim.empty()) { say(where, "Usage: !kick <nick> [reason]"); return; }
    if (reason.empty()) _srv.sendServerAs(from.nick(), "KICK " + where + " " + victim + "\r\n");
    else _srv.sendServerAs(from.nick(), "KICK " + where + " " + victim + " :" + reason + "\r\n");
}

void Bot::doWho(const std::string& where) {
    if (!isChannel(where)) { say(where, "Use in a channel."); return; }
    Channel* ch = _srv.findChannel(where);
    if (!ch) { say(where, "No such channel."); return; }
    std::string names;
    const std::set<int>& mem = ch->members();
    for (std::set<int>::const_iterator it = mem.begin(); it != mem.end(); ++it) {
        Client* m = _srv._clients[*it];
        if (!names.empty()) names += " ";
        if (ch->isOp(m->nick())) names += "@";
        names += m->nick();
    }
    say(where, names.empty() ? "(nobody here?)" : names);
}

void Bot::doModes(const Client& from, const std::string& where) {
    if (!isChannel(where)) { say(where, "Use in a channel."); return; }
    _srv.sendServerAs(from.nick(), "MODE " + where + "\r\n"); // server will 324 back to the user
}

void Bot::doRoll(const std::string& where, const std::string& arg) {
    int X = 1, Y = 6;
    if (!arg.empty()) {
        int dpos = -1;
        for (size_t i=0;i<arg.size();++i) if (arg[i]=='d' || arg[i]=='D'){ dpos=(int)i; break; }
        if (dpos == -1) Y = std::atoi(arg.c_str());
        else { X = std::atoi(arg.substr(0,dpos).c_str()); Y = std::atoi(arg.substr(dpos+1).c_str()); }
        if (X <= 0) X = 1; if (Y <= 1) Y = 6;
    }
    long sum = 0; std::ostringstream out; out << "roll " << X << "d" << Y << ":";
    for (int i=0;i<X && i<20; ++i) { int r = (rng() % Y) + 1; sum += r; out << " " << r; }
    if (X > 1) out << " (sum " << sum << ")";
    say(where, out.str());
}

void Bot::do8ball(const std::string& where, const std::string& arg) {
    static const char* ans[] = {
        "It is certain.", "It is decidedly so.", "Without a doubt.", "Yes — definitely.",
        "You may rely on it.", "As I see it, yes.", "Most likely.", "Outlook good.",
        "Signs point to yes.", "Yes.", "Reply hazy, try again.", "Ask again later.",
        "Better not tell you now.", "Cannot predict now.", "Concentrate and ask again.",
        "Don't count on it.", "My reply is no.", "My sources say no.", "Outlook not so good.",
        "Very doubtful."
    };
    (void)arg;
    say(where, ans[rng() % (sizeof(ans)/sizeof(ans[0]))]);
}

void Bot::smallTalk(const std::string& where, const std::string& who, const std::string& text) {
    std::string low = toLower(text);
    if (low.find("hello") != std::string::npos || low.find("hi") != std::string::npos || low.find("hey") != std::string::npos) {
        say(where, "hey " + who + " 👋  (try !help)");
    } else if (low.find("thanks") != std::string::npos || low.find("thank you") != std::string::npos) {
        say(where, "anytime, " + who + "!");
    }
}
