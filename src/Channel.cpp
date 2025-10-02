#include "Channel.hpp"

Channel::Channel(const std::string& name)
: _name(name), _inviteOnly(false), _topicRestricted(false), _userLimit(-1) {}

const std::string& Channel::name() const { return _name; }
const std::string& Channel::topic() const { return _topic; }
void Channel::setTopic(const std::string& t) { _topic = t; }

bool Channel::hasMemberFd(int fd) const { return _members.find(fd) != _members.end(); }
void Channel::addMember(int fd) { _members.insert(fd); }
void Channel::removeMember(int fd) { _members.erase(fd); }
const std::set<int>& Channel::members() const { return _members; }

bool Channel::isOp(const std::string& nick) const { return _operators.find(nick) != _operators.end(); }
void Channel::addOp(const std::string& nick) { _operators.insert(nick); }
void Channel::removeOp(const std::string& nick) { _operators.erase(nick); }
bool Channel::hasAnyOp() const { return !_operators.empty(); }

void Channel::invite(const std::string& nick) { _invited.insert(nick); }
bool Channel::isInvited(const std::string& nick) const { return _invited.find(nick) != _invited.end(); }
bool Channel::consumeInvite(const std::string& nick) { std::set<std::string>::iterator it = _invited.find(nick); if (it == _invited.end()) return false; _invited.erase(it); return true; }

bool Channel::inviteOnly() const { return _inviteOnly; }
void Channel::setInviteOnly(bool b) { _inviteOnly = b; }

bool Channel::topicRestricted() const { return _topicRestricted; }
void Channel::setTopicRestricted(bool b) { _topicRestricted = b; }

const std::string& Channel::key() const { return _key; }
void Channel::setKey(const std::string& k) { _key = k; }
void Channel::clearKey() { _key.clear(); }

int Channel::userLimit() const { return _userLimit; }
void Channel::setUserLimit(int lim) { _userLimit = lim; }
bool Channel::isFull() const { return _userLimit != -1 && (int)_members.size() >= _userLimit; }
