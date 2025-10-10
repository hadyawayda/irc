#ifndef FILE_TRANSFER_HPP
#define FILE_TRANSFER_HPP

/**
 * @file FileTransfer.hpp
 * @brief Simple server-mediated file transfer facility.
 *
 * The server coordinates a file transfer between two connected clients using
 * custom IRC-like commands (FILESEND/FILEACCEPT/FILEDATA/FILEDONE/FILECANCEL).
 * Data is stored server-side under a dedicated folder and optionally streamed
 * between clients when accepted.
 */

#include <string>
#include <map>

class Server;
class Client;

/**
 * @brief State of a single file transfer session.
 */
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
    /**
     * @brief Construct the file transfer coordinator bound to a Server.
     */
    FileTransfer(Server& s);
<<<<<<< Updated upstream
    int  createOffer(int sender_fd, int receiver_fd, const std::string& filename, unsigned long size_total);
    bool accept(int tid, int receiver_fd);
    bool cancel(int tid, int who_fd, std::string& reasonOut);
    // data is base64; will be decoded and forwarded *as-is* as bytes in chunks of up to chunkMax
=======

    // Create an offer; filename is a relative path under the server's CWD (project root).
    /**
     * @brief Create a file transfer offer record.
     * @param sender_fd    Sender client fd
     * @param receiver_fd  Receiver client fd
     * @param filename     File name as provided by sender (sanitized internally)
     * @param size_total   Declared total size (optional; 0 if unknown)
     * @return A unique transfer id (tid) > 0
     */
    int  createOffer(int sender_fd, int receiver_fd, const std::string& filename, unsigned long size_total);

    // Receiver accepts; streaming (read->base64->send) starts automatically here.
    /**
     * @brief Mark an offer as accepted and initiate streaming if supported.
     * @param tid         Transfer id
     * @param receiver_fd Receiver client fd confirming the accept
     * @return true on success; false if invalid state or mismatched receiver
     */
    bool accept(int tid, int receiver_fd);

    // Cancel either side before/while active.
    /**
     * @brief Cancel a transfer from either participant.
     * @param tid      Transfer id
     * @param who_fd   Caller fd (sender or receiver)
     * @param reasonOut Human-readable reason for logging
     * @return true if the transfer existed and is now canceled
     */
    bool cancel(int tid, int who_fd, std::string& reasonOut);

    // Legacy/manual path retained for compatibility (not used when auto-streaming is available)
    /** Append a base64-encoded data chunk to the transfer's file. */
>>>>>>> Stashed changes
    bool pushData(int tid, int sender_fd, const std::string& base64, std::string& errOut);
    /** Mark the transfer as complete after all data has been sent. */
    bool done(int tid, int sender_fd, std::string& errOut);

<<<<<<< Updated upstream
    // small helpers for encoding/decoding
    static bool b64Decode(const std::string& in, std::string& out);
=======
    // small helpers for encoding/decoding (server uses both)
    /** Base64 decode utility (no newlines required). */
    static bool b64Decode(const std::string& in, std::string& out);

private:
    // server-side helpers (no errno strings)
    /** Sanitize a filename for safe storage on disk. */
    static std::string sanitizeFilename(const std::string& name);
    /** Ensure the uploads directory exists (best-effort). */
    static void ensureUploadsDir();                 // creates ./"File Transfers" if missing (best-effort)
    /** Append raw bytes to a file path; returns false on I/O error. */
    static bool appendToFile(const std::string& path, const std::string& bytes);
    /** Create/truncate the destination file to prepare for streaming. */
    static bool touchFile(const std::string& path); // create/truncate

    // base64 encode for streaming
    /** Base64-encode a small chunk; appends to 'out'. */
    static void b64EncodeChunk(const unsigned char* data, size_t len, std::string& out);

    // crc32 for integrity (C++98-safe)
    /** CRC32 helpers to compute a checksum incrementally. */
    static unsigned long crc32_update(unsigned long crc, const unsigned char* buf, size_t len);
    static unsigned long crc32_init();
    static unsigned long crc32_final(unsigned long crc);
>>>>>>> Stashed changes
};

#endif
