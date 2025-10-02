#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>

class Channel {
public:
    explicit Channel(const std::string& name);

    const std::string& name() const;
    const std::string& topic() const;
    void setTopic(const std::string& t);

    bool hasMemberFd(int fd) const;
    void addMember(int fd);
    void removeMember(int fd);
    const std::set<int>& members() const;

    bool isOp(const std::string& nick) const;
    void addOp(const std::string& nick);
    void removeOp(const std::string& nick);
    bool hasAnyOp() const;

    void invite(const std::string& nick);
    bool isInvited(const std::string& nick) const;
    bool consumeInvite(const std::string& nick);

    bool inviteOnly() const;
    void setInviteOnly(bool b);

    bool topicRestricted() const;
    void setTopicRestricted(bool b);

    const std::string& key() const;
    void setKey(const std::string& k);
    void clearKey();

    int  userLimit() const;
    void setUserLimit(int lim);
    bool isFull() const;

    bool   empty() const;
    size_t memberCount() const;

private:
    std::string _name;
    std::string _topic;
    std::set<int>         _members;
    std::set<std::string> _operators;
    std::set<std::string> _invited;
    bool        _inviteOnly;
    bool        _topicRestricted;
    std::string _key;
    int         _userLimit;
};

#endif // CHANNEL_HPP
