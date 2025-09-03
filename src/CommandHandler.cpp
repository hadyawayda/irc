#include "Channel.hpp"
#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Utils.hpp"
#include <cstdlib>
#include <sstream>
#include <unistd.h> // close

void CommandHandler::sendNumeric(Client &c, const std::string &code,
	const std::string &msg)
{
	std::string nick = c.nick().empty() ? "*" : c.nick();
	_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " " + code + " " + nick
		+ " " + msg + "\r\n");
}

void CommandHandler::handleLine(Client &c, const std::string &line)
{
	std::string cmd, trailing;
	std::vector<std::string> params;
	splitCmd(line, cmd, params, trailing);
	if (cmd.empty())
		return ;
	std::string ucmd = toLower(cmd);
	if (ucmd == "pass")
		cmdPASS(c, params);
	else if (ucmd == "nick")
		cmdNICK(c, params);
	else if (ucmd == "user")
		cmdUSER(c, params, trailing);
	else if (ucmd == "ping")
		cmdPING(c, params);
	else if (ucmd == "pong")
		cmdPONG(c, params);
	else if (ucmd == "privmsg")
		cmdPRIVMSG(c, params, trailing);
	else if (ucmd == "join")
		cmdJOIN(c, params);
	else if (ucmd == "part")
		cmdPART(c, params);
	else if (ucmd == "quit")
		cmdQUIT(c, params, trailing);
	else if (ucmd == "topic")
		cmdTOPIC(c, params, trailing);
	else if (ucmd == "mode")
		cmdMODE(c, params);
	else if (ucmd == "invite")
		cmdINVITE(c, params);
	else if (ucmd == "kick")
		cmdKICK(c, params, trailing);
	else
	{
		_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 421 * " + cmd
			+ " :Unknown command\r\n");
	}
}

void CommandHandler::cmdPASS(Client &c, const std::vector<std::string> &p)
{
	if (p.empty())
	{
		sendNumeric(c, "461", "PASS :Not enough parameters");
		return ;
	}
	if (c.isRegistered())
	{
		sendNumeric(c, "462", ":You may not reregister");
		return ;
	}
	if (p[0] == _srv._password)
		c.setPassOk(true);
	else
	{
		sendNumeric(c, "464", ":Password incorrect");
	}
	c.tryRegister(_srv);
}

void CommandHandler::cmdNICK(Client &c, const std::vector<std::string> &p)
{
	Client	*other;

	if (p.empty())
	{
		sendNumeric(c, "431", ":No nickname given");
		return ;
	}
	std::string newnick = p[0];
	if (!isNickValid(newnick))
	{
		sendNumeric(c, "432", newnick + " :Erroneous nickname");
		return ;
	}
	other = _srv.findClientByNick(newnick);
	if (other && other->fd() != c.fd())
	{
		sendNumeric(c, "433", newnick + " :Nickname is already in use");
		return ;
	}
	std::string old = c.nick();
	c.setNick(newnick);
	if (!old.empty())
	{
		for (std::set<std::string>::const_iterator sit = c.channels().begin(); sit != c.channels().end(); ++sit)
		{
			_srv.broadcast(*sit, ":" + old + " NICK :" + newnick + "\r\n",
				c.fd());
		}
	}
	c.tryRegister(_srv);
}

void CommandHandler::cmdUSER(Client &c, const std::vector<std::string> &p,
	const std::string &trailing)
{
	if (p.size() < 3)
	{
		sendNumeric(c, "461", "USER :Not enough parameters");
		return ;
	}
	std::string username = p[0];
	std::string realname = trailing.empty() ? p[2] : trailing;
	c.setUser(username, realname);
	c.tryRegister(_srv);
}

void CommandHandler::cmdPING(Client &c, const std::vector<std::string> &p)
{
	if (p.empty())
	{
		sendNumeric(c, "409", ":No origin specified");
		return ;
	}
	_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " PONG "
		+ _srv.serverName() + " :" + p[0] + "\r\n");
}

void CommandHandler::cmdPONG(Client & /*c*/, const std::vector<std::string> &
	/*p*/)
{
	// ignore
}

void CommandHandler::cmdPRIVMSG(Client &c, const std::vector<std::string> &p,
	const std::string &trailing)
{
	Channel	*ch;
	Client	*dst;

	if (p.empty() || (trailing.empty() && (p.size() < 2)))
	{
		sendNumeric(c, "411", ":No recipient given (PRIVMSG)");
		return ;
	}
	std::string target = p[0];
	std::string text = trailing.empty() ? (p.size() >= 2 ? p[1] : "") : trailing;
	if (text.empty())
	{
		sendNumeric(c, "412", ":No text to send");
		return ;
	}
	if (isChannelName(target))
	{
		ch = _srv.findChannel(target);
		if (!ch || !ch->hasMemberFd(c.fd()))
		{
			sendNumeric(c, "404", target + " :Cannot send to channel");
			return ;
		}
		std::string msg = ":" + c.nick() + " PRIVMSG " + target + " :" + text
			+ "\r\n";
		_srv.broadcast(target, msg, c.fd());
	}
	else
	{
		dst = _srv.findClientByNick(target);
		if (!dst)
		{
			sendNumeric(c, "401", target + " :No such nick");
			return ;
		}
		_srv.sendToClient(dst->fd(), ":" + c.nick() + " PRIVMSG " + dst->nick()
			+ " :" + text + "\r\n");
	}
}

