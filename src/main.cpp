// STL
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include <cmath>
#include <stdlib.h>
#include <time.h>
#include <atomic>
#include <thread>
#include <random>

#ifdef _MSC_VER
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#ifdef _MSC_VER
#pragma comment( lib, "Winmm.lib" )
#endif

#define NOMINMAX

// GLAD: OpenGL function loader
#include <glad/glad.h>

// GLFW: cross-platform windowing and input
#include <GLFW/glfw3.h>

// STB Image: load image files such as .png and .jpg
#include <stb/stb_image.h>

// tiny_obj_loader: load 3D models in the .obj format
#include <tinyobjloader/tiny_obj_loader.h>

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
#include <glm/gtx/matrix_decompose.hpp>

// miniaudio
#include <miniaudio.h>

// This project
#include "util.h"
#include "world_object.h"
#include "light.h"

//#include "server.h"
#include "client.h"
#include "message.h"
#include "debug.h"
#include "graphics.h"
#include "particle.h"
#include "cam.h"
#include "game.h"
#include "music.h"
#include "input.h"
#include "midi.h"

// CHEATS/DEV OPTIONS
const B8 INFINITE_FIRE = false;
const B8 USE_FREECAM = false;

static std::mt19937 rng{ std::random_device{}() };

static std::uniform_real_distribution<float> randomPitch(0.02f, 1.15f);


std::vector<ma_sound*> liveSounds;


Cam cam = Cam { glm::vec3(-2.39366, 19.5507, -31.1686), -0.015, -0.375, USE_FREECAM ? CamType::FREECAM : CamType::STATIC };

Entity* cannons_friend[6];
Entity* cannons_enemy[6];
// F32 weezer[] = { 1.f, 1.05943508007, 1.f, 1.33482398807, 1.4982991247, 1.33482398807, 1.f, 0.89087642854, 0.79367809502, 1.f };

// Static data
static int width = 1920;
static int height = 1080;
const F32 WATER_HEIGHT = -3.0f;
ma_engine audioEngine;
double curTimeSec;
bool menuOpen = true;

// Graphics global data
extern std::vector<Vertex> vertices;
extern std::vector<Entity> objects;
extern struct Meshes;
extern struct Textures;
extern struct FrameBuffers;
extern struct DynTextures;
extern struct buffers; // Refactor into different buffers

ParticleSource particleSource;
ParticleSource featherSource;

// Networking global stuff

//servergull server(1951);
//bool is_server = false;
U16 player_id = 0xffff;
seaclient client;

// overlay state
int  g_my_health = 5;
int  g_enemy_health = 5;
int g_max_health = 5;
bool g_game_over = false;
U16  g_winner = 0xffff;
bool g_song_active = false;

// lobby
U16  g_p0_id = 0xffff, g_p1_id = 0xffff;
bool g_p0_ready = false, g_p1_ready = false;
bool g_sent_ready = false;

// connect gate
static std::atomic<bool> g_connecting = false;
static std::string g_last_connect_error;
static double g_connect_started = 0.0;

void try_connect(const std::string& ip, U16 port) {
	if (client.IsConnected() || g_connecting.load()) return;
	g_connect_started = glfwGetTime();
	g_last_connect_error.clear();
	g_connecting = true;

	player_id = 0xffff;
	bool ok = client.Connect(ip, port);
	if (!ok) {
		g_last_connect_error = "Failed to start connection";
		g_connecting = false; // only clear on immediate failure
	}
}



static void reset_network_state() {
	player_id = 0xffff;
	g_p0_id = g_p1_id = 0xffff;
	g_p0_ready = g_p1_ready = false;
	g_sent_ready = false;
	g_song_active = false;
	g_game_over = false;
	g_winner = 0xffff;
	g_last_connect_error.clear();
	g_connecting = false;
}

// forward declarations
void cleanupFinishedSounds();
void playWithRandomPitch(ma_engine* engine, const char* filePath);
void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch = 1);
void playSoundVolume(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 volume = 1);
void throw_cats();
void window_size_callback(GLFWwindow* window, int new_width, int new_height);
//void throw_cat(int cat_num, bool owned, F64);

