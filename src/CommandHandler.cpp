#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Utils.hpp"

// NEW: need complete types to call methods on _srv._bot and _srv._ft
#include "Bot.hpp"
#include "FileTransfer.hpp"

#include <sstream>
#include <cstdlib>
#include <unistd.h>

void CommandHandler::sendNumeric(Client& c, const std::string& code, const std::string& msg) {
    std::string nick = c.nick().empty() ? "*" : c.nick();
    _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " " + code + " " + nick + " " + msg + "\r\n");
}

void CommandHandler::handleLine(Client& c, const std::string& line) {
    std::string cmd, trailing; std::vector<std::string> params;
    splitCmd(line, cmd, params, trailing);
    if (cmd.empty()) return;
    std::string ucmd = toLower(cmd);

    if (ucmd == "pass") cmdPASS(c, params);
    else if (ucmd == "nick") cmdNICK(c, params);
    else if (ucmd == "user") cmdUSER(c, params, trailing);
    else if (ucmd == "ping") cmdPING(c, params);
    else if (ucmd == "pong") cmdPONG(c, params);
    else if (ucmd == "privmsg") cmdPRIVMSG(c, params, trailing);
    else if (ucmd == "join") cmdJOIN(c, params);
    else if (ucmd == "part") cmdPART(c, params);
    else if (ucmd == "quit") cmdQUIT(c, params, trailing);
    else if (ucmd == "topic") cmdTOPIC(c, params, trailing);
    else if (ucmd == "mode") cmdMODE(c, params);
    else if (ucmd == "invite") cmdINVITE(c, params);
    else if (ucmd == "kick") cmdKICK(c, params, trailing);
    else if (ucmd == "filesend")   cmdFILESEND(c, params, trailing);
    else if (ucmd == "fileaccept") cmdFILEACCEPT(c, params);
    else if (ucmd == "filedata")   cmdFILEDATA(c, params);
    else if (ucmd == "filedone")   cmdFILEDONE(c, params);
    else if (ucmd == "filecancel") cmdFILECANCEL(c, params);
    else {
        _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 421 * " + cmd + " :Unknown command\r\n");
        _srv.sendToClient(c.fd(), ":ircserv NOTICE * :Unknown command. Try: HELP (not implemented) or common IRC commands.\r\n");
    }
}

void CommandHandler::cmdPASS(Client& c, const std::vector<std::string>& p) {
    if (p.empty()) { sendNumeric(c, "461", "PASS :Not enough parameters"); return; }
    if (c.isRegistered()) { sendNumeric(c, "462", ":You may not reregister"); return; }
    if (p[0] == _srv._password) {
        c.setPassOk(true);
        _srv.sendToClient(c.fd(), ":ircserv NOTICE " + (c.nick().empty() ? std::string("*") : c.nick()) + " :Password accepted. Now send NICK <nickname> and USER <username> 0 * :<realname>.\r\n");
    } else {
        sendNumeric(c, "464", ":Password incorrect");
        _srv.sendToClient(c.fd(), ":ircserv NOTICE * :Incorrect password. Try: PASS <password>.\r\n");
    }
    c.tryRegister(_srv);
}

void CommandHandler::cmdNICK(Client& c, const std::vector<std::string>& p) {
    if (!c.passOk()) { sendNumeric(c, "451", "NICK :You have not registered (PASS first)"); return; }
    if (p.empty()) { sendNumeric(c, "431", ":No nickname given"); return; }

    std::string newnick = p[0];
    if (!isNickValid(newnick)) { sendNumeric(c, "432", newnick + " :Erroneous nickname"); return; }
    Client* other = _srv.findClientByNick(newnick);
    if (other && other->fd() != c.fd()) { sendNumeric(c, "433", newnick + " :Nickname is already in use"); return; }

    std::string old = c.nick();
    c.setNick(newnick);
    if (!old.empty()) {
        const std::set<std::string>& chs = c.channels();
        for (std::set<std::string>::const_iterator sit = chs.begin(); sit != chs.end(); ++sit) {
            _srv.broadcast(*sit, ":" + old + " NICK :" + newnick + "\r\n", c.fd());
        }
    }
    _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Your nickname is now '" + c.nick() + "'.\r\n");
    c.tryRegister(_srv);
}

