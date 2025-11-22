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
extern ma_engine audioEngine;
extern U16 player_id;
extern seaclient client;
extern double curTimeSec;
extern bool menuOpen;
extern ParticleSource featherSource;
extern Cam cam;

void make_seagull(U8 note, U8 cannon, F64 timestamp);

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        menuOpen = !menuOpen;
        if(menuOpen) windowMouseRelease(window);
        else windowMouseFocus(window);
    }
    if (!menuOpen) {
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
            song_start_time = curTimeSec;
            make_seagull(65, 0, 3);
        }

        if (key == GLFW_KEY_O && action == GLFW_PRESS) {
            featherSource.pos = cam.pos;
            featherSource.spawnParticles(100);
        }

        if (key == GLFW_KEY_P && action == GLFW_PRESS) {
            //song_start_time = cur_time_sec;
            //make_seagull(65, 0, 3);
            throw_cat(0, false);
        }
    }
}

void gamepadInput(GLFWgamepadstate state, GLFWgamepadstate lastState) {
    if (!menuOpen) {
        if (state.buttons[0] == GLFW_PRESS && lastState.buttons[0] != GLFW_PRESS) {
            cats_thrown[0] = true;
        }
        if (state.buttons[1] == GLFW_PRESS && lastState.buttons[1] != GLFW_PRESS) {
            cats_thrown[1] = true;
        }
        if (state.buttons[2] == GLFW_PRESS && lastState.buttons[2] != GLFW_PRESS) {
            cats_thrown[2] = true;
        }
        if (state.buttons[3] == GLFW_PRESS && lastState.buttons[3] != GLFW_PRESS) {
            cats_thrown[3] = true;
        }
        if (state.buttons[6] == GLFW_PRESS && lastState.buttons[6] != GLFW_PRESS) {
            cats_thrown[4] = true;
        }
        if (state.buttons[7] == GLFW_PRESS && lastState.buttons[7] != GLFW_PRESS) {
            cats_thrown[5] = true;
        }
    }
}