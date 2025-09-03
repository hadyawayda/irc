#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <set>
# include <string>

class Channel
{
	std::string _name;
	std::string _topic;
	std::set<int> _members;           // client fds
	std::set<std::string> _operators; // nicks
	std::set<std::string> _invited;   // nicks
	bool _inviteOnly;
	bool _topicRestricted;
	std::string _key;
	int _userLimit; // -1 = none

  public:
	Channel(const std::string &name);

	const std::string &name() const;
	const std::string &topic() const;
	void setTopic(const std::string &t);

	bool hasMemberFd(int fd) const;
	void addMember(int fd);
	void removeMember(int fd);
	const std::set<int> &members() const;

	bool isOp(const std::string &nick) const;
	void addOp(const std::string &nick);
	void removeOp(const std::string &nick);

	void invite(const std::string &nick);
	bool isInvited(const std::string &nick) const;

	bool inviteOnly() const;
	void setInviteOnly(bool b);

	bool topicRestricted() const;
	void setTopicRestricted(bool b);

	const std::string &key() const;
	void setKey(const std::string &k);
	void clearKey();

	int userLimit() const;
	void setUserLimit(int lim);
	bool isFull() const;
};

#endif
