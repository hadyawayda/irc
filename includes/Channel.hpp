#ifndef CHANNEL_HPP
#define CHANNEL_HPP

/**
 * @file Channel.hpp
 * @brief In-memory representation of an IRC channel and its modes.
 *
 * The Channel encapsulates member tracking (by client fd), operator list
 * (by nick, lower-cased), optional invite list, and a small subset of IRC
 * modes commonly seen in school projects:
 * - i: invite-only
 * - t: topic settable by ops only
 * - k: channel key (password)
 * - l: user limit (max members)
 *
 * The server stores channels in a case-insensitive map by lower-case key,
 * while preserving the original name for display.
 */

#include <string>
#include <set>

class Channel {
    std::string _name;
    std::string _topic;
    std::set<int> _members;                 // client fds
    std::set<std::string> _operators;       // nicks
    std::set<std::string> _invited;         // nicks
    bool _inviteOnly;
    bool _topicRestricted;
    std::string _key;
    int _userLimit; // -1 = none

public:
    Channel(const std::string& name);

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
<<<<<<< Updated upstream
=======
    /** @brief True if the channel has at least one operator. */
    bool hasAnyOp() const;                 // NEW
>>>>>>> Stashed changes

    void invite(const std::string& nick);
    bool isInvited(const std::string& nick) const;
    bool consumeInvite(const std::string& nick);

    /** @return Whether the channel is invite-only (+i). */
    bool inviteOnly() const;
    /** @brief Enable or disable invite-only mode (+i). */
    void setInviteOnly(bool b);

    /** @return Whether the topic is restricted to ops (+t). */
    bool topicRestricted() const;
    /** @brief Enable or disable topic restriction (+t). */
    void setTopicRestricted(bool b);

    /** @return Current channel key (+k), or empty if no key set. */
    const std::string& key() const;
    /** @brief Set/replace the channel key (+k). */
    void setKey(const std::string& k);
    /** @brief Remove the channel key (-k). */
    void clearKey();

<<<<<<< Updated upstream
    int userLimit() const;
=======
    /** @return Current user limit (+l), or 0 if unlimited. */
    int  userLimit() const;
    /** @brief Set the user limit (+l), clamped to non-negative values. */
>>>>>>> Stashed changes
    void setUserLimit(int lim);
    /** @return True if userLimit() > 0 and members().size() >= limit. */
    bool isFull() const;
<<<<<<< Updated upstream
=======

private:
    std::string         _name;
    std::string         _topic;
    std::set<int>         _members;     //!< Joined members (by client fd)
    std::set<std::string> _operators;   //!< Operator nicks (lower-case)
    std::set<std::string> _invited;     //!< One-time invite tokens (lower-case)
    bool                _inviteOnly;
    bool                _topicRestricted;
    std::string         _key;
    int                 _userLimit;
>>>>>>> Stashed changes
};

#endif