void CommandHandler::cmdJOIN(Client &c, const std::vector<std::string> &p)
{
	Channel	*ch;
	Client	*m;

	if (p.empty())
	{
		sendNumeric(c, "461", "JOIN :Not enough parameters");
		return ;
	}
	std::string chan = p[0];
	std::string key = (p.size() >= 2 ? p[1] : "");
	if (!isChannelName(chan))
	{
		sendNumeric(c, "403", chan + " :No such channel");
		return ;
	}
	ch = _srv.getOrCreateChannel(chan);
	if (!ch->key().empty() && ch->key() != key)
	{
		sendNumeric(c, "475", chan + " :Cannot join channel (+k)");
		return ;
	}
	if (ch->inviteOnly() && !ch->isInvited(c.nick()))
	{
		sendNumeric(c, "473", chan + " :Cannot join channel (+i)");
		return ;
	}
	if (ch->isFull())
	{
		sendNumeric(c, "471", chan + " :Cannot join channel (+l)");
		return ;
	}
	if (!ch->hasMemberFd(c.fd()))
	{
		ch->addMember(c.fd());
		c.joinChannel(toLower(chan));
		if (ch->members().size() == 1)
			ch->addOp(c.nick());
		std::string joinmsg = ":" + c.nick() + " JOIN " + chan + "\r\n";
		_srv.broadcast(chan, joinmsg, -1);
		_srv.sendToClient(c.fd(), joinmsg);
		if (!ch->topic().empty())
			_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 332 "
				+ c.nick() + " " + chan + " :" + ch->topic() + "\r\n");
		std::string names;
		const std::set<int> &mem = ch->members();
		for (std::set<int>::const_iterator it = mem.begin(); it != mem.end(); ++it)
		{
			m = _srv._clients[*it];
			if (names.size())
				names += " ";
			if (ch->isOp(m->nick()))
				names += "@";
			names += m->nick();
		}
		_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 353 " + c.nick()
			+ " = " + chan + " :" + names + "\r\n");
		_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 366 " + c.nick()
			+ " " + chan + " :End of /NAMES list.\r\n");
	}
}

void CommandHandler::cmdPART(Client &c, const std::vector<std::string> &p)
{
	Channel	*ch;

	if (p.empty())
	{
		sendNumeric(c, "461", "PART :Not enough parameters");
		return ;
	}
	std::string chan = p[0];
	ch = _srv.findChannel(chan);
	if (!ch || !ch->hasMemberFd(c.fd()))
	{
		sendNumeric(c, "442", chan + " :You're not on that channel");
		return ;
	}
	ch->removeMember(c.fd());
	c.leaveChannel(toLower(chan));
	std::string part = ":" + c.nick() + " PART " + chan + "\r\n";
	_srv.broadcast(chan, part, -1);
}

void CommandHandler::cmdQUIT(Client &c, const std::vector<std::string> &,
	const std::string &trailing)
{
	std::string reason = trailing.empty() ? "Quit" : trailing;
	for (std::set<std::string>::const_iterator sit = c.channels().begin(); sit != c.channels().end(); ++sit)
	{
		_srv.broadcast(*sit, ":" + c.nick() + " QUIT :" + reason + "\r\n",
			c.fd());
	}
	close(c.fd());
}

void CommandHandler::cmdTOPIC(Client &c, const std::vector<std::string> &p,
	const std::string &trailing)
{
	Channel	*ch;

	if (p.empty())
	{
		sendNumeric(c, "461", "TOPIC :Not enough parameters");
		return ;
	}
	std::string chan = p[0];
	ch = _srv.findChannel(chan);
	if (!ch)
	{
		sendNumeric(c, "403", chan + " :No such channel");
		return ;
	}
	if (!ch->hasMemberFd(c.fd()))
	{
		sendNumeric(c, "442", chan + " :You're not on that channel");
		return ;
	}
	if (trailing.empty())
	{
		if (ch->topic().empty())
			_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 331 "
				+ c.nick() + " " + chan + " :No topic is set\r\n");
		else
			_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 332 "
				+ c.nick() + " " + chan + " :" + ch->topic() + "\r\n");
		return ;
	}
	if (ch->topicRestricted() && !ch->isOp(c.nick()))
	{
		sendNumeric(c, "482", chan + " :You're not channel operator");
		return ;
	}
	ch->setTopic(trailing);
	std::string msg = ":" + c.nick() + " TOPIC " + chan + " :" + trailing
		+ "\r\n";
	_srv.broadcast(chan, msg, -1);
}

