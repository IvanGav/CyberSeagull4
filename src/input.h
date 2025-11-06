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
	std::vector<U8> cats_fired = {};
    if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
		cats_fired.push_back(0);
    }
    if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
		cats_fired.push_back(1);
    }
    if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
		cats_fired.push_back(2);
    }
    if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
		cats_fired.push_back(3);
    }
    if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
		cats_fired.push_back(4);
    }
    if (key == GLFW_KEY_6 && action == GLFW_PRESS) {
		cats_fired.push_back(5);
    }

	if (!cats_fired.empty()) {
		std::vector<U8> message = {player_id & 0xff, (player_id >> 8) && 0xff};
		for (int i = 0; i < sizeof(cur_time_sec); i++) {
			message.push_back(((*(U64*)&cur_time_sec) >> (8 * i)) & 0xff);
		}
		for (auto& cat_fired : cats_fired) {
			playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[cat_fired]);
			throw_cat(cat_fired, true);
			message.push_back(cat_fired);
		}
		client.send_message(PLAYER_CAT_FIRE, message);
	}

}
