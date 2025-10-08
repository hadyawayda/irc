#include "Bot.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Utils.hpp"

#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cctype>

// -------- RNG --------
static bool s_rng_init = false;
static void rng_seed() { if (!s_rng_init) { std::srand((unsigned)std::time(0)); s_rng_init = true; } }
long Bot::rngi(long maxExclusive) { rng_seed(); if (maxExclusive <= 1) return 0; return std::rand() % maxExclusive; }

// -------- tiny utils --------
std::string Bot::toLower(const std::string& s) {
    std::string o(s);
    for (size_t i=0;i<o.size();++i) {
        char c=o[i];
        if (c>='A'&&c<='Z') o[i]=char(c-'A'+'a');
    }
    return o;
}
bool Bot::isChannel(const std::string& s) const { return !s.empty() && (s[0]=='#' || s[0]=='&'); }
void Bot::say(const std::string& where, const std::string& text) {
    _srv.broadcast(where, ":" + _nick + " PRIVMSG " + where + " :" + text + "\r\n", -1);
}
static std::string trim(const std::string& s) {
    size_t a=0, b=s.size();
    while (a<b && std::isspace((unsigned char)s[a])) ++a;
    while (b>a && std::isspace((unsigned char)s[b-1])) --b;
    return s.substr(a,b-a);
}

// -------- ctor / identity --------
Bot::Bot(Server& s, const std::string& nick)
: _srv(s), _nick(nick), _startedAt(std::time(0)), _nextPollId(1)
{}
const std::string& Bot::nick() const { return _nick; }

// -------- hooks --------
void Bot::onChannelCreated(const std::string& chan) {
    _srv.broadcast(chan, ":" + _nick + " JOIN " + chan + "\r\n", -1);
    say(chan, "hey! I’m " + _nick + " — type !commands or !help");
}

void Bot::checkReminders() {
    if (_reminders.empty()) return;
    const std::time_t now = std::time(0);
    std::vector<Reminder> keep;
    for (size_t i=0;i<_reminders.size();++i) {
        const Reminder& r = _reminders[i];
        if (r.due <= now) {
            say(r.where, r.who + ": ⏰ reminder — " + r.text);
        } else {
            keep.push_back(r);
        }
    }
    _reminders.swap(keep);
}

void Bot::smallTalk(const std::string& where, const std::string& who, const std::string& text) {
    std::string low = toLower(text);
    if (low.find("hello") != std::string::npos || low.find("hi") != std::string::npos || low.find("hey") != std::string::npos) {
        say(where, "hey " + who + " 👋  (try !commands)");
    } else if (low.find("thanks") != std::string::npos || low.find("thank you") != std::string::npos) {
        say(where, "anytime, " + who + "!");
    }
}

void Bot::onPrivmsg(const Client& from, const std::string& target, const std::string& text) {
    // deliver due reminders opportunistically
    checkReminders();

    // update 'seen' info (only for channel messages)
    if (isChannel(target)) {
        std::string chkey = toLower(target);
        std::string who   = toLower(from.nick());
        _lastSeen[chkey][who] = std::time(0);
    }

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
    const std::string where = isChannel(target) ? target : from.nick();
    std::string cmd, arg; size_t sp = text.find(' ');
    if (sp == std::string::npos) cmd = text.substr(1);
    else { cmd = text.substr(1, sp-1); arg = text.substr(sp+1); }

    const std::string lcmd = toLower(cmd);

    // non-admin, useful commands
    if      (lcmd == "help")      doHelp(where, from.nick(), arg);
    else if (lcmd == "commands")  doCommands(where);
    else if (lcmd == "about")     doAbout(where);
    else if (lcmd == "ping")      doPing(where);
    else if (lcmd == "echo")      doEcho(where, arg);
    else if (lcmd == "who")       doWho(where);
    else if (lcmd == "modes")     doModes(from, where);
    else if (lcmd == "roll")      doRoll(where, arg);
    else if (lcmd == "8ball")     do8ball(where, arg);
    else if (lcmd == "choose")    doChoose(where, arg);
    else if (lcmd == "seen")      doSeen(where, arg);
    else if (lcmd == "remind")    doRemind(where, from.nick(), arg);
    else if (lcmd == "poll")      doPoll(where, from.nick(), arg);
    else if (lcmd == "calc")      doCalc(where, arg);
    else if (lcmd == "uptime")    doUptime(where);
    else                          say(where, "unknown command '" + cmd + "'. Try !commands");
}

