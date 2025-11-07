#pragma once
#include "util.h"        
enum class message_code : U32 {
    HELLO,
    GIVE_PLAYER_ID,   // Player id
    PLAYER_CAT_FIRE,  // Player id, cat id, timestep
    NEW_NOTE,         // note value, cat id, timestep
    SONG_START,       // no data
};

