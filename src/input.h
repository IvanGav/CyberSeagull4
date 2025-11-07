#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "message.h"

void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch);
F32 weezer_notes[] = { 0.79367809502, 0.89087642854, 1.f, 1.05943508007, 1.33482398807, 1.4982991247 };
F32 weezer[] = { weezer_notes[2], weezer_notes[3], weezer_notes[2], weezer_notes[4], weezer_notes[5], weezer_notes[4], weezer_notes[2], weezer_notes[1], weezer_notes[0]/*, 1.f*/};
I32 weezer_index = 0;
constexpr U16 numcats = 6;
B8 cats_thrown[numcats];
extern ma_engine engine;
extern U16 player_id;
extern seaclient client;
extern double cur_time_sec;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
		cats_thrown[0] = true;
    }
    if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
		cats_thrown[1] = true;
    }
    if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
		cats_thrown[2] = true;
    }
    if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
		cats_thrown[3] = true;
    }
    if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
		cats_thrown[4] = true;
    }
    if (key == GLFW_KEY_6 && action == GLFW_PRESS) {
		cats_thrown[5] = true;
    }

    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        cgull::net::owned_message<char> m;
        m.msg.header = { SONG_START, 0 };
        client.handle_message(m);
        m.msg.body = { 81, 1 };
        F64 timestep__ = 5;
        U64 timestep = *(U64*)&timestep__;
        for (int i = 0; i < sizeof(timestep); i++) {
            m.msg.body.push_back((((timestep) >> (8 * i)) & 0xff));
        }
        m.msg.header = { NEW_NOTE, (U32)m.msg.body.size() };
        client.handle_message(m);
    }
}
