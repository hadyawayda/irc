#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

/**
 * @file CommandHandler.hpp
 * @brief IRC command parsing and dispatching.
 *
 * CommandHandler transforms a parsed IRC line into server-side actions. It
 * assumes splitCmd() has separated the command, params, and trailing text.
 * Validation and numeric error replies are emitted from here.
 */

#include <string>
#include <vector>

class Server;
class Client;

class CommandHandler {
    Server& _srv;
public:
    CommandHandler(Server& s): _srv(s) {}
    /**
     * @brief Parse and execute a single IRC message line for a client.
     * @param c      The client issuing the command.
     * @param line   The raw line without CRLF. The method will extract the
     *               command token and route to the corresponding handler.
     */
    void handleLine(Client& c, const std::string& line);

private:
    /** Handle PASS <password> */
    void cmdPASS(Client&, const std::vector<std::string>&);
    /** Handle NICK <nickname> */
    void cmdNICK(Client&, const std::vector<std::string>&);
    /** Handle USER <user> <mode> * :<realname> */
    void cmdUSER(Client&, const std::vector<std::string>&, const std::string& trailing);
    /** Handle PING <token> */
    void cmdPING(Client&, const std::vector<std::string>&);
    /** Handle PONG <token> */
    void cmdPONG(Client&, const std::vector<std::string>&);
    /** Handle PRIVMSG <target> :<text> (user or channel) */
    void cmdPRIVMSG(Client&, const std::vector<std::string>&, const std::string& trailing);
    /** Handle JOIN <chans> [<keys>] */
    void cmdJOIN(Client&, const std::vector<std::string>&);
<<<<<<< Updated upstream
    void cmdPART(Client&, const std::vector<std::string>&);
=======
    /** Handle PART <chans> [:<reason>] */
    void cmdPART(Client&, const std::vector<std::string>&, const std::string& trailing);
    /** Handle QUIT [:<message>] */
>>>>>>> Stashed changes
    void cmdQUIT(Client&, const std::vector<std::string>&, const std::string& trailing);
    /** Handle TOPIC <chan> [:<topic>] honoring +t mode */
    void cmdTOPIC(Client&, const std::vector<std::string>&, const std::string& trailing);
    /** Handle MODE <chan> [[+|-]itkl <args>] */
    void cmdMODE(Client&, const std::vector<std::string>&);
    /** Handle INVITE <nick> <chan> */
    void cmdINVITE(Client&, const std::vector<std::string>&);
    /** Handle KICK <chan> <nick> [:<reason>] */
    void cmdKICK(Client&, const std::vector<std::string>&, const std::string& trailing);
    /** Begin a file transfer offer (custom extension) */
    void cmdFILESEND(Client&, const std::vector<std::string>&, const std::string& trailing);
    /** Accept a previously offered file transfer (custom extension) */
    void cmdFILEACCEPT(Client&, const std::vector<std::string>&);
    /** Stream data chunk for a transfer (custom extension) */
    void cmdFILEDATA(Client&, const std::vector<std::string>&);
    /** Mark end-of-stream for a transfer (custom extension) */
    void cmdFILEDONE(Client&, const std::vector<std::string>&);
    /** Cancel a transfer (custom extension) */
    void cmdFILECANCEL(Client&, const std::vector<std::string>&);

    /**
     * @brief Send a numeric reply to a client.
     * @param code Numeric as a string (e.g., "401").
     * @param msg  Preformatted message text after the code.
     */
    void sendNumeric(Client&, const std::string& code, const std::string& msg);
    
    /**
     * @brief Ensure the client is fully registered before running a command.
     * @param c      Client reference.
     * @param forCmd Command name for error reporting.
     * @return false if not registered (and an error was sent), true otherwise.
     */
    bool requireRegistered(Client& c, const char* forCmd);
};

#endif
