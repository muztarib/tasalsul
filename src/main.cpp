#include <iostream>
#include <string>
#include "tsdb.hpp"
#include "../include/server.hpp"

// Entry Point
// The actual server implementation lives in src/server.cpp.

int main(int argc, char** argv) {
    int port = 5555; // default port
    bool strict_ts = true; // default per requirement
    // Simple, forgiving CLI: --port <num> and --strict_ts <true|false>
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[i + 1]);
            i++;
        } else if (arg == "--strict_ts" && i + 1 < argc) {
            std::string val = argv[i + 1];
            if (val == "true" || val == "1" || val == "yes" ) {
                strict_ts = true;
            } else if (val == "false" || val == "0" || val == "no") {
                strict_ts = false;
            }
            i++;
        } else if (argc == 2) {
            // allow: ./tsdb 5555
            port = std::stoi(argv[1]);
        }
    }
    // Start the Phase 1 server with strict_ts flag
    start_server(port, strict_ts);
    return 0;
}
