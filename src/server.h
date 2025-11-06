#pragma once
#include <cgullnet/cgull_net.h>
#include "message.h"

class servergull : public cgull::net::server_interface<char> {
			U16 num_players = 0;
			public:
			servergull(uint16_t port) : cgull::net::server_interface<char>(port) {}

			protected:
			bool OnClientConnect(std::shared_ptr<cgull::net::connection<char>> client) override {
				std::cout << "Client connected\n";

				cgull::net::message m = (cgull::net::message<char>){{GIVE_PLAYER_ID, message_header_sizes[GIVE_PLAYER_ID]}, (std::vector<U8>){(U8)(num_players & 0xff), (U8)((num_players >> 8) & 0xff)}};
				this->MessageClient(client, m);
				return true;
			}

			void OnClientDisconnect(std::shared_ptr <cgull::net::connection<char>> client) override {
				std::cout << "Client disconnected\n";
			}

			void OnMessage(std::shared_ptr<cgull::net::connection<char>> client, cgull::net::message<char>& msg) override {
				std::cout << "Got message: " << msg << "\n";
			}


};
