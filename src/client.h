#pragma once
#include <cgullnet/cgull_net.h>
#include "message.h"
extern U16 player_id;
extern F64 cur_time_sec;
extern std::vector<Entity> objects;
void make_seagull(U8 cannon, F64 timestamp);

F64 song_start_time;
F64 song_spb = .20; // seconds/beat
static constexpr U8 SHOW_NUM_BEATS = 4;
static constexpr F64 SEAGULL_MOVE_PER_BEAT = 30;


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
						throw_cat((int)msg.msg.body[index], false, song_start_time + timestamp);
					}
					std::cout << "\n";
				}
				break;
			case NEW_NOTE: {
				int index = 0;
				U8 note = msg.msg.body[index++];
				U8 cannon = msg.msg.body[index++];
				F64 timestamp = message_read_f64(msg.msg, index);

				make_seagull(cannon, timestamp);
			}
			break;
			case SONG_START: {
				song_spb = 1;
				song_start_time = cur_time_sec;
			}
			break;
		}
	}
};
