#pragma once
#include <cgullnet/cgull_net.h>

//// platform dependent header size in networking code :p
U32 message_header_sizes[] = {sizeof(U16), 0, sizeof(U8) + sizeof(double)};

enum message_code {
	GIVE_PLAYER_ID,	/// Player id
	PLAYER_CAT_FIRE,	/// Player id, cat id, timestep
	NEW_NOTE,			/// note value / cat id, timestep
};

U16 message_read_u16(const cgull::net::message<char>& msg, int& index) {
	U16 out = msg.body[index++];
	out |= (msg.body[index++] << 8) & 0xff00;
	return out;
}

F64 message_read_f64(const cgull::net::message<char>& msg, int& index) {
	U64 out = msg.body[index++];
	for (int i = 1; i < 8; i++) {
		out |= (msg.body[index++] << (8 * i));
	}
	return *(F64*)&out;
}