void CommandHandler::cmdUSER(Client& c, const std::vector<std::string>& p, const std::string& trailing) {
    if (!c.passOk())      { sendNumeric(c, "451", "USER :You have not registered (PASS first)"); return; }
    if (c.nick().empty()) { sendNumeric(c, "451", "USER :You have not registered (NICK before USER)"); return; }
    if (p.size() < 3)     { sendNumeric(c, "461", "USER :Not enough parameters"); return; }
    if (trailing.empty()) { sendNumeric(c, "461", "USER :Realname must be after ':'"); return; }

    std::string username = p[0];
    std::string realname = trailing;
    c.setUser(username, realname);
    _srv.sendToClient(c.fd(), ":ircserv NOTICE " + (c.nick().empty() ? std::string("*") : c.nick()) + " :User registered as '" + username + "' (" + realname + ").\r\n");
    c.tryRegister(_srv);
}

void CommandHandler::cmdPING(Client& c, const std::vector<std::string>& p) {
    if (p.empty()) { sendNumeric(c, "409", ":No origin specified"); return; }
    _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " PONG " + _srv.serverName() + " :" + p[0] + "\r\n");
    _srv.sendToClient(c.fd(), ":ircserv NOTICE " + (c.nick().empty() ? std::string("*") : c.nick()) + " :PONG sent.\r\n");
}

void CommandHandler::cmdPONG(Client&, const std::vector<std::string>&) {
    // ignore
}

void CommandHandler::cmdPRIVMSG(Client& c, const std::vector<std::string>& p, const std::string& trailing) {
    if (!requireRegistered(c, "PRIVMSG")) return;
    if (p.empty()) { sendNumeric(c, "411", ":No recipient given (PRIVMSG)"); return; }

    // Determine message text: allow single-word without ':', else require trailing
    std::string text;
    if (!trailing.empty()) text = trailing;
    else if (p.size() >= 2) text = p[1];
    else { sendNumeric(c, "412", ":No text to send (use ':' before multi-word text)"); return; }
    if (text.empty()) { sendNumeric(c, "412", ":No text to send"); return; }

    // split comma-separated targets
    std::vector<std::string> tgts; { std::string cur; for (size_t i=0;i<p[0].size();++i){ if(p[0][i]==','){ if(!cur.empty()) tgts.push_back(cur); cur.clear(); } else cur+=p[0][i]; } if(!cur.empty()) tgts.push_back(cur); }
    if (tgts.empty()) { sendNumeric(c, "411", ":No recipient given (PRIVMSG)"); return; }

    for (size_t i = 0; i < tgts.size(); ++i) {
        const std::string& target = tgts[i];
        if (isChannelName(target)) {
            Channel* ch = _srv.findChannel(target);
            if (!ch) { sendNumeric(c, "403", target + " :No such channel"); continue; }
            if (!ch->hasMemberFd(c.fd())) { sendNumeric(c, "442", target + " :You're not on that channel"); continue; }
            std::string msg = ":" + c.nick() + " PRIVMSG " + target + " :" + text + "\r\n";
            _srv.broadcast(target, msg, c.fd());
            _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Message sent to " + target + ".\r\n");
        } else {
            Client* dst = _srv.findClientByNick(target);
            if (!dst) { sendNumeric(c, "401", target + " :No such nick"); continue; }
            _srv.sendToClient(dst->fd(), ":" + c.nick() + " PRIVMSG " + dst->nick() + " :" + text + "\r\n");
            _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Message sent to " + target + ".\r\n");
        }
    }

    // Bot hook
    if (_srv._bot) {
        std::string target = p[0];
        _srv._bot->onPrivmsg(c, target, text);
    }
}

