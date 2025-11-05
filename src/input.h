#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch);
void throw_cat(int);
F32 weezer_notes[] = { 0.79367809502, 0.89087642854, 1.f, 1.05943508007, 1.33482398807, 1.4982991247 };
F32 weezer[] = { weezer_notes[2], weezer_notes[3], weezer_notes[2], weezer_notes[4], weezer_notes[5], weezer_notes[4], weezer_notes[2], weezer_notes[1], weezer_notes[0]/*, 1.f*/};
I32 weezer_index = 0;
extern ma_engine engine;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
        playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[0]);
		throw_cat(0);
    }
    if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
        playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[1]);
		throw_cat(1);
    }
    if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
        playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[2]);
		throw_cat(2);
    }
    if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
        playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[3]);
		throw_cat(3);
    }
    if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
        playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[4]);
		throw_cat(4);
    }
    if (key == GLFW_KEY_6 && action == GLFW_PRESS) {
        playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[5]);
		throw_cat(5);
    }

}
