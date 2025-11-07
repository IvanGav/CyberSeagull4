#pragma once
#include <iostream>
#include <cstdint>
#include "cgullnet/cgull_net.h"
#include "message.h"
#include "util.h"

class servergull : public cgull::net::server_interface<message_code> {
public:
    using connection_t = cgull::net::connection<message_code>;
    explicit servergull(uint16_t port) : cgull::net::server_interface<message_code>(port) {}

protected:
    bool OnClientConnect(std::shared_ptr<connection_t> client) override {
        std::cout << "[SERVER] Client attempting connection\n";
        return true; // accept all clients
    }

    void OnClientDisconnect(std::shared_ptr<connection_t> client) override {
        std::cout << "[SERVER] Client [" << (client ? client->GetID() : 0) << "] disconnected\n";
    }

    void OnMessage(std::shared_ptr<connection_t> client, cgull::net::message<message_code>& msg) override {
        using cgull::net::message;

        switch (msg.header.id) {
        case message_code::HELLO: {
            std::cout << "[SERVER] HELLO from client [" << (client ? client->GetID() : 0) << "]\n";
            message<message_code> reply;
            reply.header.id = message_code::GIVE_PLAYER_ID;
            U16 assigned = NextPlayerId();
            reply << assigned;
            MessageClient(client, reply);
            break;
        }

        case message_code::PLAYER_CAT_FIRE: {
            // Fan-out to all clients, including sender
            MessageAllClients(msg);
            break;
        }

        case message_code::NEW_NOTE: {
            MessageAllClients(msg);
            break;
        }

        case message_code::SONG_START: {
            MessageAllClients(msg);
            break;
        }

        default:
            std::cout << "[SERVER] Unknown/Unhandled message id\n";
            break;
        }
    }

private:
    U16 NextPlayerId() {
        // Very simple allocator, wraps around if needed
        return ++last_player_id_;
    }

    U16 last_player_id_ = 0;
};