void CommandHandler::cmdJOIN(Client& c, const std::vector<std::string>& p) {
    if (!requireRegistered(c, "JOIN")) return;

    if (p.empty()) { sendNumeric(c, "461", "JOIN :Not enough parameters"); return; }
    std::string chan = p[0];
    std::string key  = (p.size() >= 2 ? p[1] : "");

    if (!isChannelName(chan)) { sendNumeric(c, "403", chan + " :No such channel"); return; }
    Channel* ch = _srv.getOrCreateChannel(chan);

    if (!ch->key().empty() && ch->key() != key) { sendNumeric(c, "475", chan + " :Cannot join channel (+k)"); return; }
    if (ch->inviteOnly() && !ch->isInvited(c.nick())) { sendNumeric(c, "473", chan + " :Cannot join channel (+i)"); return; }
    if (ch->isFull()) { sendNumeric(c, "471", chan + " :Cannot join channel (+l)"); return; }

    if (ch->isInvited(c.nick())) ch->consumeInvite(c.nick());

    if (!ch->hasMemberFd(c.fd())) {
        ch->addMember(c.fd());
        c.joinChannel(toLower(chan));
        if (ch->members().size() == 1) ch->addOp(c.nick());

        std::string joinmsg = ":" + c.nick() + " JOIN " + chan + "\r\n";
        _srv.broadcast(chan, joinmsg, -1);
        _srv.sendToClient(c.fd(), joinmsg);

        if (!ch->topic().empty())
            _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 332 " + c.nick() + " " + chan + " :" + ch->topic() + "\r\n");

        std::string names;
        const std::set<int>& mem = ch->members();
        for (std::set<int>::const_iterator it = mem.begin(); it != mem.end(); ++it) {
            Client* m = _srv._clients[*it];
            if (names.size()) names += " ";
            if (ch->isOp(m->nick())) names += "@";
            names += m->nick();
        }
        _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 353 " + c.nick() + " = " + chan + " :" + names + "\r\n");
        _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 366 " + c.nick() + " " + chan + " :End of /NAMES list.\r\n");

        _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Joined " + chan + ". Type: PRIVMSG " + chan + " :hello\r\n");
    }
}

void CommandHandler::cmdPART(Client& c, const std::vector<std::string>& p) {
    if (!requireRegistered(c, "PART")) return;

    if (p.empty()) { sendNumeric(c, "461", "PART :Not enough parameters"); return; }
    std::string chan = p[0];
    Channel* ch = _srv.findChannel(chan);
    if (!ch || !ch->hasMemberFd(c.fd())) { sendNumeric(c, "442", chan + " :You're not on that channel"); return; }

    ch->removeMember(c.fd());
    ch->removeOp(c.nick()); // do not keep op on leave
    c.leaveChannel(toLower(chan));
    std::string part = ":" + c.nick() + " PART " + chan + "\r\n";
    _srv.broadcast(chan, part, -1);
    _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :You left " + chan + ".\r\n");

    _srv.promoteOpIfNone(*ch);
    _srv.maybeDeleteChannel(chan);
}

void CommandHandler::cmdQUIT(Client& c, const std::vector<std::string>&, const std::string& trailing) {
    std::string reason = trailing.empty() ? "Quit" : trailing;
    for (std::set<std::string>::const_iterator sit = c.channels().begin(); sit != c.channels().end(); ++sit) {
        _srv.broadcast(*sit, ":" + c.nick() + " QUIT :" + reason + "\r\n", c.fd());
    }
    close(c.fd()); // Server::removeClient will finish cleanup on next loop
}

