#include "FileTransfer.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include <vector>

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

FileTransfer::FileTransfer(Server& s): _srv(s), _nextId(1) {}

int FileTransfer::createOffer(int sender_fd, int receiver_fd, const std::string& filename, unsigned long size_total) {
    Transfer t;
    t.id = _nextId++;
    t.sender_fd = sender_fd;
    t.receiver_fd = receiver_fd;
    t.filename = filename;
    t.size_total = size_total;
    t.size_seen  = 0;
    t.accepted   = false;
    t.active     = true;
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

    std::string raw; if (!b64Decode(base64, raw)) { errOut = "Invalid base64"; return false; }
    t.size_seen += (unsigned long)raw.size();
    // forward chunk (server relays bytes in NOTICE wrapper so it stays IRC-safe)
    // We wrap as: :server FILEDATA <tid> <chunk-bytes> (raw is binary; wrap into base64 again for receiver)
    // But receiver already expects base64? Keep symmetry: the server forwards the *same* base64 chunk.
    // Send to receiver as numeric 740 + chunk:
    _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 740 * " + base64 + " \r\n");
    return true;
}

bool FileTransfer::done(int tid, int sender_fd, std::string& errOut) {
    std::map<int,Transfer>::iterator it = _byId.find(tid);
    if (it == _byId.end()) { errOut = "Unknown transfer id"; return false; }
    Transfer& t = it->second;
    if (!t.active) { errOut = "Transfer not active"; return false; }
    if (sender_fd != t.sender_fd) { errOut = "Only sender may finish"; return false; }
    if (t.size_total && t.size_seen != t.size_total) {
        // allow mismatch but warn
    }
    t.active = false;
    _srv.sendToClient(t.receiver_fd, ":" + _srv.serverName() + " 741 * " + t.filename + " :FILE DONE\r\n");
    _srv.sendToClient(t.sender_fd,   ":" + _srv.serverName() + " 741 * " + t.filename + " :FILE DONE\r\n");
    return true;
}
