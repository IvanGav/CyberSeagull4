#pragma once

#include <atomic>

// ImGUI: gui library
#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <glm/gtx/matrix_decompose.hpp>

void tryConnect(const std::string& ip, U16 port);
static void resetNetworkState();


/* ImGui Functions */

void healthbars(ImGuiWindowFlags flags, static int windowWidth, static int windowHeight, int g_my_health, int g_enemy_health, int g_max_health, bool g_game_over, U16 g_winner) {
	ImGui::SetNextWindowSize(ImVec2(windowWidth - (windowWidth / 10), 100));
	ImGui::SetNextWindowPos(ImVec2(windowWidth / 20, windowHeight * 0.01));
	ImGui::Begin("Status", nullptr, flags);
	/*
	ImGui::Text("My HP: %d", g_my_health);
	ImGui::Text("Enemy HP: %d", g_enemy_health);
	*/
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
	ImGui::ProgressBar(static_cast<F32>(g_my_health) / static_cast<F32>(g_max_health));
	ImGui::ProgressBar(static_cast<F32>(g_enemy_health) / static_cast<F32>(g_max_health));
	ImGui::PopStyleColor();
	if (g_game_over) {
		ImGui::Separator();
		if (g_winner == 0xffff) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Game Over");
		else if (g_winner == g_playerID) ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "You Win!");
		else ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "You Lose!");
	}
	ImGui::End();
}



void page(ImGuiWindowFlags flags, static int windowWidth, static int windowHeight) {
	ImGui::SetNextWindowSize(ImVec2(windowWidth - 200, windowHeight - 200));
	ImGui::SetNextWindowPos(ImVec2(100, 100));
	ImGui::Begin("menu", NULL, flags);
	ImGui::Image((ImTextureID)textures.menu.page, ImVec2(windowWidth - 200, windowHeight - 200));
	ImGui::End();
}