void CommandHandler::cmdTOPIC(Client& c, const std::vector<std::string>& p, const std::string& trailing) {
    if (!requireRegistered(c, "TOPIC")) return;

    if (p.empty()) { sendNumeric(c, "461", "TOPIC :Not enough parameters"); return; }
    std::string chan = p[0];
    Channel* ch = _srv.findChannel(chan);
    if (!ch) { sendNumeric(c, "403", chan + " :No such channel"); return; }
    if (!ch->hasMemberFd(c.fd())) { sendNumeric(c, "442", chan + " :You're not on that channel"); return; }

    if (trailing.empty()) {
        if (ch->topic().empty()) {
            _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 331 " + c.nick() + " " + chan + " :No topic is set\r\n");
            _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Use: TOPIC " + chan + " :<new topic>\r\n");
        } else {
            _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 332 " + c.nick() + " " + chan + " :" + ch->topic() + "\r\n");
        }
        return;
    }
    if (ch->topicRestricted() && !ch->isOp(c.nick())) { sendNumeric(c, "482", chan + " :You're not channel operator"); return; }
    ch->setTopic(trailing);
    std::string msg = ":" + c.nick() + " TOPIC " + chan + " :" + trailing + "\r\n";
    _srv.broadcast(chan, msg, -1);
    _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Topic for " + chan + " is now: " + trailing + "\r\n");
}

void CommandHandler::cmdMODE(Client& c, const std::vector<std::string>& p) {
    if (!requireRegistered(c, "MODE")) return;

    if (p.empty()) { sendNumeric(c, "461", "MODE :Not enough parameters"); return; }
    std::string chan = p[0];
    Channel* ch = _srv.findChannel(chan);
    if (!ch) { sendNumeric(c, "403", chan + " :No such channel"); return; }
    if (!ch->hasMemberFd(c.fd())) { sendNumeric(c, "442", chan + " :You're not on that channel"); return; }

    // helper to send current modes + args
    std::string modes = "+";
    std::string args;
    if (ch->inviteOnly()) modes += "i";
    if (ch->topicRestricted()) modes += "t";
    if (!ch->key().empty()) { modes += "k"; if (!args.empty()) args += " "; args += ch->key(); }
    if (ch->userLimit() != -1) { modes += "l"; if (!args.empty()) args += " "; std::ostringstream os; os << ch->userLimit(); args += os.str(); }

    if (p.size() == 1) {
        _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 324 " + c.nick() + " " + chan + " " + modes + (args.empty() ? "" : (" " + args)) + "\r\n");
        _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Modes on " + chan + " are " + modes + (args.empty() ? "" : (" " + args)) + " (i=invite-only, t=topic-ops-only, k=key, l=limit).\r\n");
        return;
    }
    if (!ch->isOp(c.nick())) { sendNumeric(c, "482", chan + " :You're not channel operator"); return; }

    std::string flags = p[1];
    bool adding = true;
    size_t argi = 2;
    for (size_t i = 0; i < flags.size(); ++i) {
        char f = flags[i];
        if (f == '+') { adding = true; continue; }
        if (f == '-') { adding = false; continue; }
        if (f == 'i') ch->setInviteOnly(adding);
        else if (f == 't') ch->setTopicRestricted(adding);
        else if (f == 'k') {
            if (adding) {
                if (argi >= p.size()) { sendNumeric(c, "461", "MODE :Not enough parameters"); return; }
                ch->setKey(p[argi++]);
            } else ch->clearKey();
        } else if (f == 'o') {
            if (argi >= p.size()) { sendNumeric(c, "461", "MODE :Not enough parameters"); return; }
            std::string nick = p[argi++];
            if (adding) ch->addOp(nick);
            else ch->removeOp(nick);
            _srv.broadcast(chan, ":" + c.nick() + " MODE " + chan + (adding ? " +o " : " -o ") + nick + "\r\n", -1);
        } else if (f == 'l') {
            if (adding) {
                if (argi >= p.size()) { sendNumeric(c, "461", "MODE :Not enough parameters"); return; }
                int lim = std::atoi(p[argi++].c_str());
                if (lim < (int)ch->members().size()) {
                    sendNumeric(c, "471", chan + " :Cannot set limit below current members");
                    continue;
                }
                if (lim < 0) lim = 0;
                ch->setUserLimit(lim);
            } else ch->setUserLimit(-1);
        }
    }
    // rebuild modes + args after change
    modes = "+";
    args.clear();
    if (ch->inviteOnly()) modes += "i";
    if (ch->topicRestricted()) modes += "t";
    if (!ch->key().empty()) { modes += "k"; if (!args.empty()) args += " "; args += ch->key(); }
    if (ch->userLimit() != -1) {
        modes += "l";
        if (!args.empty()) args += " ";
        std::ostringstream os; os << ch->userLimit();
        args += os.str();
    }

    std::string final_modes_line = ":" + c.nick() + " MODE " + chan + " " + modes + (args.empty() ? "" : (" " + args)) + "\r\n";
    _srv.broadcast(chan, final_modes_line, -1);
    _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 324 " + c.nick() + " " + chan + " " + modes + (args.empty() ? "" : (" " + args)) + "\r\n");
    _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Set modes on " + chan + " to " + modes + (args.empty() ? "" : (" " + args)) + " (i=invite-only, t=topic-ops-only, k=key, l=limit).\r\n");
}

