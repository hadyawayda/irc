#ifndef CLIENT_HPP
#define CLIENT_HPP

/**
 * @file Client.hpp
 * @brief Representation of a single connected IRC client.
 *
 * Client objects track registration state (PASS/NICK/USER), identity fields
 * (nick, user, real), per-fd input/output buffers, and the set of joined
 * channel names. The Server owns Client instances and manages their lifetime.
 */

#include <string>
#include <set>

class Server;

class Client {
    int _fd;
    bool _registered;
    bool _pass_ok;
    std::string _nick, _user, _real;
    std::string _inbuf, _outbuf;
    std::set<std::string> _channels; // lowercased names

public:
    /**
     * @brief Construct a client wrapper for a newly accepted fd.
     * @param fd Non-blocking socket file descriptor.
     */
    Client(int fd);
    ~Client();

    /** @return The client's socket fd. */
    int fd() const;
    /** @return Current nickname (may be empty before registration). */
    const std::string& nick() const;
    /** @return USER field. */
    const std::string& user() const;
    /** @return Real name (gecos). */
    const std::string& real() const;
    /** @return true if NICK/USER completed and PASS (if required) succeeded. */
    bool isRegistered() const;
    /** @return true if PASS <password> matched server policy. */
    bool passOk() const;

    /** @brief Set PASS result; used by CommandHandler PASS. */
    void setPassOk(bool v);
    /** @brief Update nickname; validation is done in CommandHandler. */
    void setNick(const std::string& n);
    /** @brief Set USER/REAL fields. */
    void setUser(const std::string& u, const std::string& r);
    /**
     * @brief Attempt to mark the client registered if preconditions hold.
     *
     * Invoked after PASS/NICK/USER updates. Sends welcome numerics when the
     * client transitions into the registered state for the first time.
     */
    void tryRegister(Server& s);

    /** @return Mutable reference to the input accumulation buffer. */
    std::string& inbuf();
    /** @return Mutable reference to the output (pending send) buffer. */
    std::string& outbuf();

    /** @return Set of lower-cased channel names the client has joined. */
    const std::set<std::string>& channels() const;
    /** @brief Track that the client joined a channel (lower-case name). */
    void joinChannel(const std::string& name);
    /** @brief Track that the client left a channel (lower-case name). */
    void leaveChannel(const std::string& name);
};

#endif
