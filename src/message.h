#pragma once
#include "cgullnet/cgull_net.h"

enum class message_code : uint32_t {
    HELLO,           
    GIVE_PLAYER_ID, 
    PLAYER_CAT_FIRE,
    NEW_NOTE,      
};

