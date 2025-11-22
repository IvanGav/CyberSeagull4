#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "message.h"


void throwCat(int cat_num, bool owned, F64 = -1);

void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch);
F32 weezer_notes[] = { 0.79367809502, 0.89087642854, 1.f, 1.05943508007, 1.33482398807, 1.4982991247 };
F32 weezer[] = { weezer_notes[2], weezer_notes[3], weezer_notes[2], weezer_notes[4], weezer_notes[5], weezer_notes[4], weezer_notes[2], weezer_notes[1], weezer_notes[0]/*, 1.f*/};
I32 weezer_index = 0;
constexpr U16 numCats = 6;
B8 catsThrown[numCats];
extern ma_engine audioEngine;
extern U16 g_playerID;
extern seaclient g_client;
extern double curTimeSec;
extern bool menuOpen;
extern ParticleSource g_featherSource;
extern Cam g_cam;

void makeSeagull(U8 note, U8 cannon, F64 timestamp);

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        menuOpen = !menuOpen;
        if(menuOpen) windowMouseRelease(window);
        else windowMouseFocus(window);
    }
    if (!menuOpen) {
        if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
            catsThrown[0] = true;
        }
        if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
            catsThrown[1] = true;
        }
        if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
            catsThrown[2] = true;
        }
        if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
            catsThrown[3] = true;
        }
        if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
            catsThrown[4] = true;
        }
        if (key == GLFW_KEY_6 && action == GLFW_PRESS) {
            catsThrown[5] = true;
        }
      
        if (key == GLFW_KEY_I && action == GLFW_PRESS) {
            songStartTime = curTimeSec;
            makeSeagull(65, 0, 3);
        }

        if (key == GLFW_KEY_O && action == GLFW_PRESS) {
            g_featherSource.pos = g_cam.pos;
            g_featherSource.spawnParticles(100);
        }

        if (key == GLFW_KEY_P && action == GLFW_PRESS) {
            //song_start_time = cur_time_sec;
            //make_seagull(65, 0, 3);
            throwCat(0, false);
        }
    }
}

void gamepadInput(GLFWgamepadstate state, GLFWgamepadstate lastState) {
    if (!menuOpen) {
        if (state.buttons[0] == GLFW_PRESS && lastState.buttons[0] != GLFW_PRESS) {
            catsThrown[0] = true;
        }
        if (state.buttons[1] == GLFW_PRESS && lastState.buttons[1] != GLFW_PRESS) {
            catsThrown[1] = true;
        }
        if (state.buttons[2] == GLFW_PRESS && lastState.buttons[2] != GLFW_PRESS) {
            catsThrown[2] = true;
        }
        if (state.buttons[3] == GLFW_PRESS && lastState.buttons[3] != GLFW_PRESS) {
            catsThrown[3] = true;
        }
        if (state.buttons[6] == GLFW_PRESS && lastState.buttons[6] != GLFW_PRESS) {
            catsThrown[4] = true;
        }
        if (state.buttons[7] == GLFW_PRESS && lastState.buttons[7] != GLFW_PRESS) {
            catsThrown[5] = true;
        }
    }
}