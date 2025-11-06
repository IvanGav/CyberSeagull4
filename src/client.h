#pragma once
#include <cgullnet/cgull_net.h>
#include "message.h"
extern U16 player_id;

void throw_cat(int, bool, F64);
class seaclient : public cgull::net::client_interface<char> {
	public:
	void send_message(message_code mc, std::vector<uint8_t> msg) {
		cgull::net::message<char> m;
		m.header = { (char)mc, (U32)msg.size() };
		m.body = msg;
		this->Send(m);
	}

	void check_messages() {
		while (!this->Incoming().empty()) {
			handle_message(this->Incoming().pop_front());
		}
	}


	void handle_message(const cgull::net::owned_message<char>& msg) {
		std::cout << "msg\n";
		switch (msg.msg.header.id) {
			case GIVE_PLAYER_ID: {
				int index = 0;
				player_id = message_read_u16(msg.msg, index);
				}
				break;
			case PLAYER_CAT_FIRE: {
					int index = 0;
					U16 fired_player_id = message_read_u16(msg.msg, index);
					if (fired_player_id == player_id) return;
					F64 timestamp = message_read_f64(msg.msg, index);
					std::cout << "throwing cat: ";
					for (; index < msg.msg.size(); index++) {
						std::cout << (int)msg.msg.body[index] << "  ";
						throw_cat((int)msg.msg.body[index], false, timestamp);
					}
					std::cout << "\n";
				}
				break;
		}
	}
};
