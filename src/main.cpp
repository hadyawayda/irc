#include "Server.hpp"
#include <iostream>
#include <cstdlib>

static bool is_number(const char* s) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; ++p) if (*p < '0' || *p > '9') return false;
    return true;
}

int main(int ac, char** av) {
    if (ac != 3 || !is_number(av[1])) {
        std::cerr << "Usage: " << av[0] << " <port> <password>\n";
        return 1;
    }
    try {
        Server s(av[1], av[2]);
        s.run();
    } catch (...) {
        std::cerr << "Fatal error\n";
        return 1;
    }
    return 0;
}
