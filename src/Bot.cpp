#include "Bot.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Utils.hpp"

#include <sstream>
#include <cstdlib>
#include <vector>

static void splitChoices(const std::string& s, std::vector<std::string>& out) {
    out.clear();
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '|' ) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else cur += s[i];
    }
    if (!cur.empty()) out.push_back(cur);
    // trim spaces
    for (size_t i = 0; i < out.size(); ++i) {
        while (!out[i].empty() && (out[i][0]==' '||out[i][0]=='\t')) out[i].erase(0,1);
        while (!out[i].empty() && (out[i][out[i].size()-1]==' '||out[i][out[i].size()-1]=='\t')) out[i].erase(out[i].size()-1);
    }
}

Bot::Bot(Server& s, const std::string& nick)
: _srv(s), _nick(nick)
{
    // privileged requesters (lowercase)
    _ops_lower.insert("admin");
    _ops_lower.insert("operator");
    std::srand((unsigned)std::time(NULL));
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

std::string Bot::nowString() const {
    std::time_t t = std::time(NULL);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

void Bot::onChannelCreated(const std::string& chan) {
    // “join” and greet (not added to NAMES)
    _srv.broadcast(chan, ":" + _nick + " JOIN " + chan + "\r\n", -1);
    say(chan, "hey there, I’m " + _nick + " — try !help");
}

void Bot::onPrivmsg(const Client& from, const std::string& target, const std::string& text) {
    if (text.empty()) return;

    // track last seen
    _lastSeen[from.nick()] = std::time(NULL);

    // command parsing
    if (text[0] != '!') {
        // small talk if they greet the bot explicitly
        std::string low = toLower(text);
        if (low.find("hello " + toLower(_nick)) == 0 || low.find("hi " + toLower(_nick)) == 0
            || low == "hello" || low == "hi") {
            std::string where = isChannel(target) ? target : from.nick();
            say(where, "hi " + from.nick() + "! need anything? type !help");
        }
        return;
    }

    std::string where = isChannel(target) ? target : from.nick();
    std::string cmd, arg; size_t sp = text.find(' ');
    if (sp == std::string::npos) cmd = text.substr(1);
    else { cmd = text.substr(1, sp-1); arg = text.substr(sp+1); }

    std::string lcmd = toLower(cmd);

    if (lcmd == "help") {
        say(where, "!ping | !echo <text> | !roll [NdM] | !choose a|b|c | !time | !seen <nick> | !topic <text> | !op <nick> | !kick <nick> [reason]");
    } else if (lcmd == "ping") {
        say(where, "pong");
    } else if (lcmd == "echo") {
        say(where, arg.empty() ? "(nothing to echo)" : arg);
    } else if (lcmd == "roll") {
        int n=1,m=6;
        if (!arg.empty()) {
            // parse NdM
            int tn=0, tm=0; char x='x';
            if (std::sscanf(arg.c_str(), "%d%cx%d", &tn, &x, &tm) == 3 && (x=='d'||x=='D') && tn>0 && tm>0) { n=tn; m=tm; }
        }
        if (n>20) n=20; if (m>1000000) m=1000000;
        long total=0;
        std::ostringstream os; os << "rolled:";
        for (int i=0;i<n;++i){ int r= (std::rand()%m)+1; total+=r; os<<" "<<r; }
        os<<" (sum="<<total<<")";
        say(where, os.str());
    } else if (lcmd == "choose") {
        std::vector<std::string> choices; splitChoices(arg, choices);
        if (choices.empty()) say(where, "usage: !choose a|b|c");
        else {
            int idx = std::rand()%choices.size();
            say(where, "I pick: " + choices[idx]);
        }
    } else if (lcmd == "time") {
        say(where, "server time is " + nowString());
    } else if (lcmd == "seen") {
        std::string who = arg;
        if (who.empty()) say(where, "usage: !seen <nick>");
        else {
            std::map<std::string,std::time_t>::const_iterator it = _lastSeen.find(who);
            if (it == _lastSeen.end()) say(where, "I haven’t seen " + who + " yet.");
            else {
                char buf[64];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&it->second));
                say(where, who + " was last seen at " + std::string(buf));
            }
        }
    } else if (lcmd == "topic") {
        if (!isChannel(where)) { say(where, "Use in a channel."); return; }
        if (arg.empty()) { say(where, "Usage: !topic <new topic>"); return; }
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
