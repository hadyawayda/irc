#include "Utils.hpp"
#include <cctype>
#include <sstream>

std::string toLower(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
		out[i] = std::tolower(out[i]);
	return (out);
}

static void	trimCRLF(std::string &s)
{
	while (!s.empty() && (s[s.size() - 1] == '\r' || s[s.size() - 1] == '\n'))
		s.erase(s.size() - 1);
}

void	splitCmd(const std::string &rawline, std::string &command,
		std::vector<std::string> &params, std::string &trailing)
{
	size_t	pos;
	size_t	sp;
	size_t	colon;

	params.clear();
	trailing.clear();
	command.clear();
	std::string line = rawline;
	trimCRLF(line);
	// optional prefix starts with ':' — ignore (no s2s)
	pos = 0;
	if (!line.empty() && line[0] == ':')
	{
		sp = line.find(' ');
		if (sp == std::string::npos)
			return ;
		pos = sp + 1;
	}
	// trailing starts with " :"
	colon = line.find(" :", pos);
	std::string head = (colon == std::string::npos) ? line.substr(pos) : line.substr(pos,
			colon - pos);
	if (colon != std::string::npos)
		trailing = line.substr(colon + 2);
	std::istringstream iss(head);
	iss >> command;
	std::string p;
	while (iss >> p)
		params.push_back(p);
}

bool	isChannelName(const std::string &name)
{
	return (!name.empty() && (name[0] == '#' || name[0] == '&'));
}

bool	isNickValid(const std::string &nick)
{
	char	c;

	if (nick.empty())
		return (false);
	for (size_t i = 0; i < nick.size(); ++i)
	{
		c = nick[i];
		if (!(std::isalnum(c) || c == '-' || c == '_'))
			return (false);
	}
	return (true);
}
