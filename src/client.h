#pragma once
#include <cgullnet/cgull_net.h>
#include "message.h"
extern U16 player_id;

void throw_cat(int, bool);
class seaclient : public cgull::net::client_interface<char> {
	public:
	void send_message(message_code mc, std::vector<uint8_t> msg) {
		cgull::net::message m = (cgull::net::message<char>){{mc, (U32)msg.size()}, msg};
		this->Send(m);
	}

	void check_messages() {
		while (!this->Incoming().empty()) {
			handle_message(this->Incoming().pop_front());
		}
	}


	void handle_message(const cgull::net::owned_message<char>& msg) {
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
					for (; index < msg.msg.size();) {
						throw_cat(msg.msg.body[index++], false);
					}
				}
				break;
		}
	}
};
