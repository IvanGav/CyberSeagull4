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

    // Not reached
    server.Stop();
    return 0;
}

//void server_send_seagulls() {
//    // assume a global variable `track`; time should be tracked with `track.tick(dt);` every tick
//
//    while (track.next_note_in() < 2.0) {
//        cgull::net::message<message_code> m;
//        m.header.id = message_code::NEW_NOTE;
//
//        F64 timestamp = track.next_note().time;
//        U8 note = track.next_note().note;
//        U8 cannon = track.play_note();
//
//        // Pack in reverse: last thing you want to read goes in first
//        m << timestamp;
//        m << cannon;
//        m << note;
//
//        this->Send(m);
//    }
//}
