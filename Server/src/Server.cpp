// Headless server entrypoint for Cyber Seagull
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include "server.h"
#include "message.h"

int main(int argc, char** argv) {
    uint16_t port = 1951;
    if (argc > 1) {
        try {
            int p = std::stoi(argv[1]);
            if (p > 0 && p < 65536) port = static_cast<uint16_t>(p);
        }
        catch (...) {
            std::cerr << "[SERVER] Invalid port arg, falling back to 1951\n";
        }
    }

    servergull server(port);
    if (!server.Start()) {
        std::cerr << "[SERVER] Failed to start listener on port " << port << "\n";
        return 1;
    }

    std::cout << "[SERVER] Listening on port " << port << "\n";
    // Run until killed
    while (true) {
        server.Update(-1, true); // block until a message then process
    }

    server.Stop();
    return 0;
}

