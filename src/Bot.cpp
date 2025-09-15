#include "Bot.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Utils.hpp"   // toLower already exists if you prefer to use that

#include <sstream>
#include <cstdlib>

Bot::Bot(Server& s, const std::string& nick)
: _srv(s), _nick(nick)
{
    // add your own nick(s) here to allow privileged bot actions
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
    // broadcast as if the bot sent a PRIVMSG
    _srv.broadcast(where, ":" + _nick + " PRIVMSG " + where + " :" + text + "\r\n", -1);
}

void Bot::onChannelCreated(const std::string& chan) {
    // pretend the bot joined and post a short help; we do not add the bot to NAMES
    _srv.broadcast(chan, ":" + _nick + " JOIN " + chan + "\r\n", -1);
    say(chan, "hi, I'm " + _nick + " — try !help");
}

void Bot::onPrivmsg(const Client& from, const std::string& target, const std::string& text) {
    if (text.empty() || text[0] != '!') return;

    std::string where = isChannel(target) ? target : from.nick();
    std::string cmd, arg; size_t sp = text.find(' ');
    if (sp == std::string::npos) cmd = text.substr(1);
    else { cmd = text.substr(1, sp-1); arg = text.substr(sp+1); }

    std::string lcmd = toLower(cmd);

    if (lcmd == "help") {
        say(where, "!ping | !echo <text> | !topic <text> | !op <nick> | !kick <nick> [reason]");
    } else if (lcmd == "ping") {
        say(where, "pong");
    } else if (lcmd == "echo") {
        say(where, arg.empty() ? "(nothing to echo)" : arg);
    } else if (lcmd == "topic") {
        if (!isChannel(where)) { say(where, "Use in a channel."); return; }
        if (arg.empty()) { say(where, "Usage: !topic <new topic>"); return; }
        // let the server enforce +t/+o
        _srv.sendServerAs(from.nick(), "TOPIC " + where + " :" + arg + "\r\n");
    } else if (lcmd == "op") {
        if (!isChannel(where)) { say(where, "Use in a channel."); return; }
        if (arg.empty()) { say(where, "Usage: !op <nick>"); return; }
        if (_ops_lower.find(toLower(from.nick())) == _ops_lower.end()) { say(where, from.nick() + ": not authorized."); return; }
        _srv.sendServerAs(from.nick(), "MODE " + where + " +o " + arg + "\r\n");
    } else if (lcmd == "kick") {
        if (!isChannel(where)) { say(where, "Use in a channel."); return; }
        if (_ops_lower.find(toLower(from.nick())) == _ops_lower.end()) { say(where, from.nick() + ": not authorized."); return; }
        std::string victim = arg; std::string reason;
        size_t sp2 = arg.find(' ');
        if (sp2 != std::string::npos) { victim = arg.substr(0, sp2); reason = arg.substr(sp2+1); }
        if (victim.empty()) { say(where, "Usage: !kick <nick> [reason]"); return; }
        if (reason.empty()) _srv.sendServerAs(from.nick(), "KICK " + where + " " + victim + "\r\n");
        else _srv.sendServerAs(from.nick(), "KICK " + where + " " + victim + " :" + reason + "\r\n");
    }
}
