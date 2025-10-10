<<<<<<< Updated upstream
// ... existing includes ...
=======
#ifndef SERVER_HPP
#define SERVER_HPP

/**
 * @file Server.hpp
 * @brief Core IRC server event loop and state management.
 *
 * This header declares the Server class, which owns the listening socket,
 * the poll() loop, and all global process state such as connected clients and
 * known channels. It wires together the command handler, bot, and file
 * transfer sub-systems.
 *
 * Design notes:
 * - Single-threaded, non-blocking I/O via poll().
 * - Each connected client has an integer file descriptor (fd) that indexes
 *   into _pfds and _clients.
 * - Channels are looked up by a lower-cased key (IRC channels are case-
 *   insensitive in practice; the project normalizes names).
 * - The server exposes some containers publicly to keep the project simple;
 *   higher-level helpers wrap common operations for safety and clarity.
 */

#include <string>
>>>>>>> Stashed changes
#include <map>
#include <vector>
#include <poll.h>
#include <string>

#include "Bot.hpp"
#include "FileTransfer.hpp"

class Client;
class Channel;
class CommandHandler;
class Bot;
class FileTransfer;

class Server {
    int _listen_fd;
    std::string _password;
    std::string _servername;
    std::map<int, Client*> _clients;
    std::map<std::string, Channel*> _channels;
    std::vector<struct pollfd> _pfds;

    Bot*          _bot;
    FileTransfer* _ft;

public:
    /**
     * @brief Construct and prepare the server instance.
     *
     * The constructor stores configuration (port, server password) but does
     * not start the accept loop. Call run() to start the event loop.
     *
     * @param port     TCP port string to bind the listening socket on.
     * @param password Server password that clients must PASS before
     *                 completing registration.
     */
    Server(const std::string& port, const std::string& password);
    ~Server();

<<<<<<< Updated upstream
    void run();
=======
    /**
     * @brief Get the server's advertised name.
     * @return Immutable reference to the server name used in numerics and
     *         prefixed messages.
     */
    const std::string& serverName() const;
>>>>>>> Stashed changes

    /**
     * @brief Queue a raw IRC line to a single client.
     *
     * This function appends to the client's outgoing buffer. The actual write
     * to the socket occurs in handleClientWrite() when poll() reports the fd
     * is writable.
     *
     * @param fd  Target client's file descriptor.
     * @param msg Full IRC line including any trailing CRLF (or not; the
     *            sender can omit CRLF and the transport layer will ensure
     *            proper framing). The project tolerates either.
     */
    void sendToClient(int fd, const std::string& msg);

    /**
     * @brief Broadcast a message to all members of a channel.
     *
     * @param chan       Channel name (any case). Internally resolved using a
     *                   lower-cased key.
     * @param msg        Full message to deliver (prefix and command already
     *                   prepared by the caller).
     * @param except_fd  A member fd to skip (e.g., echo suppression). Pass -1
     *                   to send to everyone.
     */
    void broadcast(const std::string& chan, const std::string& msg, int except_fd);
<<<<<<< Updated upstream
=======

    /**
     * @brief Send a server-prefixed line that appears to come from a nick.
     *
     * Convenience for bot/system features: builds a prefix using nickFrom and
     * sends the resulting command line to the appropriate destinations.
     *
     * @param nickFrom     Nick to appear as the sender (no user/host part).
     * @param commandLine  The rest of the IRC line (command and params).
     */
    void sendServerAs(const std::string& nickFrom, const std::string& commandLine);

    /**
     * @brief Lookup a channel by name or create it if missing.
     *
     * The new channel is inserted using a normalized lower-case key and its
     * original name preserved for display. The bot is notified on creation.
     *
     * @param name Display or input channel name (e.g., "#general").
     * @return Pointer to an existing or newly created Channel.
     */
>>>>>>> Stashed changes
    Channel* getOrCreateChannel(const std::string& name);

    /**
     * @brief Find a channel by name.
     * @param name Case-insensitive channel name.
     * @return Channel* if found; NULL otherwise.
     */
    Channel* findChannel(const std::string& name);
<<<<<<< Updated upstream
    Client* findClientByNick(const std::string& nick);
    const std::string& serverName() const;

