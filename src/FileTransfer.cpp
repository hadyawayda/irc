#include "FileTransfer.hpp"
#include "Server.hpp"
#include "Client.hpp"

#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#endif

// ------- base64 decode (existing) -------
static unsigned char ub(unsigned char c){ return c; }

static bool b64_table_ready = false;
static int  b64_table[256];

static void b64_init() {
    if (b64_table_ready) return;
    for (int i=0;i<256;++i) b64_table[i] = -1;
    const std::string a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i=0;i<a.size();++i) b64_table[ ub(a[i]) ] = (int)i;
    b64_table_ready = true;
}

bool FileTransfer::b64Decode(const std::string& in, std::string& out) {
    b64_init();
    out.clear();
    int val=0, valb=-8;
    for (size_t i=0;i<in.size();++i){
        unsigned char c = ub(in[i]);
        if (c=='=') break;
        int d = b64_table[c];
        if (d == -1) continue;
        val = (val<<6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val>>valb)&0xFF));
            valb -= 8;
        }
    }
    return true;
}

// ------- base64 encode (new; minimal) -------
static const char* B64_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void FileTransfer::b64EncodeChunk(const unsigned char* data, size_t len, std::string& out) {
    out.clear();
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= len) {
        unsigned int v = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
        out.push_back(B64_ALPHABET[(v >> 18) & 0x3F]);
        out.push_back(B64_ALPHABET[(v >> 12) & 0x3F]);
        out.push_back(B64_ALPHABET[(v >>  6) & 0x3F]);
        out.push_back(B64_ALPHABET[(v      ) & 0x3F]);
        i += 3;
    }
    size_t rem = len - i;
    if (rem == 1) {
        unsigned int v = (data[i] << 16);
        out.push_back(B64_ALPHABET[(v >> 18) & 0x3F]);
        out.push_back(B64_ALPHABET[(v >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        unsigned int v = (data[i] << 16) | (data[i+1] << 8);
        out.push_back(B64_ALPHABET[(v >> 18) & 0x3F]);
        out.push_back(B64_ALPHABET[(v >> 12) & 0x3F]);
        out.push_back(B64_ALPHABET[(v >>  6) & 0x3F]);
        out.push_back('=');
    }
}

// ------- CRC32 (IEEE 802.3) -------
static bool s_crc_table_ready = false;
static unsigned long s_crc_table[256];

static void crc32_build_table() {
    for (unsigned long i = 0; i < 256; ++i) {
        unsigned long c = i;
        for (int j = 0; j < 8; ++j) {
            if (c & 1) c = 0xEDB88320UL ^ (c >> 1);
            else       c = c >> 1;
        }
        s_crc_table[i] = c;
    }
    s_crc_table_ready = true;
}

unsigned long FileTransfer::crc32_init() {
    if (!s_crc_table_ready) crc32_build_table();
    return 0xFFFFFFFFUL;
}
unsigned long FileTransfer::crc32_update(unsigned long crc, const unsigned char* buf, size_t len) {
    if (!s_crc_table_ready) crc32_build_table();
    for (size_t i = 0; i < len; ++i)
        crc = s_crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}
unsigned long FileTransfer::crc32_final(unsigned long crc) {
    return crc ^ 0xFFFFFFFFUL;
}

// ---- server-side fs helpers (no errno text leaks) ----
std::string FileTransfer::sanitizeFilename(const std::string& name) {
    // strip any path components and allow only [A-Za-z0-9._-]
    std::string base;
    size_t pos = name.find_last_of("/\\");
    base = (pos == std::string::npos) ? name : name.substr(pos + 1);

    std::string out;
    for (size_t i=0;i<base.size();++i) {
        char c = base[i];
        bool ok = (c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='.' || c=='_' || c=='-';
        out.push_back(ok ? c : '_');
    }
    if (out.empty()) out = "file";
    return out;
}

void FileTransfer::ensureUploadsDir() {
    // Best-effort mkdir; ignore return value (no errno usage)
    struct stat st;
#ifdef _WIN32
    const char* dirName = "File Transfers";
#else
    const char* dirName = "File Transfers";
#endif
    if (stat(dirName, &st) == 0) return;
#ifdef _WIN32
    _mkdir(dirName);
#else
    mkdir(dirName, 0755);
#endif
}

bool FileTransfer::appendToFile(const std::string& path, const std::string& bytes) {
    std::FILE* f = std::fopen(path.c_str(), "ab");
    if (!f) return false;
    size_t n = bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return n == bytes.size();
}

bool FileTransfer::touchFile(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

// ---- main API ----
FileTransfer::FileTransfer(Server& s): _srv(s), _nextId(1) {}

int FileTransfer::createOffer(int sender_fd, int receiver_fd, const std::string& filename, unsigned long size_total) {
    ensureUploadsDir();

    Transfer t;
    t.id           = _nextId++;
    t.sender_fd    = sender_fd;
    t.receiver_fd  = receiver_fd;
    t.filename     = filename;
    t.size_total   = size_total;
    t.size_seen    = 0;
    t.accepted     = false;
    t.active       = true;

    // prepare server-side destination path now so we can stream to disk
    const std::string safe = sanitizeFilename(filename);
    char idbuf[32]; std::sprintf(idbuf, "%d", t.id);
    t.saved_path = std::string("File Transfers/") + idbuf + "_" + safe;

    (void)touchFile(t.saved_path); // best effort

    _byId[t.id]  = t;
    return t.id;
}

bool FileTransfer::accept(int tid, int receiver_fd) {
    std::map<int,Transfer>::iterator it = _byId.find(tid);
    if (it == _byId.end()) return false;
    Transfer& t = it->second;
    if (!t.active || t.receiver_fd != receiver_fd) return false;

    t.accepted = true;

    // ----- Auto-stream from project root to receiver as 740 base64, and copy to "File Transfers" -----
    // Open source file (relative to server CWD)
    std::string safeSrc = sanitizeFilename(t.filename);
    std::FILE* src = std::fopen(safeSrc.c_str(), "rb");
    if (!src) {
        // can't open source: notify both ends, then cancel
        _srv.sendToClient(t.sender_fd,   ":" + _srv.serverName() + " 400 * " + std::string("1") + " :Cannot open source\r\n");
        _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 400 * " + std::string("1") + " :Cannot open source\r\n");
        t.active = false;
        return false;
    }

    // Let both sides know streaming is starting
    _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 746 * " + std::string("STREAM") + " :BEGIN " + safeSrc + "\r\n");
    _srv.sendToClient(t.sender_fd,   ":" + _srv.serverName() + " 746 * " + std::string("STREAM") + " :BEGIN " + safeSrc + "\r\n");

    const size_t RAW_CHUNK = 450; // ~600 chars in base64; safe for IRC line length
    std::vector<unsigned char> buf;
    buf.resize(RAW_CHUNK);

    unsigned long crc = crc32_init();

    for (;;) {
        size_t n = std::fread(&buf[0], 1, RAW_CHUNK, src);
        if (n == 0) break;

        // update CRC & size
        crc = crc32_update(crc, &buf[0], n);
        t.size_seen += (unsigned long)n;

        // append to server-side file
        std::string raw_str; raw_str.assign((const char*)&buf[0], (size_t)n);
        if (!appendToFile(t.saved_path, raw_str)) {
            _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 400 * " + std::string("1") + " :Cannot write to destination\r\n");
            _srv.sendToClient(t.sender_fd,   ":" + _srv.serverName() + " 400 * " + std::string("1") + " :Cannot write to destination\r\n");
            std::fclose(src);
            t.active = false;
            return false;
        }

        // base64 encode and forward chunk
        std::string b64;
        b64EncodeChunk(&buf[0], n, b64);
        _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 740 * " + b64 + " \r\n");
    }

    std::fclose(src);

    // Done
    t.active = false;

    // Existing numeric 741: FILE DONE
    _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 741 * " + sanitizeFilename(t.filename) + " :FILE DONE\r\n");
    _srv.sendToClient(t.sender_fd,   ":" + _srv.serverName() + " 741 * " + sanitizeFilename(t.filename) + " :FILE DONE\r\n");

    // 744: where the server saved it and sizes
    char idbuf[32]; std::sprintf(idbuf, "%d", t.id);

    std::ostringstream seen_ss; seen_ss << t.size_seen;
    std::string seen_str = seen_ss.str();

    std::string total_str;
    if (t.size_total) { std::ostringstream tot_ss; tot_ss << t.size_total; total_str = "/" + tot_ss.str(); }

    std::string size_pair = "(" + seen_str + total_str + ")";

    _srv.sendToClient(
        t.receiver_fd,
        ":" + _srv.serverName() + " 744 * " + std::string(idbuf) +
        " :SAVED " + t.saved_path + " " + size_pair + "\r\n"
    );
    _srv.sendToClient(
        t.sender_fd,
        ":" + _srv.serverName() + " 744 * " + std::string(idbuf) +
        " :SAVED " + t.saved_path + " " + size_pair + "\r\n"
    );

    // 745: CRC32 hex
    unsigned long crc_final = crc32_final(crc);
    char crcbuf[16]; std::sprintf(crcbuf, "%08lX", crc_final);
    _srv.sendToClient(
        t.receiver_fd,
        ":" + _srv.serverName() + " 745 * " + std::string(idbuf) + " :HASH CRC32 " + std::string(crcbuf) + "\r\n"
    );
    _srv.sendToClient(
        t.sender_fd,
        ":" + _srv.serverName() + " 745 * " + std::string(idbuf) + " :HASH CRC32 " + std::string(crcbuf) + "\r\n"
    );

    return true;
}

bool FileTransfer::cancel(int tid, int who_fd, std::string& reasonOut) {
    std::map<int,Transfer>::iterator it = _byId.find(tid);
    if (it == _byId.end()) return false;
    Transfer& t = it->second;
    if (!t.active) return false;
    if (who_fd != t.sender_fd && who_fd != t.receiver_fd) return false;
    reasonOut = (who_fd == t.sender_fd ? "Sender cancelled" : "Receiver cancelled");
    t.active = false;
    return true;
}

// ----- Manual/legacy mode (still supported) -----
bool FileTransfer::pushData(int tid, int sender_fd, const std::string& base64, std::string& errOut) {
    std::map<int,Transfer>::iterator it = _byId.find(tid);
    if (it == _byId.end()) { errOut = "Unknown transfer id"; return false; }
    Transfer& t = it->second;
    if (!t.active) { errOut = "Transfer not active"; return false; }
    if (!t.accepted) { errOut = "Transfer not accepted yet"; return false; }
    if (sender_fd != t.sender_fd) { errOut = "Only sender may push data"; return false; }

    std::string raw; if (!b64Decode(base64, raw)) { errOut = "Invalid base64"; return false; }
    t.size_seen += (unsigned long)raw.size();
    // forward the same base64 chunk for clients that expect data this way
    _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 740 * " + base64 + " \r\n");
    // also append to file if path ready
    appendToFile(t.saved_path, raw);
    return true;
}

bool FileTransfer::done(int tid, int sender_fd, std::string& errOut) {
    std::map<int,Transfer>::iterator it = _byId.find(tid);
    if (it == _byId.end()) { errOut = "Unknown transfer id"; return false; }
    Transfer& t = it->second;
    if (!t.active) { errOut = "Transfer not active"; return false; }
    if (sender_fd != t.sender_fd) { errOut = "Only sender may finish"; return false; }
    t.active = false;

    _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 741 * " + t.filename + " :FILE DONE\r\n");
    _srv.sendToClient(t.sender_fd,   ":" + _srv.serverName() + " 741 * " + t.filename + " :FILE DONE\r\n");

    // we don't recompute CRC here in legacy mode
    return true;
}