void CommandHandler::cmdINVITE(Client& c, const std::vector<std::string>& p) {
    if (!requireRegistered(c, "INVITE")) return;

    if (p.size() < 2) { sendNumeric(c, "461", "INVITE :Not enough parameters"); return; }
    std::string nick = p[0];
    std::string chan = p[1];
    Channel* ch = _srv.findChannel(chan);
    if (!ch) { sendNumeric(c, "403", chan + " :No such channel"); return; }
    if (!ch->hasMemberFd(c.fd())) { sendNumeric(c, "442", chan + " :You're not on that channel"); return; }
    if (!ch->isOp(c.nick())) { sendNumeric(c, "482", chan + " :You're not channel operator"); return; }
    Client* target = _srv.findClientByNick(nick);
    if (!target) { sendNumeric(c, "401", nick + " :No such nick"); return; }

    ch->invite(nick);
    _srv.sendToClient(target->fd(), ":" + c.nick() + " INVITE " + nick + " " + chan + "\r\n");
    sendNumeric(c, "341", nick + " " + chan);
    _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Invited " + nick + " to " + chan + ". If +i (invite-only) is set, they can now JOIN.\r\n");
}

void CommandHandler::cmdKICK(Client& c, const std::vector<std::string>& p, const std::string& trailing) {
    if (!requireRegistered(c, "KICK")) return;

    if (p.size() < 2) { sendNumeric(c, "461", "KICK :Not enough parameters"); return; }
    std::string chan = p[0];
    std::string victimNick = p[1];
    Channel* ch = _srv.findChannel(chan);
    if (!ch) { sendNumeric(c, "403", chan + " :No such channel"); return; }
    if (!ch->hasMemberFd(c.fd())) { sendNumeric(c, "442", chan + " :You're not on that channel"); return; }
    if (!ch->isOp(c.nick())) { sendNumeric(c, "482", chan + " :You're not channel operator"); return; }
    if (p.size() > 2 && trailing.empty()) { sendNumeric(c, "461", "KICK :Reason must be after ':'"); return; }

    Client* victim = _srv.findClientByNick(victimNick);
    if (!victim || !ch->hasMemberFd(victim->fd())) { sendNumeric(c, "441", victimNick + " " + chan + " :They aren't on that channel"); return; }

    std::string reason = trailing.empty() ? "Kicked" : trailing;
    std::string kickmsg = ":" + c.nick() + " KICK " + chan + " " + victimNick + " :" + reason + "\r\n";
    _srv.broadcast(chan, kickmsg, victim->fd());
    _srv.sendToClient(victim->fd(), kickmsg);

    ch->removeMember(victim->fd());
    ch->removeOp(victim->nick()); // do not keep op after kick
    victim->leaveChannel(toLower(chan));

    _srv.promoteOpIfNone(*ch);
    _srv.maybeDeleteChannel(chan);

    _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Kicked " + victimNick + " from " + chan + ".\r\n");
}