    // helper for the Bot to inject server-validated commands (string must end with \r\n)
    void sendServerAs(const std::string& nickFrom, const std::string& commandLine);
=======

    /**
     * @brief Find a connected Client by nick.
     * @param nick Case-insensitive nickname.
     * @return Client* if found; NULL otherwise.
     */
    Client*  findClientByNick(const std::string& nick);

    /**
     * @brief Enter the poll()-based event loop.
     *
     * This method blocks and continuously:
     * - Accepts new connections
     * - Reads incoming data and parses IRC lines
     * - Dispatches commands to CommandHandler
     * - Writes pending outbound buffers
     * - Performs periodic housekeeping (e.g., bot reminders)
     */
    void run();

    // Helpers used by commands / cleanup
    /**
     * @brief Disconnect and remove a client from all server state.
     *
     * Closes the socket, removes the fd from poll(), leaves channels, and
     * possibly triggers channel cleanup (see maybeDeleteChannel()).
     *
     * @param fd Client file descriptor to remove.
     */
    void removeClient(int fd);

    // ---- new helpers for features/fixes ----
    /**
     * @brief Delete an empty channel if it has no members and no special state.
     * @param lower_key Lower-cased dictionary key for the channel.
     */
    void maybeDeleteChannel(const std::string& lower_key);

    /**
     * @brief Re-grant operator if a channel has no operators left.
     *
     * Some workflows may remove all operators. This helper can promote the
     * first remaining member to op to keep administrative actions possible.
     * @param ch Channel pointer (must be valid).
     */
    void autoReopIfNone(Channel* ch);

    /**
     * @brief Notify and handle state after a member leaves a channel.
     *
     * Updates membership, informs the bot, attempts auto-reop, and triggers
     * deletion if the channel becomes empty.
     *
     * @param ch            Channel pointer (must be valid).
     * @param lower_key     Channel map key (lower-case).
     * @param nickJustLeft  The nickname that left (for logs/bot hooks).
     */
    void onMemberLeftChannel(Channel* ch, const std::string& lower_key, const std::string& nickJustLeft);

    // Exposed state for bot/ft (kept simple for this project)
    /** Map of client fd -> Client*. Owned by Server; cleaned up on remove. */
    std::map<int, Client*>              _clients;
    /** Map of lower(channel) -> Channel*. Owned by Server. */
    std::map<std::string, Channel*>     _channels;
    /** Configured server password (required by PASS). */
    std::string                         _password;
    /** Advertised server name used in numerics/prefixes. */
    std::string                         _servername;
    /** Optional built-in helper bot. Constructed during startup. */
    Bot*                                _bot;
    /** File transfer coordinator for FILE* pseudo-commands. */
    FileTransfer*                       _ft;
>>>>>>> Stashed changes

private:
    /**
     * @brief Create a non-blocking listening socket and bind to the port.
     * @param port Port string (e.g., "6667").
     */
    void setupSocket(const std::string& port);

    /**
     * @brief Add an fd to the pollfd vector with desired events mask.
     */
    void addPollfd(int fd, short events);

    /**
     * @brief Change the events mask for an already-tracked fd.
     */
    void setPollEvents(int fd, short events);
<<<<<<< Updated upstream
    void removeClient(int fd);
=======

    /**
     * @brief Accept a new inbound connection and allocate a Client.
     */
>>>>>>> Stashed changes
    void handleNewConnection();

    /**
     * @brief Read incoming data, accumulate, and dispatch complete lines.
     * @param fd The client fd ready for reading.
     */
    void handleClientRead(int fd);

    /**
     * @brief Attempt to flush the client's outbound buffer to the socket.
     * @param fd The client fd ready for writing.
     */
    void handleClientWrite(int fd);

    /**
     * @brief Close the listening socket and free global resources.
     * Called during orderly shutdown.
     */
    void closeAndCleanup();

    friend class Client;
    friend class CommandHandler;
    friend class Bot;
    friend class FileTransfer;
};
