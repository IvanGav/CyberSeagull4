#pragma once

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ImGUI: gui library
#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

// miniaudio
#include <miniaudio.h>

void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch);
extern ma_engine engine;

void songSelect(GLuint display, const char* filepath, ImVec2 dim) {
	if (display) {
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;
		playSound(&engine, filepath, false, .9);
		ImGui::SetNextWindowSize(ImVec2(1000, 1000));
		ImGui::SetNextWindowPos(ImVec2((1920 - dim[0]) / 2, 1080 * 0.1));
		ImGui::Begin("Song", NULL, flags);
		ImGui::Image((ImTextureID)display, dim);
		ImGui::End();
	}
}