const F64 distancebetweenthetwoshipswhichshallherebyshootateachother = 100;
const glm::vec3 catstartingpos(10.0, 0.0, 10.0);
const F64 distbetweencats = -5.0; // offset to catstartingpos's x axis
constexpr U32 catnumber = 6;
const F64 beats_grace = .5;
B8 cannon_can_fire[catnumber];
B8 cannon_note[catnumber];


glm::mat4 get_cannon_pos(U32 cannon_num, bool friendly) {
	if (friendly)
		return glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(catstartingpos.x + (distbetweencats * cannon_num), catstartingpos.y, catstartingpos.z)), (F32) -PI/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	else
		return glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(catstartingpos.x + (distbetweencats * cannon_num), catstartingpos.y, catstartingpos.z + distancebetweenthetwoshipswhichshallherebyshootateachother)), (F32) PI/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
}

void make_seagull(U8 note, U8 cannon, F64 timestamp) {
	// create an entity a while away from the cannon and move towards the cannon
	objects.push_back(Entity::create(&meshes.seagWalk2, textures.seagull, glm::translate(glm::rotate(get_cannon_pos(cannon, true), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(0.0,1.0,0.0)), PROECTILE));
	objects.back().start_time = timestamp;
	objects.back().pretransmodel = objects.back().model;
	objects.back().update = [cannon, note](Entity& cat, F64 curtime) {
		F64 beats_from_fire = (((song_start_time + cat.start_time) - curTimeSec) / song_spb);
		U8 beats_left = (U8)glm::floor(beats_from_fire);


		glm::mat4 transform(1.0);
		if (beats_left > SHOW_NUM_BEATS) {
			transform = glm::translate(glm::mat4(1.0), glm::vec3(100000));
		} else if (beats_left <= 1) {
			cat.mesh = &meshes.seagBall;
			F64 dist = SEAGULL_MOVE_PER_BEAT;
			// TODO do the jumping animation here
			transform = glm::translate(toModel(
				glm::clamp((F32)(1+(curtime - (song_start_time + cat.start_time - song_spb)) / song_spb), 0.f, 1.f) * SEAGULL_MOVE_PER_BEAT, // distance along lane
				0, // unused
				dist,
				0,
				1.0
			), glm::vec3(0.0, 0.0, -dist));
			/*cat.model = glm::translate(
				glm::translate(cat.pretransmodel, glm::vec3(0.0, dist, 0.0)), 
				-glm::vec3(0.0, glm::clamp((F32)(1+(curtime - (song_start_time + cat.start_time - song_spb)) / song_spb), 0.f, 1.f) * SEAGULL_MOVE_PER_BEAT, 0.0)
			);*/

			//std::cout << "-----dist: " << (1+(curtime - (song_start_time + cat.start_time - song_spb)) / song_spb) << "\n";
		}
		else {
			transform = glm::translate(transform, glm::vec3(0.0, 0.0, -SEAGULL_MOVE_PER_BEAT * (beats_left - 1)));
			cat.mesh = (beats_left % 2) ? &meshes.seagWalk2 : &meshes.seagWalk3;
		}
		cat.model = transform * cat.pretransmodel;

		F64 grace_period = beats_grace * song_spb;
		B8 ccf = (abs(beats_from_fire - 1) < beats_grace);
		if (cats_thrown[cannon] && ccf) {
			return false;
		}
		cannon_can_fire[cannon] |= ccf;
		if (beats_from_fire <= 1-beats_grace) {
			cannon_can_fire[cannon] = 0;
		}
		cannon_note[cannon] = note;

		//return (cat.model[3][1] >= 0.0f);
		return beats_from_fire > 1-beats_grace;
	};
}


int main(int argc, char** argv) {
	//if (argc > 1) {
	//	if (argv[1][0] == '-' && argv[1][1] == 'S') {
	//		server.Start();
	//		while (true) {
	//			server.Update();
	//		}
	//		return 0;
	//	}
	//}
	GLFWwindow* window = init_window(width, height);
	initMouse(window); // function in cam.h
	glfwUpdateGamepadMappings("03000000ba1200004b07000000000000,Guitar Hero,platform:Windows,a:b1,b:b2,x:b3,y:b0,back:b4,start:b5,dpdown:+a1,dpup:-a1");
	initDefaultTexture();
	init_graphics(width, height);

	// Set up window callbacks **NO LONGER IN init_window!!!**

	glfwSetWindowSizeCallback(window, window_size_callback);
	glfwSetKeyCallback(window, key_callback);

	// Create geometry

	std::string song_name;
	std::vector<midi_note> notes = midi_parse_file("asset/Buddy Holly riff.mid", song_name);

	for (int i = 0; i < notes.size(); i++) {
		std::cout << notes[i].time << "s: " << (int)notes[i].note << "\n";
	}

	objects.push_back(Entity::create(&meshes.shipNoMast, textures.ship.color, textures.ship.norm, glm::translate(glm::rotate(glm::radians(90.0F), glm::vec3(0.0F, 1.0F, 0.0F)), glm::vec3(5.0F, -4.1F, 0.0F)), NONEMITTER));
	objects.push_back(Entity::create(&meshes.ship, textures.ship.color, textures.ship.norm, glm::translate(glm::rotate(glm::radians(90.0F), glm::vec3(0.0F, 1.0F, 0.0F)), glm::vec3(-25.0F - distancebetweenthetwoshipswhichshallherebyshootateachother, -4.1F, 0.0F)), NONEMITTER));
	objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), NONEMITTER));

	for (int i = 0; i < 6; i++) {
		//objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(get_cannon_pos(i, true), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), CANNON, true));
		objects.push_back(Entity::create(&meshes.cannon, textures.cannon.color, textures.cannon.norm, get_cannon_pos(i, true), CANNON, true));
		cannons_friend[i] = &objects.back();
		//objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(get_cannon_pos(i, false), (float)PI, glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), CANNON));
		objects.push_back(Entity::create(&meshes.cannon, textures.cannon.color, textures.cannon.norm, get_cannon_pos(i, false), CANNON));
		cannons_enemy[i] = &objects.back();
	}

	std::string server_ip = "136.112.101.5";
	// try_connect(server_ip, 1951);

	Entity water = Entity::create(&meshes.quad, default_tex, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, WATER_HEIGHT, 0.0f)), glm::vec3(500.0, 500.0, 500.0)), NONEMITTER); // TODO water should have its own normal map thing

	// Create static particle sources (later change this to be dynamic or something)
	particleSource = { glm::vec3(0.0F, 2.0F, 0.0F), glm::vec3(0.1f), RGBA8 { 255,255,255,255 }, 1.0f, 1.0f, 0 }; // live for 1 seconds
	particleSource.setSheetRes(8, 8);
	particleSource.scaleOverTime = 0.9;
	particleSource.gravity = 0.0;
	particleSource.drag = 0.9;
	particleSource.initVelScale = 3.0;

	featherSource = { glm::vec3(0.0), glm::vec3(0.0), RGBA8 {255,255,255,255}, 0.6f, 2.0f, 1 }; // live for 2 seconds
	featherSource.randomRotation = 2.0f * PI;
	featherSource.randomRotationOverTime = 10.0;
	featherSource.gravity = 10.0;
	featherSource.drag = 0.95;
	featherSource.initVelScale = 8.0;

	// Bind textures to particle array
	particle_textures[0] = textures.particleExplosion;
	particle_textures[1] = textures.feather;
	particle_textures[2] = textures.cannonExplosion;

	DirectionalLight sun = DirectionalLight{};
	sun.illuminateArea(150.0);

	double last_time_sec = 0.0;
	int songstart;

	/// Initialize midi
	libremidi::observer obs;
	libremidi::midi_in midi{
		libremidi::input_configuration{.on_message = midi_callback }
	};
	std::cout << "midi\n";
	if (obs.get_input_ports().size()) {
		midi_init(midi);
	}

	F32 volume = 100.0;

	ma_engine_init(NULL, &audioEngine);
	ma_engine_set_volume(&audioEngine, volume/100.f);
	playSoundVolume(&audioEngine, "asset/seagull-flock-sound-effect-206610.wav", MA_TRUE, 0.25f);
	//int val1 = 10, val2 = 0, val3 = 0, val4 = 153;

	bool bothReady = false;

	GLFWgamepadstate lastState{};

	static char buf[64];
	windowMouseRelease(window);
	// event loop (each iteration of this loop is one frame of the application)
	while (!glfwWindowShouldClose(window)) {
		// calculate delta time
		curTimeSec = glfwGetTime();
		double dt = curTimeSec - last_time_sec;
		last_time_sec = curTimeSec;
		double lightAzimuth = glfwGetTime() / 3.0;
		glm::vec3 lightDir = getAngle(lightAzimuth, -PI / 4.0);

		// per-frame input
		glfwPollEvents();

		GLFWgamepadstate state{};
		if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
			gamepadInput(state, lastState);
			lastState = state;
		}
		
		if (!menuOpen) {
			moveCamGamepad(window, cam, dt, state);
			moveCamKeyboard(window, cam, dt);
			if (midi_exists) {
				extern std::vector<char> midi_keys_velocity;
				moveCamMidi(window, cam, dt);
			}
		}
		

		for (int i = 0; i < objects.size(); i++) {
			if (objects[i].update) {
				if (!objects[i].update(objects[i], curTimeSec)) {
					objects.erase(objects.begin() + i);
					i--;
				}
			}
		}

		throw_cats();

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


		cleanupFinishedSounds();

		//client.check_messages();

		// pump(fornite shotgun) client incoming messages (assigns player_id, handles remote cat fire)
		if (client.IsConnected()) {
			client.check_messages();
		}
		
		if (client.IsConnected()) {
			g_connecting = false;
		}
		else if (g_connecting && (glfwGetTime() - g_connect_started) > 5.0) {
			g_connecting = false;
			g_last_connect_error = "Timed out connecting to " + server_ip;
			client.Disconnect();
			reset_network_state();
		}



		// Update particles
		advanceParticles(dt);
		//particleSource.spawnParticle();
		sortParticles(cam);
		packParticles();
		send_particle_data_to_gpu();

		// get cam matrices
		glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.lookDir(), cam_up);
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)(width) / height, 0.1f, 1000.0f);

		// Update the light direction
		sun.setLightDirVec3(lightDir);

		// Update the shadowmap texture
		render_shadows(sun);

		// Update the reflection texture
		{
			glm::vec3 modified_pos = cam.pos; modified_pos.y = 2 * WATER_HEIGHT - modified_pos.y;
			glm::vec3 modified_look_dir = glm::vec3(sin(cam.theta) * cos(-cam.y_theta), sin(-cam.y_theta), cos(cam.theta) * cos(-cam.y_theta));
			glm::mat4 modified_view = glm::lookAt(modified_pos, modified_pos + modified_look_dir, glm::vec3(0.0f, -1.0f, 0.0f));

			glBindFramebuffer(GL_FRAMEBUFFER, framebuffers.reflection_framebuffer);
			glViewport(0, 0, width, height);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			draw_skybox(projection, modified_view);
			glEnable(GL_CLIP_DISTANCE0);
			glProgramUniform1f(programs.program, 20, WATER_HEIGHT);
			draw_objects(projection, modified_view, sun, modified_pos, true);
			glDisable(GL_CLIP_DISTANCE0);
			draw_particles(projection, modified_view);

			// Draw the player as a cat
			//{
			//	auto _model = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(cam.pos.x, cam.pos.y, cam.pos.z)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.2f, 0.2f, 0.2f));
			//	Entity o = Entity::create(&meshes.cat, textures.cat, _model, NONEMITTER);
			//	glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
			//	glBindTextureUnit(0, o.tex);
			//	glBindTextureUnit(2, o.normal);
			//	glProgramUniformMatrix4fv(programs.program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
			//	glProgramUniformMatrix3fv(programs.program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));
			//	glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
			//}

			glUseProgram(programs.program);

			glProgramUniformMatrix4fv(programs.program, 4, 1, GL_FALSE, glm::value_ptr(projection * modified_view));
			glProgramUniformMatrix4fv(programs.program, 11, 1, GL_FALSE, glm::value_ptr(sun.combined));
			glProgramUniform3fv(programs.program, 15, 1, glm::value_ptr(modified_pos));
			glProgramUniform3fv(programs.program, 16, 1, glm::value_ptr(sun.dir));
			glProgramUniform3fv(programs.program, 17, 1, glm::value_ptr(glm::vec3(1.0)));
			glProgramUniform2f(programs.program, 18, (F32)shadowmap_height, (F32)shadowmap_width);
			glProgramUniform1i(programs.program, 19, false);
			glBindTextureUnit(1, dyn_textures.shadowmap);

			{
				auto _model = glm::rotate(glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(cam.pos.x, cam.pos.y, cam.pos.z)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.2f, 0.2f, 0.2f)), cam.theta, glm::vec3(0.0, 0.0, 1.0));
				Entity o = Entity::create(&meshes.cat, textures.cat, _model, NONEMITTER);
				glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
				glBindTextureUnit(0, o.tex);
				glBindTextureUnit(2, o.normal);

				glProgramUniformMatrix4fv(programs.program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
				glProgramUniformMatrix3fv(programs.program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

				glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
			}
		}

		// Draw to screen
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		draw_skybox(projection, view);
		draw_objects(projection, view, sun, cam.pos);
		draw_water(projection, view, sun, cam.pos, curTimeSec, water);
		draw_particles(projection, view);

		// TEMP UI FIX
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;

		ImGui::SetNextWindowSize(ImVec2(width-(width/10), 100));
		ImGui::SetNextWindowPos(ImVec2(width/20, height * 0.01));
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
			else if (g_winner == player_id) ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "You Win!");
			else ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "You Lose!");
		}
		ImGui::End();

		/*if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
			songSelect(textures.weezer, "asset/weezer-riff.wav", ImVec2(637, 640));
			playSound(&engine, "asset/weezer-riff.wav", MA_FALSE);
		}*/


		if (menuOpen) {
			//flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;

			ImGui::SetNextWindowSize(ImVec2(width - 200, height - 200));
			ImGui::SetNextWindowPos(ImVec2(100, 100));
			ImGui::Begin("menu", NULL, flags);
			ImGui::Image((ImTextureID)textures.menu.page, ImVec2(width - 200, height - 200));
			ImGui::End();
			flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;

			ImGui::SetNextWindowSize(ImVec2(width-200, height-200));
			ImGui::SetNextWindowPos(ImVec2(100, 100));
			ImGui::Begin("menu", NULL, flags);
			int padding = 100;
			F32 menuw = (width) / 3, menuh = ((width) / 3) * 0.562, menux = (width - (2 * padding) - menuw) / 2, menuy = height / 70;
			ImGui::SetCursorPos(ImVec2(menux, menuy));
			ImGui::Image((ImTextureID)textures.menu.menu_logo, ImVec2(menuw, menuh));
			F32 contextw = width / 3, contexth = width / 3, contextx = (width - (2 * padding) - (contextw)) / 2 + (menuw / 2), contexty = menuy + menuh - (menuh/2);
			ImGui::SetCursorPos(ImVec2(contextx, contexty));
			ImGui::Image((ImTextureID)textures.menu.context, ImVec2(contextw, contexth));
			/*
				ImGui::SliderInt("val 1", &val1, 0, 256);
				ImGui::SliderInt("val 2", &val2, 0, 256);
				ImGui::SliderInt("val 3", &val3, 0, 256);
				ImGui::SliderInt("val 4", &val4, 0, 256);
				ImGui::Text(((std::to_string(val1) + "." + std::to_string(val2) + "." + std::to_string(val3) + "." + std::to_string(val4)).c_str()));
			*/
			F32 inputw = width/20, inputx = width/10, inputy = menuy + menuh, spacing = height/20;
			ImGui::SetCursorPos(ImVec2(inputx, inputy));
			ImGui::PushItemWidth(inputw);
			
			
			
			
			/*
			if (ImGui::InputText("Ip Address", buf, IM_ARRAYSIZE(buf), inflags) && !client.IsConnected()) goto inputChange;
			ImGui::PopItemWidth();
			if (!client.IsConnected()) {
				ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
				ImGui::Image((ImTextureID)textures.menu.connect, ImVec2(buttonw, buttonh));
				ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
				if (ImGui::InvisibleButton("Connect", ImVec2(buttonw, buttonh))) {
					inputChange:
					//server_ip = std::to_string(val1) + "." + std::to_string(val2) + "." + std::to_string(val3) + "." + std::to_string(val4);
					server_ip = buf;
					client.Connect(server_ip, 1951);
				}
			}
			else {
				F32 buttondisw = buttonw * 0.479166667, buttondish = buttondisw * buttonDisar;
				ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
				ImGui::Image((ImTextureID)textures.menu.leave, ImVec2(buttondisw, buttondish));
				ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
				if (ImGui::InvisibleButton("Disconnect", ImVec2(buttondisw, buttondish))) {
					client.Disconnect();
				}
			}
			*/
			
			
			// Editable server IP and connect disconnect buttons
			static bool ipbuf_init = false;
			static char ipbuf[64];
			if (!ipbuf_init) {
				std::snprintf(ipbuf, sizeof(ipbuf), "%s", server_ip.c_str());
				ipbuf_init = true;
			}


			ImGuiInputTextFlags inflags = ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
			F32 buttonw = inputw * 2, buttonConar = 0.40625, buttonDisar = 1.7173913, buttonh = buttonw * buttonConar;
			if (ImGui::InputText("Server IP", ipbuf, sizeof(ipbuf))) {
				server_ip = ipbuf;
				std::cout << "Server IP:" <<  ipbuf;
			}

			if (!client.IsConnected()) {
				if (!g_connecting) {
					if (!g_last_connect_error.empty()) {
						ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", g_last_connect_error.c_str());
					}
					ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
					ImGui::Image((ImTextureID)textures.menu.connect, ImVec2(buttonw, buttonh));
					ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
					if (ImGui::InvisibleButton("Join Game", ImVec2(buttonw, buttonh))) {
						try_connect(server_ip, 1951);
					}
				}
				else {
					ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
					ImGui::Text("Connecting to %s...", server_ip.c_str());
					ImGui::SetCursorPos(ImVec2(inputx, inputy + (2 * spacing)));
					if (ImGui::Button("Cancel")) {
						client.Disconnect();
						reset_network_state();
					}
				}
			}
			else {
				F32 buttondisw = buttonw * 0.479166667, buttondish = buttondisw * buttonDisar;
				ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
				ImGui::Image((ImTextureID)textures.menu.leave, ImVec2(buttondisw, buttondish));
				ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
				if (ImGui::InvisibleButton("Disconnect", ImVec2(buttondisw, buttondish))) {
					client.Disconnect();
					reset_network_state(); 
				}
			}

			F32 readyd = buttonw*0.5, ready1x = inputx + buttonw + width / 40, ready1y = inputy + (spacing);
			
			if (g_p0_ready) {
				ImGui::SetCursorPos(ImVec2(ready1x, ready1y));
				ImGui::Image((ImTextureID)textures.menu.P1Ready, ImVec2(readyd, readyd));
			}
			else {
				ImGui::SetCursorPos(ImVec2(ready1x, ready1y));
				ImGui::Image((ImTextureID)textures.menu.P1NotReady, ImVec2(readyd, readyd));
			}

			F32 ready2x = ready1x + readyd + width / 50, ready2y = ready1y;

			if (g_p1_ready) {
				ImGui::SetCursorPos(ImVec2(ready2x, ready2y));
				ImGui::Image((ImTextureID)textures.menu.P2Ready, ImVec2(readyd, readyd));
			}
			else {
				ImGui::SetCursorPos(ImVec2(ready2x, ready2y));
				ImGui::Image((ImTextureID)textures.menu.P2NotReady, ImVec2(readyd, readyd));
			}

			/*
			ImGui::Text("Player 0: %s  [%s]",
				g_p0_id == 0xffff ? "(empty)" : std::to_string(g_p0_id).c_str(),
				g_p0_ready ? "Ready" : "Not Ready");
			ImGui::SetCursorPos(ImVec2(inputx, inputy + (spacing * 3) + buttonh));
			ImGui::Text("Player 1: %s  [%s]",
				g_p1_id == 0xffff ? "(empty)" : std::to_string(g_p1_id).c_str(),
				g_p1_ready ? "Ready" : "Not Ready");
			*/
			
			// main.cpp – inside the menu_open == true block, around the "Start Game" drawing
			const bool i_am_player0 = (player_id != 0xffff && player_id == g_p0_id);
			const bool i_am_player1 = (player_id != 0xffff && player_id == g_p1_id);
			const bool i_am_player = i_am_player0 || i_am_player1;

			ImVec2 startSize = ImVec2(readyd * 1.5f, readyd * 1.5f);
			ImVec2 startPos = ImVec2(((2 * ready1x) + (2 * readyd) - startSize.x) / 2.0f,
				ready1y + readyd);

			// Show button only if I'm actually a player and no match is running
			if (!g_song_active && i_am_player && player_id != 0xffff)
			{
				if (!g_sent_ready)
				{
					ImGui::SetCursorPos(startPos);
					ImGui::Image((ImTextureID)textures.menu.startGame, startSize);

					// Place the invisible button at the SAME position/size as the image:
					ImGui::SetCursorPos(startPos);
					if (ImGui::InvisibleButton("Start Game", startSize))
					{
						cgull::net::message<message_code> m;
						m.header.id = message_code::PLAYER_READY;
						U16 pid = player_id;
						m << pid;
						if (client.IsConnected() && player_id != 0xffff) {
							client.Send(m);
							g_sent_ready = true;
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
				ImGui::TextDisabled(g_song_active ? "Match in progress" : "Spectating (button disabled)");
			}

			
			
			ImGui::SetCursorPos(ImVec2(inputx, inputy + (spacing * 5) + buttonh));
			ImGui::PushItemWidth(inputw*4);
			ImGui::SliderFloat("Volume", &volume, 0, 256);
			ImGui::PopItemWidth();
			
			ImGui::SetCursorPos(ImVec2((width - 200 - 100) / 2, height - 350));
			ImGui::Image((ImTextureID)textures.menu.closeMenu, ImVec2(100, 100));
			ImGui::SetCursorPos(ImVec2((width - 200 - 100) / 2, height - 350));
			if (ImGui::InvisibleButton("Close Menu", ImVec2(100, 100))) {
				menuOpen = false;
				windowMouseFocus(window);
			}
			ImGui::End();
			ma_engine_set_volume(&audioEngine, volume / 100.f);
		}
		else {
			ImGui::SetNextWindowSize(ImVec2(50, 50));
			ImGui::SetNextWindowPos(ImVec2(50, 50));
			ImGui::Begin("open menu", NULL, flags);
			if (ImGui::Button("Menu")) {
				menuOpen = true;
				windowMouseRelease(window);
			}
			ImGui::End();

		}


		// Status

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// present
		glfwSwapBuffers(window);

	}

	ma_engine_uninit(&audioEngine);
	graphics_cleanup();
	cleanup_window(window);
}

void draw_cannon(glm::vec3 pos) {
	glDrawArrays(GL_TRIANGLES, meshes.cannon.offset, meshes.cannon.size);
}

void throw_cats() {
	bool send = false;
	std::vector<uint8_t> cats;

	for (int i = 0; i < numcats; i++) {
		if (cats_thrown[i]) {
			if (cannon_can_fire[i] || INFINITE_FIRE) { // TODO remove this later
				throw_cat(i, true); // local projectile + sfx
				cats.push_back(static_cast<uint8_t>(i));
				send = true;
				cannon_can_fire[i] = false;
			}
			cats_thrown[i] = false;
		}
	}

	if (send && client.IsConnected() && player_id != 0xffff) {
		client.send_player_cat_fire(player_id, curTimeSec, cats);
	}
}

void throw_cat(int cat_num, bool owned, double the_note_that_this_cat_was_played_to_is_supposed_to_be_played_at_time) {
	F64 start_time = curTimeSec;
	F64 time_diff = curTimeSec - the_note_that_this_cat_was_played_to_is_supposed_to_be_played_at_time;

	// Prefer a cannon whose owned flag matches, otherwise use any matching cat_id.
	int best = -1, fallback = -1;
	for (int j = 0; j < (int)objects.size(); ++j) {
		if (objects[j].type == CANNON && objects[j].cat_id == cat_num) {
			if (objects[j].owned == owned) { best = j; break; }
			if (fallback == -1) fallback = j;
		}
	}
	const int i = (best != -1 ? best : fallback);
	if (i == -1) return; // no suitable cannon found

	playSound(&audioEngine, "asset/cat-meow-401729-2.wav", false, noteMultiplier((U8)72,cannon_note[cat_num]));
	playSoundVolume(&audioEngine, "asset/cannon.wav", false, 0.3);

	glm::vec4 pos = objects[i].model[3];
	addParticle(Particle{
			.pos = glm::vec3(pos.x, pos.y + 2.16504F, pos.z + 0.052887F),
			.color = { 255, 255, 255, 255 },
			.size = 10.0,
			.life = 2.0F,
			.maxLife = 2.0F,
			.tex = 2,
			.sheetResX = 8,
			.sheetResY = 4,
			.axis = glm::vec3(0.0F, 2.75997F - 2.66555F, 1.06046F + 0.251254F) * 2.0F
	});

	// Spawn projectile using the chosen cannon's transform
	objects.push_back(Entity::create(&meshes.seagBall, textures.seagull, objects[i].model, PROECTILE));
	Entity& p = objects.back();

	p.start_time = start_time;
	p.pretransmodel = p.model;
	p.shoot_angle = owned ? 0.0f : PI;

	p.update = [cat_num](Entity& cat, F64 curtime) {
		cat.model = toModel(
			(curtime - cat.start_time) * 50, // distance along lane
			0,                               // lane index
			distancebetweenthetwoshipswhichshallherebyshootateachother,
			cat.shoot_angle
		) * cat.pretransmodel;

		glm::vec3 self_pos = glm::vec3(cat.model[3]);
		for (int i = 0; i < objects.size(); i++) {
			if (objects[i].type != PROECTILE || // only collide with seagulls
				//objects[i].owned == cat.owned // don't collide with own faction
				objects[i].shoot_angle == cat.shoot_angle
			) continue;
			glm::vec3 enemy_pos = glm::vec3(objects[i].model[3]);
			F32 dist = glm::length(self_pos - enemy_pos);
			if (dist < 5.0) {
				featherSource.pos = self_pos;
				featherSource.spawnParticles(75);
				objects[i].markedForDeath = true;
				return false;
			}
		}

		// Die if marked for death
		if (cat.markedForDeath) {
			featherSource.pos = self_pos;
			featherSource.spawnParticles(75);
			return false;
		}
		// keep alive while above ground
		if (cat.model[3][1] < 2.0f && (curtime-cat.start_time) > 1.0) {
			particleSource.pos = self_pos;
			particleSource.spawnParticles(75);
			return false;
		}

		return true;
	};
}

void cleanupFinishedSounds() {
	for (auto it = liveSounds.begin(); it != liveSounds.end();) {
		ma_sound* s = *it;
		if (!ma_sound_is_playing(s) && ma_sound_at_end(s)) {
			ma_sound_uninit(s);
			delete s;
			it = liveSounds.erase(it);
		}
		else {
			++it;
		}
	}
}

void playWithRandomPitch(ma_engine* engine, const char* filePath) {
	ma_sound* s = new ma_sound{};
	if (ma_sound_init_from_file(engine, filePath,
		MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE,
		nullptr, nullptr, s) == MA_SUCCESS) {
		ma_sound_set_pitch(s, randomPitch(rng));
		ma_sound_start(s);
		liveSounds.push_back(s);
	}
	else {
		delete s;
	}
}

void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch) {
	ma_sound* s = new ma_sound{};
	//ma_data_source_set_looping(s, loop);
	if (ma_sound_init_from_file(engine, filePath,
		MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE,
		nullptr, nullptr, s) == MA_SUCCESS) {
		ma_sound_set_looping(s, loop);
		ma_sound_set_pitch(s, pitch);
		ma_sound_start(s);
		liveSounds.push_back(s);
	}
	else {
		delete s;
	}
}

void playSoundVolume(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 volume) {
	ma_sound* s = new ma_sound{};
	//ma_data_source_set_looping(s, loop);
	if (ma_sound_init_from_file(engine, filePath,
		MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE,
		nullptr, nullptr, s) == MA_SUCCESS) {
		ma_sound_set_looping(s, loop);
		ma_sound_set_volume(s, volume);
		ma_sound_start(s);
		liveSounds.push_back(s);
	}
	else {
		delete s;
	}
}

/* Graphics Functions */

void window_size_callback(GLFWwindow* window, int new_width, int new_height) {
	width = new_width;
	height = new_height;
	initWaterFramebuffer(width, height);
}