void CommandHandler::cmdMODE(Client &c, const std::vector<std::string> &p)
{
	Channel	*ch;
	bool	adding;
	size_t	argi;
	char	f;
	int		lim;

	if (p.empty())
	{
		sendNumeric(c, "461", "MODE :Not enough parameters");
		return ;
	}
	std::string chan = p[0];
	ch = _srv.findChannel(chan);
	if (!ch)
	{
		sendNumeric(c, "403", chan + " :No such channel");
		return ;
	}
	if (!ch->hasMemberFd(c.fd()))
	{
		sendNumeric(c, "442", chan + " :You're not on that channel");
		return ;
	}
	if (p.size() == 1)
	{
		std::string modes = "+";
		if (ch->inviteOnly())
			modes += "i";
		if (ch->topicRestricted())
			modes += "t";
		if (!ch->key().empty())
			modes += "k";
		if (ch->userLimit() != -1)
			modes += "l";
		_srv.sendToClient(c.fd(), ":" + _srv.serverName() + " 324 " + c.nick()
			+ " " + chan + " " + modes + "\r\n");
		return ;
	}
	if (!ch->isOp(c.nick()))
	{
		sendNumeric(c, "482", chan + " :You're not channel operator");
		return ;
	}
	std::string flags = p[1];
	adding = true;
	argi = 2;
	for (size_t i = 0; i < flags.size(); ++i)
	{
		f = flags[i];
		if (f == '+')
		{
			adding = true;
			continue ;
		}
		if (f == '-')
		{
			adding = false;
			continue ;
		}
		if (f == 'i')
			ch->setInviteOnly(adding);
		else if (f == 't')
			ch->setTopicRestricted(adding);
		else if (f == 'k')
		{
			if (adding)
			{
				if (argi >= p.size())
				{
					sendNumeric(c, "461", "MODE :Not enough parameters");
					return ;
				}
				ch->setKey(p[argi++]);
			}
			else
				ch->clearKey();
		}
		else if (f == 'o')
		{
			if (argi >= p.size())
			{
				sendNumeric(c, "461", "MODE :Not enough parameters");
				return ;
			}
			std::string nick = p[argi++];
			if (adding)
				ch->addOp(nick);
			else
				ch->removeOp(nick);
		}
		else if (f == 'l')
		{
			if (adding)
			{
				if (argi >= p.size())
				{
					sendNumeric(c, "461", "MODE :Not enough parameters");
					return ;
				}
				lim = std::atoi(p[argi++].c_str());
				if (lim < 0)
					lim = 0;
				ch->setUserLimit(lim);
			}
			else
				ch->setUserLimit(-1);
		}
	}
	_srv.broadcast(chan, ":" + c.nick() + " MODE " + chan + " " + flags
		+ "\r\n", -1);
}

void CommandHandler::cmdINVITE(Client &c, const std::vector<std::string> &p)
{
	Channel	*ch;
	Client	*target;

	if (p.size() < 2)
	{
		sendNumeric(c, "461", "INVITE :Not enough parameters");
		return ;
	}
	std::string nick = p[0];
	std::string chan = p[1];
	ch = _srv.findChannel(chan);
	if (!ch)
	{
		sendNumeric(c, "403", chan + " :No such channel");
		return ;
	}
	if (!ch->hasMemberFd(c.fd()))
	{
		sendNumeric(c, "442", chan + " :You're not on that channel");
		return ;
	}
	if (!ch->isOp(c.nick()))
	{
		sendNumeric(c, "482", chan + " :You're not channel operator");
		return ;
	}
	target = _srv.findClientByNick(nick);
	if (!target)
	{
		sendNumeric(c, "401", nick + " :No such nick");
		return ;
	}
	ch->invite(nick);
	_srv.sendToClient(target->fd(), ":" + c.nick() + " INVITE " + nick + " "
		+ chan + "\r\n");
	sendNumeric(c, "341", nick + " " + chan);
}

void CommandHandler::cmdKICK(Client &c, const std::vector<std::string> &p,
	const std::string &trailing)
{
	Channel	*ch;
	Client	*victim;

	if (p.size() < 2)
	{
		sendNumeric(c, "461", "KICK :Not enough parameters");
		return ;
	}
	std::string chan = p[0];
	std::string victimNick = p[1];
	ch = _srv.findChannel(chan);
	if (!ch)
	{
		sendNumeric(c, "403", chan + " :No such channel");
		return ;
	}
	if (!ch->hasMemberFd(c.fd()))
	{
		sendNumeric(c, "442", chan + " :You're not on that channel");
		return ;
	}
	if (!ch->isOp(c.nick()))
	{
		sendNumeric(c, "482", chan + " :You're not channel operator");
		return ;
	}
	victim = _srv.findClientByNick(victimNick);
	if (!victim || !ch->hasMemberFd(victim->fd()))
	{
		sendNumeric(c, "441", victimNick + " " + chan
			+ " :They aren't on that channel");
		return ;
	}
	std::string reason = trailing.empty() ? "Kicked" : trailing;
	std::string kickmsg = ":" + c.nick() + " KICK " + chan + " " + victimNick
		+ " :" + reason + "\r\n";
	_srv.broadcast(chan, kickmsg, -1);
	_srv.sendToClient(victim->fd(), kickmsg);
	ch->removeMember(victim->fd());
	victim->leaveChannel(toLower(chan));
}