void menu2(
	ImGuiWindowFlags flags, 
	int windowWidth, 
	int windowHeight, 
	std::string server_ip, 
	std::atomic<bool> &g_connecting, 
	std::string g_last_connect_error,
	F32 menux,
	F32 menuy,
	F32 menuw,
	F32 menuh,
	int padding,
	F32 inputx,
	F32 inputy,
	F32 inputw,
	F32 buttonw,
	F32 buttonh,
	F32 spacing,
	F32 buttonDisar,
	float* volume,
	GLFWwindow* window
) {
	page(flags, windowWidth, windowHeight);
	flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;

	ImGui::SetNextWindowSize(ImVec2(windowWidth - 200, windowHeight - 200));
	ImGui::SetNextWindowPos(ImVec2(100, 100));
	ImGui::Begin("menu", NULL, flags);
	
	// menu logo
	ImGui::SetCursorPos(ImVec2(menux, menuy));
	ImGui::SetCursorPos(ImVec2(menux, menuy));
	ImGui::Image((ImTextureID)textures.menu.menu_logo, ImVec2(menuw, menuh));
	
	// context
	F32 contextw = windowWidth / 3, contexth = windowWidth / 3, contextx = (windowWidth - (2 * padding) - (contextw)) / 2 + (menuw / 2), contexty = menuy + menuh - (menuh / 2);
	ImGui::SetCursorPos(ImVec2(contextx, contexty));
	ImGui::Image((ImTextureID)textures.menu.context, ImVec2(contextw, contexth));


	// Editable server IP and connect disconnect setup
	static bool ipbuf_init = false;
	static char ipbuf[64];
	if (!ipbuf_init) {
		std::snprintf(ipbuf, sizeof(ipbuf), "%s", server_ip.c_str());
		ipbuf_init = true;
	}

	// server ip input
	ImGui::SetCursorPos(ImVec2(inputx, inputy));
	ImGui::PushItemWidth(inputw);
	ImGuiInputTextFlags inflags = ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
	if (ImGui::InputText("Server IP", ipbuf, sizeof(ipbuf))) {
		server_ip = ipbuf;
		std::cout << "Server IP:" << ipbuf;
	}

	// Checking connect and displaying button
	if (!g_client.IsConnected()) {
		if (!g_connecting) {
			if (!g_last_connect_error.empty()) {
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", g_last_connect_error.c_str());
			}
			ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
			ImGui::Image((ImTextureID)textures.menu.connect, ImVec2(buttonw, buttonh));
			ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
			if (ImGui::InvisibleButton("Join Game", ImVec2(buttonw, buttonh))) {
				tryConnect(server_ip, 1951);
			}
		}
		else {
			ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
			ImGui::Text("Connecting to %s...", server_ip.c_str());
			ImGui::SetCursorPos(ImVec2(inputx, inputy + (2 * spacing)));
			if (ImGui::Button("Cancel")) {
				g_client.Disconnect();
				resetNetworkState();
			}
		}
	}

	// Disconnect button
	else {
		F32 buttondisw = buttonw * 0.479166667, buttondish = buttondisw * buttonDisar;
		ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
		ImGui::Image((ImTextureID)textures.menu.leave, ImVec2(buttondisw, buttondish));
		ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
		if (ImGui::InvisibleButton("Disconnect", ImVec2(buttondisw, buttondish))) {
			g_client.Disconnect();
			resetNetworkState();
		}
	}

	// Player 1 ready display
	F32 readyd = buttonw * 0.5, ready1x = inputx + buttonw + windowWidth / 40, ready1y = inputy + (spacing);
	if (g_p0_ready) {
		ImGui::SetCursorPos(ImVec2(ready1x, ready1y));
		ImGui::Image((ImTextureID)textures.menu.P1Ready, ImVec2(readyd, readyd));
	}
	else {
		ImGui::SetCursorPos(ImVec2(ready1x, ready1y));
		ImGui::Image((ImTextureID)textures.menu.P1NotReady, ImVec2(readyd, readyd));
	}

	// Player 2 ready display
	F32 ready2x = ready1x + readyd + windowWidth / 50, ready2y = ready1y;
	if (g_p1_ready) {
		ImGui::SetCursorPos(ImVec2(ready2x, ready2y));
		ImGui::Image((ImTextureID)textures.menu.P2Ready, ImVec2(readyd, readyd));
	}
	else {
		ImGui::SetCursorPos(ImVec2(ready2x, ready2y));
		ImGui::Image((ImTextureID)textures.menu.P2NotReady, ImVec2(readyd, readyd));
	}


	// Determines if player 1 or 2
	const bool i_am_player0 = (g_playerID != 0xffff && g_playerID == g_p0_id);
	const bool i_am_player1 = (g_playerID != 0xffff && g_playerID == g_p1_id);
	const bool i_am_player = i_am_player0 || i_am_player1;

	ImVec2 startSize = ImVec2(readyd * 1.5f, readyd * 1.5f);
	ImVec2 startPos = ImVec2(((2 * ready1x) + (2 * readyd) - startSize.x) / 2.0f,
		ready1y + readyd);

	// Show button only if I'm actually a player and no match is running
	if (!g_songActive && i_am_player && g_playerID != 0xffff)
	{
		if (!g_sentReady)
		{
			ImGui::SetCursorPos(startPos);
			ImGui::Image((ImTextureID)textures.menu.startGame, startSize);

			// Place the invisible button at the SAME position/size as the image:
			ImGui::SetCursorPos(startPos);
			if (ImGui::InvisibleButton("Start Game", startSize))
			{
				cgull::net::message<message_code> m;
				m.header.id = message_code::PLAYER_READY;
				U16 pid = g_playerID;
				m << pid;
				if (g_client.IsConnected() && g_playerID != 0xffff) {
					g_client.Send(m);
					g_sentReady = true;
				}
			}
			ImGui::SameLine(); ImGui::TextDisabled("(press when ready)");
		}
		else
		{
			ImGui::SetCursorPos(startPos);
			ImGui::Image((ImTextureID)textures.menu.waitForPlayer, startSize);
		}
	}
	else
	{
		ImGui::SetCursorPos(startPos);
		ImGui::TextDisabled(g_songActive ? "Match in progress" : "Spectating (button disabled)");
	}

	// Volume Bar
	ImGui::SetCursorPos(ImVec2(inputx, inputy + (spacing * 5) + buttonh));
	ImGui::PushItemWidth(inputw * 4);
	ImGui::SliderFloat("Volume", volume, 0, 256);
	ImGui::PopItemWidth();

	// Close Yar Menu
	ImGui::SetCursorPos(ImVec2((windowWidth - 200 - 100) / 2, windowHeight - 350));
	ImGui::Image((ImTextureID)textures.menu.closeMenu, ImVec2(100, 100));
	ImGui::SetCursorPos(ImVec2((windowWidth - 200 - 100) / 2, windowHeight - 350));
	if (ImGui::InvisibleButton("Close Menu", ImVec2(100, 100))) {
		menuOpen = false;
		windowMouseFocus(window);
	}


	ImGui::End();
	// menu end
}

void menu(
	ImGuiWindowFlags flags,
	int width,
	int height,
	std::string server_ip,
	std::atomic<bool>& g_connecting,
	std::string g_last_connect_error, 
	float* volume,
	GLFWwindow* window
) {
	int padding = 100;
	F32 menuw = (width) / 3, menuh = ((width) / 3) * 0.562, menux = (width - (2 * padding) - menuw) / 2, menuy = height / 70;
	F32 inputw = width / 20, inputx = width / 10, inputy = menuy + menuh, spacing = height / 20;
	F32 buttonw = inputw * 2, buttonConar = 0.40625, buttonDisar = 1.7173913, buttonh = buttonw * buttonConar;
	menu2(flags, width, height, server_ip, g_connecting, g_last_connect_error, menux, menuy, menuw, menuh, padding, inputx, inputy, inputw, buttonw, buttonh, spacing, buttonDisar, volume, window);
}

void menuClose(
	ImGuiWindowFlags flags,
	GLFWwindow* window
) {
	ImGui::SetNextWindowSize(ImVec2(50, 50));
	ImGui::SetNextWindowPos(ImVec2(50, 50));
	ImGui::Begin("open menu", NULL, flags);
	if (ImGui::Button("Menu")) {
		menuOpen = true;
		windowMouseRelease(window);
	}
	ImGui::End();
}