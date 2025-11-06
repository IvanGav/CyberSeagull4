#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "message.h"

void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch);
void throw_cat(int, bool, F64 time = 0);
F32 weezer_notes[] = { 0.79367809502, 0.89087642854, 1.f, 1.05943508007, 1.33482398807, 1.4982991247 };
F32 weezer[] = { weezer_notes[2], weezer_notes[3], weezer_notes[2], weezer_notes[4], weezer_notes[5], weezer_notes[4], weezer_notes[2], weezer_notes[1], weezer_notes[0]/*, 1.f*/};
I32 weezer_index = 0;
extern ma_engine engine;
extern U16 player_id;
extern seaclient client;
extern double cur_time_sec;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	U8 cat_fired = 0xff;
    if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
		cat_fired = 0;
    }
    if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
		cat_fired = 1;
    }
    if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
		cat_fired = 2;
    }
    if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
		cat_fired = 3;
    }
    if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
		cat_fired = 4;
    }
    if (key == GLFW_KEY_6 && action == GLFW_PRESS) {
		cat_fired = 5;
    }

	if (cat_fired != 0xff) {
		F64 timestep = cur_time_sec + 1;
		std::vector<U8> message = {(U8)(player_id & 0xff), (U8)((player_id >> 8) && 0xff)};
		for (int i = 0; i < sizeof(timestep); i++) {
			message.push_back(((*(U64*)&timestep) >> (8 * i)) & 0xff);
		}
		throw_cat(cat_fired, true);
		message.push_back(cat_fired);
		client.send_message(PLAYER_CAT_FIRE, message);
	}

}
