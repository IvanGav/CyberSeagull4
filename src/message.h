#pragma once
#include <cgullnet/cgull_net.h>

enum class message_code : U32 {
	HELLO,
	GIVE_PLAYER_ID,	/// Player id
	PLAYER_CAT_FIRE,	/// Player id, cat id, timestep
	NEW_NOTE,			/// note value, cat id, timestep
	SONG_START,		/// no data
	HEALTH_UPDATE, // p1_id, p1_hp, p2_id, p2_hp
	GAME_OVER,		// winner_id
	PLAYER_READY, 
	LOBBY_STATE
};
