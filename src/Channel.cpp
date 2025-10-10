#include "Channel.hpp"

// Construct a channel with the given display name. Modes and limits are
// initialized to defaults (not invite-only, no topic restriction, unlimited users).
Channel::Channel(const std::string& name)
: _name(name), _inviteOnly(false), _topicRestricted(false), _userLimit(-1) {}

// Return the display name of the channel.
const std::string& Channel::name() const { return _name; }
// Return the current topic string.
const std::string& Channel::topic() const { return _topic; }
// Set the channel topic.
void Channel::setTopic(const std::string& t) { _topic = t; }

// True if the given fd is a member of this channel.
bool Channel::hasMemberFd(int fd) const { return _members.find(fd) != _members.end(); }
// Add a client fd to the member set.
void Channel::addMember(int fd) { _members.insert(fd); }
// Remove a client fd from the member set.
void Channel::removeMember(int fd) { _members.erase(fd); }
// Return the set of member fds.
const std::set<int>& Channel::members() const { return _members; }

// True if the given nick is an operator in this channel.
bool Channel::isOp(const std::string& nick) const { return _operators.find(nick) != _operators.end(); }
// Add a nick to the operator set.
void Channel::addOp(const std::string& nick) { _operators.insert(nick); }
// Remove a nick from the operator set.
void Channel::removeOp(const std::string& nick) { _operators.erase(nick); }
<<<<<<< Updated upstream
=======
// True if any operator exists.
bool Channel::hasAnyOp() const { return !_operators.empty(); }
>>>>>>> Stashed changes

// Add a nick to the invite list (for +i channels).
void Channel::invite(const std::string& nick) { _invited.insert(nick); }
// True if the nick is currently invited.
bool Channel::isInvited(const std::string& nick) const { return _invited.find(nick) != _invited.end(); }
// Remove the invite for a nick, returning true if it was present.
bool Channel::consumeInvite(const std::string& nick) { std::set<std::string>::iterator it = _invited.find(nick); if (it == _invited.end()) return false; _invited.erase(it); return true; }

// True if invite-only mode (+i) is set.
bool Channel::inviteOnly() const { return _inviteOnly; }
// Set or clear invite-only mode (+i).
void Channel::setInviteOnly(bool b) { _inviteOnly = b; }

// True if topic is restricted to ops (+t).
bool Channel::topicRestricted() const { return _topicRestricted; }
// Set or clear topic restriction (+t).
void Channel::setTopicRestricted(bool b) { _topicRestricted = b; }

// Return the current channel key (+k), or empty if none.
const std::string& Channel::key() const { return _key; }
// Set the channel key (+k).
void Channel::setKey(const std::string& k) { _key = k; }
// Remove the channel key (-k).
void Channel::clearKey() { _key.clear(); }

// Return the user limit (+l), or -1 if unlimited.
int Channel::userLimit() const { return _userLimit; }
// Set the user limit (+l).
void Channel::setUserLimit(int lim) { _userLimit = lim; }
// True if the channel is full (limit reached).
bool Channel::isFull() const { return _userLimit != -1 && (int)_members.size() >= _userLimit; }