// -------- commands impl --------
void Bot::doCommands(const std::string& where) {
    say(where,
        "commands: !help [topic] | !about | !ping | !echo <text> | !who | !modes "
        "| !roll [XdY] | !8ball <q> | !choose a | b | c | !seen <nick> "
        "| !remind <in> <msg> | !poll <new|vote|show|close> ... | !calc <expr> | !uptime");
}

void Bot::doHelp(const std::string& where, const std::string& who, const std::string& arg) {
    (void)who;
    if (arg.empty()) {
        doCommands(where);
        say(where, "Try !help <topic>. Topics: help, about, ping, echo, who, modes, roll, 8ball, choose, seen, remind, poll, calc, uptime, mode, invite, topic");
        return;
    }
    const std::string l = toLower(arg);

    if (l == "modes" || l == "mode") {
        say(where, "modes: +i invite-only, +t topic-by-ops, +k <key>, +l <limit>.");
        say(where, "examples: /MODE #room +t | /MODE #room +k hunter2 | /MODE #room -k | /MODE #room +l 42");
        say(where, "tip: !modes asks the server to show current modes (324 reply).");
    } else if (l == "invite") {
        say(where, "invite: ops can /INVITE <nick> <#room>. If +i is set, the user can then /JOIN.");
    } else if (l == "topic") {
        say(where, "topic: /TOPIC #room :New topic. If +t, only ops may change it.");
    } else if (l == "help") say(where, "Usage: !help [topic] — show docs for a topic or command.");
    else if (l == "about")  say(where, "Usage: !about — what I can do.");
    else if (l == "ping")   say(where, "Usage: !ping — reply with pong.");
    else if (l == "echo")   say(where, "Usage: !echo <text> — repeat text.");
    else if (l == "who")    say(where, "Usage: !who — list nicks in this channel; ops prefixed with @.");
    else if (l == "roll")   say(where, "Usage: !roll [XdY] — roll X dice of Y sides (default 1d6). Example: !roll 3d20");
    else if (l == "8ball")  say(where, "Usage: !8ball <question> — mystical yes/no.");
    else if (l == "choose") say(where, "Usage: !choose option1 | option2 | option3 — pick one at random.");
    else if (l == "seen")   say(where, "Usage: !seen <nick> — when nick was last seen speaking in this channel.");
    else if (l == "remind") say(where, "Usage: !remind <in> <msg>. <in> like 10s, 5m, 2h30m, 1d2h. Ex: !remind 15m stand up");
    else if (l == "poll")   say(where, "Usage: !poll new Question | Yes | No; !poll vote <id> <n>; !poll show <id>; !poll close <id>");
    else if (l == "calc")   say(where, "Usage: !calc <expr>. Supports + - * / and parentheses on integers. Ex: !calc (2+3)*4");
    else if (l == "uptime") say(where, "Usage: !uptime — bot uptime.");
    else say(where, "No help for '" + arg + "'. Try !commands");
}

void Bot::doAbout(const std::string& where) {
    say(where, "I’m a chat helper: reminders, polls, seen, choose, calc, dice, 8ball, who, modes docs. Type !commands.");
}

void Bot::doEcho(const std::string& where, const std::string& arg) {
    say(where, arg.empty() ? "(nothing to echo)" : arg);
}

void Bot::doPing(const std::string& where) { say(where, "pong"); }

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
    _srv.sendServerAs(from.nick(), "MODE " + where + "\r\n"); // server will reply 324 to the user
}

