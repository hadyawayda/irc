#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

#include <string>
#include <vector>

class Server;
class Client;

class CommandHandler {
    Server& _srv;
public:
    CommandHandler(Server& s): _srv(s) {}
    void handleLine(Client& c, const std::string& line);

private:
    void cmdPASS(Client&, const std::vector<std::string>&);
    void cmdNICK(Client&, const std::vector<std::string>&);
    void cmdUSER(Client&, const std::vector<std::string>&, const std::string& trailing);
    void cmdPING(Client&, const std::vector<std::string>&);
    void cmdPONG(Client&, const std::vector<std::string>&);
    void cmdPRIVMSG(Client&, const std::vector<std::string>&, const std::string& trailing);
    void cmdJOIN(Client&, const std::vector<std::string>&);
    void cmdPART(Client&, const std::vector<std::string>&);
    void cmdQUIT(Client&, const std::vector<std::string>&, const std::string& trailing);
    void cmdTOPIC(Client&, const std::vector<std::string>&, const std::string& trailing);
    void cmdMODE(Client&, const std::vector<std::string>&);
    void cmdINVITE(Client&, const std::vector<std::string>&);
    void cmdKICK(Client&, const std::vector<std::string>&, const std::string& trailing);

    void sendNumeric(Client&, const std::string& code, const std::string& msg);
};

#endif
