#ifndef FILE_TRANSFER_HPP
#define FILE_TRANSFER_HPP

#include <string>
#include <map>

class Server;
class Client;

struct Transfer {
    int           id;
    int           sender_fd;
    int           receiver_fd;
    std::string   filename;     // original, as provided by sender
    std::string   saved_path;   // server-side path where bytes are stored (uploads/<tid>_<safe>)
    unsigned long size_total;   // declared size (may be 0 or mismatch)
    unsigned long size_seen;    // actually streamed bytes
    bool          accepted;
    bool          active;
    Transfer()
    : id(0), sender_fd(-1), receiver_fd(-1),
      size_total(0), size_seen(0),
      accepted(false), active(false) {}
};

class FileTransfer {
    Server& _srv;
    int     _nextId;
    std::map<int, Transfer> _byId;

public:
    FileTransfer(Server& s);

    // Create an offer; filename is a relative path under the server's CWD (project root).
    int  createOffer(int sender_fd, int receiver_fd, const std::string& filename, unsigned long size_total);

    // Receiver accepts; streaming (read->base64->send) starts automatically here.
    bool accept(int tid, int receiver_fd);

    // Cancel either side before/while active.
    bool cancel(int tid, int who_fd, std::string& reasonOut);

    // Legacy/manual path retained for compatibility (not used when auto-streaming is available)
    bool pushData(int tid, int sender_fd, const std::string& base64, std::string& errOut);
    bool done(int tid, int sender_fd, std::string& errOut);

    // small helpers for encoding/decoding (server uses both)
    static bool b64Decode(const std::string& in, std::string& out);

private:
    // server-side helpers (no errno strings)
    static std::string sanitizeFilename(const std::string& name);
    static void ensureUploadsDir();                 // creates ./uploads if missing (best-effort)
    static bool appendToFile(const std::string& path, const std::string& bytes);
    static bool touchFile(const std::string& path); // create/truncate

    // base64 encode for streaming
    static void b64EncodeChunk(const unsigned char* data, size_t len, std::string& out);

    // crc32 for integrity (C++98-safe)
    static unsigned long crc32_update(unsigned long crc, const unsigned char* buf, size_t len);
    static unsigned long crc32_init();
    static unsigned long crc32_final(unsigned long crc);
};

#endif