void Bot::doRoll(const std::string& where, const std::string& arg) {
    int X = 1, Y = 6;
    if (!arg.empty()) {
        int dpos = -1;
        for (size_t i=0;i<arg.size();++i) if (arg[i]=='d' || arg[i]=='D'){ dpos=(int)i; break; }
        if (dpos == -1) Y = std::atoi(arg.c_str());
        else {
            std::string sx = arg.substr(0,dpos);
            std::string sy = arg.substr(dpos+1);
            X = std::atoi(sx.c_str()); Y = std::atoi(sy.c_str());
        }
        if (X <= 0) X = 1;
        if (Y <= 1) Y = 6;
    }
    long sum = 0; std::ostringstream out; out << "roll " << X << "d" << Y << ":";
    int limit = X < 50 ? X : 50; // avoid spam
    for (int i=0;i<limit; ++i) { int r = (int)rngi(Y) + 1; sum += r; out << " " << r; }
    if (X > 1) out << " (sum " << sum << ")";
    if (X > limit) out << " .. +" << (X-limit) << " more";
    say(where, out.str());
}

void Bot::do8ball(const std::string& where, const std::string& arg) {
    (void)arg;
    static const char* ans[] = {
        "It is certain.", "It is decidedly so.", "Without a doubt.", "Yes — definitely.",
        "You may rely on it.", "As I see it, yes.", "Most likely.", "Outlook good.",
        "Signs point to yes.", "Yes.", "Reply hazy, try again.", "Ask again later.",
        "Better not tell you now.", "Cannot predict now.", "Concentrate and ask again.",
        "Don't count on it.", "My reply is no.", "My sources say no.", "Outlook not so good.",
        "Very doubtful."
    };
    say(where, ans[rngi((long)(sizeof(ans)/sizeof(ans[0])))]);
}

bool Bot::splitByBar(const std::string& s, std::vector<std::string>& parts) {
    parts.clear();
    std::string cur;
    for (size_t i=0;i<s.size();++i) {
        if (s[i]=='|') { parts.push_back(trim(cur)); cur.clear(); }
        else cur += s[i];
    }
    parts.push_back(trim(cur));
    // remove empty segments at ends
    std::vector<std::string> p2;
    for (size_t i=0;i<parts.size();++i) if (!parts[i].empty()) p2.push_back(parts[i]);
    parts.swap(p2);
    return !parts.empty();
}

void Bot::doChoose(const std::string& where, const std::string& arg) {
    std::vector<std::string> options;
    if (!splitByBar(arg, options) || options.size() < 2) {
        say(where, "Usage: !choose opt1 | opt2 | opt3");
        return;
    }
    long idx = rngi((long)options.size());
    say(where, "I choose: " + options[(size_t)idx]);
}

void Bot::doSeen(const std::string& where, const std::string& arg) {
    if (!isChannel(where)) { say(where, "Use in a channel."); return; }
    std::string nick = trim(arg);
    if (nick.empty()) { say(where, "Usage: !seen <nick>"); return; }
    std::string chkey = toLower(where);
    std::string who = toLower(nick);
    std::map<std::string,std::time_t>& chmap = _lastSeen[chkey];
    std::map<std::string,std::time_t>::iterator it = chmap.find(who);
    if (it == chmap.end()) { say(where, nick + " has not been seen here (yet)."); return; }
    long ago = (long)std::difftime(std::time(0), it->second);
    say(where, nick + " was last seen " + formatDuration(ago) + " ago.");
}

bool Bot::parseDuration(const std::string& s, long& secondsOut) {
    if (s.empty()) return false;
    long total=0, num=0; bool any=false;
    for (size_t i=0;i<=s.size();++i) {
        char c = (i<s.size()? s[i] : '\0');
        if (std::isdigit((unsigned char)c)) { num = num*10 + (c-'0'); any=true; continue; }
        long mul=0;
        if (c=='h') mul=3600;
        else if (c=='m') mul=60;
        else if (c=='s' || c=='\0') mul=1;
        else return false;
        total += num*mul; num=0;
        if (c=='\0') break;
    }
    secondsOut = total;
    return any && total>0;
}
std::string Bot::formatDuration(long secs) {
    long d = secs/86400; secs%=86400;
    long h = secs/3600;  secs%=3600;
    long m = secs/60;    secs%=60;
    std::ostringstream o;
    if (d) o<<d<<"d";
    if (h){ if(o.tellp()>0) o<<" "; o<<h<<"h"; }
    if (m){ if(o.tellp()>0) o<<" "; o<<m<<"m"; }
    if (secs || (!d&&!h&&!m)){ if(o.tellp()>0) o<<" "; o<<secs<<"s"; }
    return o.str();
}

