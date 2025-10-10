//
// main.cpp — Program entry point
//
// This file initializes the IRC server using the provided command-line
// arguments and starts its event loop. It performs minimal validation of the
// port argument and prints a usage message on error.
//
#include "Server.hpp"
#include <iostream>
#include <cstdlib>

/**
 * @brief Return true if the C-string consists only of decimal digits.
 * Used to validate the <port> argument before attempting to bind.
 */
static bool is_number(const char* s) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; ++p) if (*p < '0' || *p > '9') return false;
    return true;
}

/**
 * @brief Entry point: parse arguments, construct Server, and run.
 *
 * Expected usage: ./ircserv <port> <password>
 * - <port> must be numeric (e.g., 6667)
 * - <password> is required and will be checked by PASS
 *
 * The server runs until terminated. Fatal exceptions produce a brief error.
 */
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
