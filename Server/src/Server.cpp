// Server.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include "server.h"
#include "message.h"
#include <libremidi/libremidi.hpp>


int main(int argc, char** argv) {
    uint16_t port = 1951;
    std::string midi_path = "asset/Buddy Holly riff.mid"; // default demo

    if (argc > 1) {
        try {
            int p = std::stoi(argv[1]);
            if (p > 0 && p < 65536) port = static_cast<uint16_t>(p);
        }
        catch (...) {
            std::cerr << "[SERVER] Invalid port arg, falling back to 1951\n";
        }
    }
    if (argc > 2) {
        midi_path = argv[2];
    }

    servergull server(port);
    if (!server.Start()) {
        std::cerr << "[SERVER] Failed to start listener on port " << port << "\n";
        return 1;
    }

    if (!server.LoadSong(midi_path)) {
        std::cerr << "[SERVER] Failed to load MIDI '" << midi_path << "'\n";
        return 2;
    }

    // Start the song (broadcast schedule and begin damage checkin)
    server.StartSong();

    std::cout << "[SERVER] Listening on port " << port << "\n";
    while (true) {
        server.Update(-1, true); // process network messages as they arrive
    }

    server.Stop();
    return 0;
}