void Bot::doRemind(const std::string& where, const std::string& who, const std::string& arg) {
    std::istringstream iss(arg);
    std::string dur; iss >> dur;
    if (dur.empty()) { say(where, "Usage: !remind <in> <msg>  e.g., !remind 15m stand up"); return; }
    std::string msg; std::getline(iss, msg);
    msg = trim(msg);
    if (msg.empty()) { say(where, "Provide a reminder message."); return; }
    long sec=0;
    if (!parseDuration(dur, sec)) { say(where, "Bad duration. Try 10s, 5m, 2h30m, 1d2h."); return; }
    Reminder r; r.where = where; r.who = who; r.text = msg; r.due = std::time(0) + sec;
    _reminders.push_back(r);
    say(where, who + ": ok, I’ll remind you in " + formatDuration(sec) + ".");
}

void Bot::doPoll(const std::string& where, const std::string& who, const std::string& arg) {
    if (!isChannel(where)) { say(where, "Use in a channel."); return; }
    std::string lowArg = toLower(trim(arg));
    if (lowArg.empty() || lowArg == "help") {
        say(where, "poll: !poll new Question | Opt1 | Opt2 | ...  |  !poll vote <id> <n>  |  !poll show <id>  |  !poll close <id>");
        return;
    }
    // subcommand
    std::istringstream iss(arg);
    std::string sub; iss >> sub; std::string rest; std::getline(iss, rest); rest = trim(rest);
    std::string chkey = toLower(where);

    if (toLower(sub) == "new") {
        std::vector<std::string> parts;
        if (!splitByBar(rest, parts) || parts.size() < 3) { say(where, "Format: !poll new Question | Option1 | Option2 [...]."); return; }
        Poll p; p.id = _nextPollId++; p.channelKey = chkey; p.question = parts[0]; p.open = true; p.createdAt = std::time(0);
        for (size_t i=1;i<parts.size();++i) p.options.push_back(parts[i]);
        _polls[p.id] = p;
        std::ostringstream o; o<<"poll #"<<p.id<<": "<<p.question<<"  ";
        for (size_t i=0;i<p.options.size();++i) { if (i) o<<" | "; o<<(i+1)<<") "<<p.options[i]; }
        say(where, o.str());
        std::ostringstream idss; idss<<p.id;
        say(where, "vote with: !poll vote " + idss.str() + " <option#>");
    } else if (toLower(sub) == "vote") {
        int id=0, choice=0; { std::istringstream ps(rest); ps>>id>>choice; }
        if (id<=0 || choice<=0) { say(where, "Usage: !poll vote <id> <option#>"); return; }
        std::map<int,Poll>::iterator it = _polls.find(id);
        if (it==_polls.end() || it->second.channelKey!=chkey) { say(where, "No such poll in this channel."); return; }
        Poll& p = it->second;
        if (!p.open) { std::ostringstream os; os<<id; say(where, "Poll #"+os.str()+" is closed."); return; }
        if (choice > (int)p.options.size()) { say(where, "Invalid option."); return; }
        p.votes[toLower(who)] = choice;
        std::ostringstream os; os<<choice; std::ostringstream is; is<<id;
        say(where, who + " voted #" + os.str() + " on poll #" + is.str());
    } else if (toLower(sub) == "show") {
        int id=0; { std::istringstream ps(rest); ps>>id; }
        if (id<=0) { say(where, "Usage: !poll show <id>"); return; }
        std::map<int,Poll>::iterator it = _polls.find(id);
        if (it==_polls.end() || it->second.channelKey!=chkey) { say(where, "No such poll in this channel."); return; }
        Poll& p = it->second;
        std::vector<int> counts(p.options.size(), 0);
        for (std::map<std::string,int>::iterator vt=p.votes.begin(); vt!=p.votes.end(); ++vt) {
            int c = vt->second; if (c>=1 && c <= (int)p.options.size()) counts[(size_t)(c-1)]++;
        }
        std::ostringstream o; o<<"poll #"<<p.id<<(p.open?" (open)":" (closed)")<<": "<<p.question<<"  ";
        for (size_t i=0;i<p.options.size();++i) {
            if (i) o<<" | ";
            o<<(i+1)<<") "<<p.options[i]<<" — "<<counts[i];
        }
        say(where, o.str());
    } else if (toLower(sub) == "close") {
        int id=0; { std::istringstream ps(rest); ps>>id; }
        if (id<=0) { say(where, "Usage: !poll close <id>"); return; }
        std::map<int,Poll>::iterator it = _polls.find(id);
        if (it==_polls.end() || it->second.channelKey!=chkey) { say(where, "No such poll in this channel."); return; }
        Poll& p = it->second;
        if (!p.open) { say(where, "Poll already closed."); return; }
        p.open = false;
        std::ostringstream is; is<<id;
        say(where, "Poll #"+ is.str() +" closed.");
        // auto show
        doPoll(where, who, "show " + is.str());
    } else {
        say(where, "Unknown poll subcommand. Try: !poll (new|vote|show|close)");
    }
}

