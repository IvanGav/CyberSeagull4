#pragma once
#include <cgullnet/cgull_net.h>

class servergull : public cgull::net::server_interface<char> {
			public:
			servergull(uint16_t port) : cgull::net::server_interface<char>(port) {}

			protected:
			bool OnClientConnect(std::shared_ptr<cgull::net::connection<char>> client) override {
				std::cout << "Client connected\n";
				return true;
			}

			void OnClientDisconnect(std::shared_ptr <cgull::net::connection<char>> client) override {
				std::cout << "Client disconnected\n";
			}

			void OnMessage(std::shared_ptr<cgull::net::connection<char>> client, cgull::net::message<char>& msg) override {
				std::cout << "Got message: " << msg << "\n";
			}


};