void CommandHandler::cmdFILESEND(Client& c, const std::vector<std::string>& p, const std::string& trailing) {
    if (!requireRegistered(c, "FILESEND")) return;
    if (p.size() < 2 || trailing.empty()) { sendNumeric(c, "461", "FILESEND :Not enough parameters"); return; }
    std::string targetNick = p[0];
    unsigned long sizeTotal = std::strtoul(p[1].c_str(), 0, 10);
    Client* dst = _srv.findClientByNick(targetNick);
    if (!dst) { sendNumeric(c, "401", targetNick + " :No such nick"); return; }
    int tid = _srv._ft->createOffer(c.fd(), dst->fd(), trailing, sizeTotal);
    std::ostringstream tidStream; tidStream << tid;
    std::string tidStr = tidStream.str();
    _srv.sendToClient(c.fd(),  ":" + _srv.serverName() + " 739 " + c.nick() + " " + targetNick + " " + (tid > 0 ? tidStr : "0") + " " + p[1] + " :" + trailing + "\r\n");
    _srv.sendToClient(dst->fd(), ":" + _srv.serverName() + " 738 " + c.nick() + " " + tidStr + " " + p[1] + " :" + trailing + "\r\n");
    _srv.sendToClient(dst->fd(), ":ircserv NOTICE " + dst->nick() + " :Use FILEACCEPT " + tidStr + " to receive.\r\n");
}

void CommandHandler::cmdFILEACCEPT(Client& c, const std::vector<std::string>& p) {
    if (!requireRegistered(c, "FILEACCEPT")) return;
    if (p.empty()) { sendNumeric(c, "461", "FILEACCEPT :Not enough parameters"); return; }
    int tid = std::atoi(p[0].c_str());
    if (_srv._ft->accept(tid, c.fd())) {
        _srv.sendToClient(c.fd(),  ":" + _srv.serverName() + " 742 * " + p[0] + " :ACCEPTED\r\n");
        _srv.sendToClient(c.fd(), ":ircserv NOTICE " + c.nick() + " :Start receiving with FILEDATA relayed by server.\r\n");
    } else sendNumeric(c, "400", p[0] + " :Cannot accept");
}

void CommandHandler::cmdFILEDATA(Client& c, const std::vector<std::string>& p) {
    if (!requireRegistered(c, "FILEDATA")) return;
    if (p.size() < 2) { sendNumeric(c, "461", "FILEDATA :Not enough parameters"); return; }
    int tid = std::atoi(p[0].c_str());
    std::string err;
    if (!_srv._ft->pushData(tid, c.fd(), p[1], err)) {
        sendNumeric(c, "400", p[0] + " :" + err);
    }
}

void CommandHandler::cmdFILEDONE(Client& c, const std::vector<std::string>& p) {
    if (!requireRegistered(c, "FILEDONE")) return;
    if (p.empty()) { sendNumeric(c, "461", "FILEDONE :Not enough parameters"); return; }
    int tid = std::atoi(p[0].c_str());
    std::string err;
    if (!_srv._ft->done(tid, c.fd(), err)) sendNumeric(c, "400", p[0] + " :" + err);
}

void CommandHandler::cmdFILECANCEL(Client& c, const std::vector<std::string>& p) {
    if (!requireRegistered(c, "FILECANCEL")) return;
    if (p.empty()) { sendNumeric(c, "461", "FILECANCEL :Not enough parameters"); return; }
    int tid = std::atoi(p[0].c_str());
    std::string reason;
    if (_srv._ft->cancel(tid, c.fd(), reason)) {
        _srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 743 * " + p[0] + " :" + reason + "\r\n");
    } else sendNumeric(c, "400", p[0] + " :Cannot cancel");
}

bool CommandHandler::requireRegistered(Client& c, const char* forCmd) {
    if (c.isRegistered()) return true;
    std::string what = forCmd ? std::string(forCmd) : std::string("*");
    sendNumeric(c, "451", what + " :You have not registered");
    return false;
}