// Shunting-yard integer calculator (+ - * / and parentheses)
void Bot::doCalc(const std::string& where, const std::string& arg) {
    std::string s; { // strip spaces
        for (size_t i=0;i<arg.size();++i) if (!std::isspace((unsigned char)arg[i])) s.push_back(arg[i]);
    }
    if (s.empty()) { say(where, "Usage: !calc <expr>   e.g., !calc (2+3)*4 - 5"); return; }

    // to RPN
    std::vector<char> op;
    std::vector<std::string> out;
    for (size_t i=0;i<s.size();) {
        char c=s[i];
        if (std::isdigit((unsigned char)c)) {
            size_t j=i; while (j<s.size() && std::isdigit((unsigned char)s[j])) ++j;
            out.push_back(s.substr(i,j-i)); i=j; continue;
        }
        int prec=0; bool rightAssoc=false; char oc=0;
        if (c=='+'||c=='-'){ prec=1; oc=c; }
        else if (c=='*'||c=='/'){ prec=2; oc=c; }
        else if (c=='('){ op.push_back(c); ++i; continue; }
        else if (c==')'){
            while(!op.empty() && op.back()!='('){
                out.push_back(std::string(1,op.back()));
                op.pop_back();
            }
            if (!op.empty() && op.back()=='(') {
                op.pop_back();
            }
            ++i;
            continue;
        } else { say(where, "Invalid char in expr."); return; }

        while(!op.empty()){
            char t=op.back();
            int tp = (t=='+'||t=='-')?1: (t=='*'||t=='/')?2: 0;
            if (t=='(') break;
            if ((rightAssoc && prec<tp) || (!rightAssoc && prec<=tp)) {
                out.push_back(std::string(1,t));
                op.pop_back();
            } else break;
        }
        op.push_back(oc); ++i;
    }
    while(!op.empty()){
        if(op.back()=='('){
            say(where, "Mismatched '('");
            return;
        }
        out.push_back(std::string(1,op.back()));
        op.pop_back();
    }

    // eval RPN
    std::vector<long> st;
    for (size_t i=0;i<out.size();++i){
        const std::string& tk=out[i];
        if (tk.size()>1 || std::isdigit((unsigned char)tk[0])) { st.push_back(std::atol(tk.c_str())); continue; }
        if (st.size()<2){ say(where, "Bad expression."); return; }
        long b=st.back(); st.pop_back(); long a=st.back(); st.pop_back(); long r=0;
        char oc=tk[0];
        if (oc=='+') r=a+b; else if (oc=='-') r=a-b; else if (oc=='*') r=a*b;
        else if (oc=='/'){ if (b==0){ say(where,"Division by zero."); return; } r=a/b; }
        st.push_back(r);
    }
    if (st.size()!=1){ say(where, "Bad expression."); return; }
    std::ostringstream o; o<<st.back();
    say(where, o.str());
}

void Bot::doUptime(const std::string& where) {
    long secs = (long)std::difftime(std::time(0), _startedAt);
    say(where, "uptime: " + formatDuration(secs));
}
