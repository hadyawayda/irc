#ifndef FILE_TRANSFER_HPP
#define FILE_TRANSFER_HPP

#include <string>
#include <map>

class Server;
class Client;

struct Transfer {
    int          id;
    int          sender_fd;
    int          receiver_fd;
    std::string  filename;
    unsigned long size_total;
    unsigned long size_seen;
    bool         accepted;
    bool         active;
    Transfer(): id(0), sender_fd(-1), receiver_fd(-1), size_total(0), size_seen(0), accepted(false), active(false) {}
};

class FileTransfer {
    Server& _srv;
    int     _nextId;
    std::map<int, Transfer> _byId;
public:
    FileTransfer(Server& s);
    int  createOffer(int sender_fd, int receiver_fd, const std::string& filename, unsigned long size_total);
    bool accept(int tid, int receiver_fd);
    bool cancel(int tid, int who_fd, std::string& reasonOut);
    // data is base64; will be decoded and forwarded *as-is* as bytes in chunks of up to chunkMax
    bool pushData(int tid, int sender_fd, const std::string& base64, std::string& errOut);
    bool done(int tid, int sender_fd, std::string& errOut);

    // small helpers for encoding/decoding
    static bool b64Decode(const std::string& in, std::string& out);
};

#endif
