// Headless server entrypoint for Cyber Seagull
// Starts the cgull server and relays gameplay messages.
// Build as its own executable.

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
            std::cout << "[SERVER] Invalid port arg, using default 1951\n";
        }
    }

    servergull server(port);
    if (!server.Start()) {
        std::cerr << "[SERVER] Failed to start on port " << port << "\n";
        return 1;
    }

    std::cout << "[SERVER] Listening on port " << port << "\n";
    // Run until stopped (Ctrl+C)
    while (true) {
        server.Update(); // processes incoming/outgoing messages
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Not reached
    server.Stop();
    return 0;
}
