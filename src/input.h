#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "message.h"


void throw_cat(int cat_num, bool owned, F64 = -1);

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
extern bool menu_open;

void make_seagull(U8 cannon, F64 timestamp);

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        menu_open = !menu_open;
        if(menu_open) windowMouseRelease(window);
        else windowMouseFocus(window);
    }
    if (!menu_open) {
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
      
        if (key == GLFW_KEY_I && action == GLFW_PRESS) {
            song_start_time = cur_time_sec;
            make_seagull(0, 3);
        }
    }
}
