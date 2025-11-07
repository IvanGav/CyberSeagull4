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

#include <cmath>

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

/*

1,00000,00000

1 one
10 ten
100 hundred
1000 thousand
10000 kleep klop
1,00000 quinto
10,00000 ten quintos
...
10000,00000 kleep klop quintos = 1,000,000,000 billion
45000,00000 4.5 kleep klop quintos = age of universe
1,00000,00000 flippo = 10,000,000,000 ten billion
10000,00000,00000 kleep klop flippos = 100,000,000,000,000 hundred trillion

*/

F32 noteMultiplier(U8 start, U8 end) {
	F32 freq1 = 440.f * pow((double)2, (start - 81) / 12.f), freq2 = 440.f * pow((double)2, (end - 81) / 12.f);
	return freq2 / freq1;
}

F32 noteMultiplier(F32 start, U8 end) {
	F32 freq2 = 440.f * pow((double)2, (end - 81) / 12.f);
	return freq2 / start;
}
