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
    if (stat("uploads", &st) == 0) return;
#ifdef _WIN32
    _mkdir("uploads");
#else
    mkdir("uploads", 0755);
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

    // prepare server-side path now so we can stream to disk
    const std::string safe = sanitizeFilename(filename);
    char idbuf[32]; std::sprintf(idbuf, "%d", t.id);
    t.saved_path = std::string("uploads/") + idbuf + "_" + safe;

    // create/truncate file early; if we fail we still keep the transfer active,
    // but pushData will fail with a clear message
    (void)touchFile(t.saved_path);

    _byId[t.id]  = t;
    return t.id;
}

bool FileTransfer::accept(int tid, int receiver_fd) {
    std::map<int,Transfer>::iterator it = _byId.find(tid);
    if (it == _byId.end()) return false;
    Transfer& t = it->second;
    if (!t.active || t.receiver_fd != receiver_fd) return false;
    t.accepted = true;
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

bool FileTransfer::pushData(int tid, int sender_fd, const std::string& base64, std::string& errOut) {
    std::map<int,Transfer>::iterator it = _byId.find(tid);
    if (it == _byId.end()) { errOut = "Unknown transfer id"; return false; }
    Transfer& t = it->second;
    if (!t.active) { errOut = "Transfer not active"; return false; }
    if (!t.accepted) { errOut = "Transfer not accepted yet"; return false; }
    if (sender_fd != t.sender_fd) { errOut = "Only sender may push data"; return false; }

    std::string raw;
    if (!b64Decode(base64, raw)) { errOut = "Invalid base64"; return false; }

    // append to server-side file
    if (!appendToFile(t.saved_path, raw)) {
        errOut = "Cannot write to destination";
        return false;
    }

    t.size_seen += (unsigned long)raw.size();

    // forward the SAME base64 chunk to receiver so smart clients can reconstruct
    _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 740 * " + base64 + " \r\n");
    return true;
}

bool FileTransfer::done(int tid, int sender_fd, std::string& errOut) {
    std::map<int,Transfer>::iterator it = _byId.find(tid);
    if (it == _byId.end()) { errOut = "Unknown transfer id"; return false; }
    Transfer& t = it->second;
    if (!t.active) { errOut = "Transfer not active"; return false; }
    if (sender_fd != t.sender_fd) { errOut = "Only sender may finish"; return false; }

    t.active = false;

    // Let both sides know we're done (existing numeric)
    _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 741 * " + t.filename + " :FILE DONE\r\n");
    _srv.sendToClient(t.sender_fd,   ":" + _srv.serverName() + " 741 * " + t.filename + " :FILE DONE\r\n");

    // New: tell both where the server saved the file (C++98 string building)
    char idbuf[32]; std::sprintf(idbuf, "%d", t.id);

    std::ostringstream seen_ss; seen_ss << t.size_seen;
    std::string seen_str = seen_ss.str();

    std::string total_str;
    if (t.size_total) {
        std::ostringstream tot_ss; tot_ss << t.size_total;
        total_str = "/" + tot_ss.str();
    }

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

    return true;
}
