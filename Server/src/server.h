#pragma once
#include "cgullnet/cgull_net.h"
#include "message.h"

class servergull : public cgull::net::server_interface<message_code> {
public:
    using connection_t = cgull::net::connection<message_code>;
    explicit servergull(uint16_t port) : cgull::net::server_interface<message_code>(port) {}

protected:
    bool OnClientConnect(std::shared_ptr<connection_t> client) override {
        std::cout << "[SERVER] Client attempting connection\n";
        return true;
    }

    void OnClientDisconnect(std::shared_ptr<connection_t> client) override {
        std::cout << "[SERVER] Client disconnected ["
            << (client ? client->GetID() : 0) << "]\n";
    }

    void OnMessage(std::shared_ptr<connection_t> client,
        cgull::net::message<message_code>& msg) override {
        switch (msg.header.id) {
        case message_code::HELLO: {
            cgull::net::message<message_code> m;
            m.header.id = message_code::GIVE_PLAYER_ID;
            uint32_t cid = client ? client->GetID() : 0;
            m << cid;
            this->MessageClient(client, m);
            std::cout << "[SERVER] Sent player id " << cid << " to client\n";
            break;
        }

        case message_code::PLAYER_CAT_FIRE: {
            this->MessageAllClients(msg, client);
            std::cout << "[SERVER] Relayed PLAYER_CAT_FIRE from ["
                << (client ? client->GetID() : 0) << "]\n";
            break;
        }

        case message_code::NEW_NOTE: {
            break;
        }

        default:
            std::cout << "[SERVER] Unknown/Unhandled message id\n";
            break;
        }
    }
};

