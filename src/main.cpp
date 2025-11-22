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
#include "renderGUI.h"

// CHEATS/DEV OPTIONS
const B8 g_INFINITE_FIRE = false;
const B8 g_USE_FREECAM = false;

static std::mt19937 g_rng{ std::random_device{}() };

static std::uniform_real_distribution<float> g_randomPitch(0.02f, 1.15f);


std::vector<ma_sound*> g_liveSounds;


Cam g_cam = Cam { glm::vec3(-2.39366, 19.5507, -31.1686), -0.015, -0.375, g_USE_FREECAM ? CamType::FREECAM : CamType::STATIC };

Entity* g_cannonsFriendly[6];
Entity* g_cannonsEnemy[6];
// F32 weezer[] = { 1.f, 1.05943508007, 1.f, 1.33482398807, 1.4982991247, 1.33482398807, 1.f, 0.89087642854, 0.79367809502, 1.f };

// Static data
static int g_width = 1920;
static int g_height = 1080;
const F32 WATER_HEIGHT = -3.0f;
ma_engine audioEngine;
double curTimeSec;
bool menuOpen = true;

// Graphics global data
extern std::vector<Vertex> g_vertices;
extern std::vector<Entity> g_objects;
extern struct g_Meshes;
extern struct g_Textures;
extern struct g_FrameBuffers;
extern struct g_DynTextures;
extern struct g_buffers; // Refactor into different buffers

ParticleSource g_particleSource;
ParticleSource g_featherSource;

// Networking global stuff

//servergull server(1951);
//bool is_server = false;
U16 g_playerID = 0xffff;
seaclient g_client;

// overlay state
int  g_myHealth = 5;
int  g_enemyHealth = 5;
int g_maxHealth = 5;
bool g_gameOver = false;
U16  g_winner = 0xffff;
bool g_songActive = false;

// lobby
U16  g_p0_id = 0xffff, g_p1_id = 0xffff;
bool g_p0_ready = false, g_p1_ready = false;
bool g_sentReady = false;

// connect gate
static std::atomic<bool> g_connecting = false;
static std::string g_lastConnectError;
static double g_connectStarted = 0.0;

void tryConnect(const std::string& ip, U16 port) {
	if (g_client.IsConnected() || g_connecting.load()) return;
	g_connectStarted = glfwGetTime();
	g_lastConnectError.clear();
	g_connecting = true;

	g_playerID = 0xffff;
	bool ok = g_client.Connect(ip, port);
	if (!ok) {
		g_lastConnectError = "Failed to start connection";
		g_connecting = false; // only clear on immediate failure
	}
}

static void resetNetworkState() {
	g_playerID = 0xffff;
	g_p0_id = g_p1_id = 0xffff;
	g_p0_ready = g_p1_ready = false;
	g_sentReady = false;
	g_songActive = false;
	g_gameOver = false;
	g_winner = 0xffff;
	g_lastConnectError.clear();
	g_connecting = false;
}

// forward declarations
void cleanupFinishedSounds();
void playWithRandomPitch(ma_engine* engine, const char* filePath);
void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch = 1);
void playSoundVolume(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 volume = 1);
void throwCats();
void windowSizeCallback(GLFWwindow* window, int newWidth, int newHeight);
//void throw_cat(int cat_num, bool owned, F64);

const F64 DIST_BETWEEN_ENEMY_SHIPS = 100;
const glm::vec3 CAT_STARTING_POS(10.0, 0.0, 10.0);
const F64 DIST_BETWEEN_CATS = -5.0; // offset to catstartingpos's x axis
constexpr U32 CAT_NUM = 6;
const F64 BEATS_GRACE = .5;
B8 cannonCanFire[CAT_NUM];
B8 cannonNote[CAT_NUM];


glm::mat4 getCannonPos(U32 cannon_num, bool friendly) {
	if (friendly)
		return glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(CAT_STARTING_POS.x + (DIST_BETWEEN_CATS * cannon_num), CAT_STARTING_POS.y, CAT_STARTING_POS.z)), (F32) -PI/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	else
		return glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(CAT_STARTING_POS.x + (DIST_BETWEEN_CATS * cannon_num), CAT_STARTING_POS.y, CAT_STARTING_POS.z + DIST_BETWEEN_ENEMY_SHIPS)), (F32) PI/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
}

void makeSeagull(U8 note, U8 cannon, F64 timestamp) {
	// create an entity a while away from the cannon and move towards the cannon
	g_objects.push_back(Entity::create(&meshes.seagWalk2, textures.seagull, glm::translate(glm::rotate(getCannonPos(cannon, true), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(0.0,1.0,0.0)), PROECTILE));
	g_objects.back().startTime = timestamp;
	g_objects.back().preTransModel = g_objects.back().model;
	g_objects.back().update = [cannon, note](Entity& cat, F64 curtime) {
		F64 beatsFromFire = (((songStartTime + cat.startTime) - curTimeSec) / song_spb);
		U8 beatsLeft = (U8)glm::floor(beatsFromFire);


		glm::mat4 transform(1.0);
		if (beatsLeft > SHOW_NUM_BEATS) {
			transform = glm::translate(glm::mat4(1.0), glm::vec3(100000)); // why are we making them invisible like this
		} else if (beatsLeft <= 1) {
			cat.mesh = &meshes.seagBall;
			F64 dist = SEAGULL_MOVE_PER_BEAT;
			// TODO do the jumping animation here
			transform = glm::translate(toModel(
				glm::clamp((F32)(1+(curtime - (songStartTime + cat.startTime - song_spb)) / song_spb), 0.f, 1.f) * SEAGULL_MOVE_PER_BEAT, // distance along lane
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
			transform = glm::translate(transform, glm::vec3(0.0, 0.0, -SEAGULL_MOVE_PER_BEAT * (beatsLeft - 1)));
			cat.mesh = (beatsLeft % 2) ? &meshes.seagWalk2 : &meshes.seagWalk3;
		}
		cat.model = transform * cat.preTransModel;

		F64 grace_period = BEATS_GRACE * song_spb;
		B8 ccf = (abs(beatsFromFire - 1) < BEATS_GRACE);
		if (catsThrown[cannon] && ccf) {
			return false;
		}
		cannonCanFire[cannon] |= ccf;
		if (beatsFromFire <= 1-BEATS_GRACE) {
			cannonCanFire[cannon] = 0;
		}
		cannonNote[cannon] = note;

		//return (cat.model[3][1] >= 0.0f);
		return beatsFromFire > 1-BEATS_GRACE;
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
	GLFWwindow* window = init_window(g_width, g_height);
	initMouse(window); // function in cam.h
	glfwUpdateGamepadMappings("03000000ba1200004b07000000000000,Guitar Hero,platform:Windows,a:b1,b:b2,x:b3,y:b0,back:b4,start:b5,dpdown:+a1,dpup:-a1");
	initDefaultTexture();
	initGraphics(g_width, g_height);

	// Set up window callbacks **NO LONGER IN init_window!!!**

	glfwSetWindowSizeCallback(window, windowSizeCallback);
	glfwSetKeyCallback(window, keyCallback);

	// Create geometry

	std::string songName;
	std::vector<midi_note> notes = midiParseFile("asset/Buddy Holly riff.mid", songName);

	for (int i = 0; i < notes.size(); i++) {
		std::cout << notes[i].time << "s: " << (int)notes[i].note << "\n";
	}

	g_objects.push_back(Entity::create(&meshes.shipNoMast, textures.ship.color, textures.ship.norm, glm::translate(glm::rotate(glm::radians(90.0F), glm::vec3(0.0F, 1.0F, 0.0F)), glm::vec3(5.0F, -4.1F, 0.0F)), NONEMITTER));
	g_objects.push_back(Entity::create(&meshes.ship, textures.ship.color, textures.ship.norm, glm::translate(glm::rotate(glm::radians(90.0F), glm::vec3(0.0F, 1.0F, 0.0F)), glm::vec3(-25.0F - DIST_BETWEEN_ENEMY_SHIPS, -4.1F, 0.0F)), NONEMITTER));
	g_objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), NONEMITTER));

	for (int i = 0; i < 6; i++) {
		//objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(get_cannon_pos(i, true), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), CANNON, true));
		g_objects.push_back(Entity::create(&meshes.cannon, textures.cannon.color, textures.cannon.norm, getCannonPos(i, true), CANNON, true));
		g_cannonsFriendly[i] = &g_objects.back();
		//objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(get_cannon_pos(i, false), (float)PI, glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), CANNON));
		g_objects.push_back(Entity::create(&meshes.cannon, textures.cannon.color, textures.cannon.norm, getCannonPos(i, false), CANNON));
		g_cannonsEnemy[i] = &g_objects.back();
	}

	std::string serverIP = "136.112.101.5";
	// try_connect(server_ip, 1951);

	Entity water = Entity::create(&meshes.quad, default_tex, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, WATER_HEIGHT, 0.0f)), glm::vec3(500.0, 500.0, 500.0)), NONEMITTER); // TODO water should have its own normal map thing

	// Create static particle sources (later change this to be dynamic or something)
	g_particleSource = { glm::vec3(0.0F, 2.0F, 0.0F), glm::vec3(0.1f), RGBA8 { 255,255,255,255 }, 1.0f, 1.0f, 0 }; // live for 1 seconds
	g_particleSource.setSheetRes(8, 8);
	g_particleSource.scaleOverTime = 0.9;
	g_particleSource.gravity = 0.0;
	g_particleSource.drag = 0.9;
	g_particleSource.initVelScale = 3.0;

	g_featherSource = { glm::vec3(0.0), glm::vec3(0.0), RGBA8 {255,255,255,255}, 0.6f, 2.0f, 1 }; // live for 2 seconds
	g_featherSource.randomRotation = 2.0f * PI;
	g_featherSource.randomRotationOverTime = 10.0;
	g_featherSource.gravity = 10.0;
	g_featherSource.drag = 0.95;
	g_featherSource.initVelScale = 8.0;

	// Bind textures to particle array
	particle_textures[0] = textures.particleExplosion;
	particle_textures[1] = textures.feather;
	particle_textures[2] = textures.cannonExplosion;

	DirectionalLight sun = DirectionalLight{};
	sun.illuminateArea(150.0);

	double lastTimeSec = 0.0;
	int songstart;

	/// Initialize midi
	libremidi::observer obs;
	libremidi::midi_in midi{
		libremidi::input_configuration{.on_message = midiCallback }
	};
	std::cout << "midi\n";
	if (obs.getInputPorts().size()) {
		midiInit(midi);
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
		double dt = curTimeSec - lastTimeSec;
		lastTimeSec = curTimeSec;
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
			moveCamGamepad(window, g_cam, dt, state);
			moveCamKeyboard(window, g_cam, dt);
			if (midiExists) {
				extern std::vector<char> midiKeysVelocity;
				moveCamMidi(window, g_cam, dt);
			}
		}
		

		for (int i = 0; i < g_objects.size(); i++) {
			if (g_objects[i].update) {
				if (!g_objects[i].update(g_objects[i], curTimeSec)) {
					g_objects.erase(g_objects.begin() + i);
					i--;
				}
			}
		}

		throwCats();

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
		if (g_client.IsConnected()) {
			g_client.checkMessages();
		}
		
		if (g_client.IsConnected()) {
			g_connecting = false;
		}
		else if (g_connecting && (glfwGetTime() - g_connectStarted) > 5.0) {
			g_connecting = false;
			g_lastConnectError = "Timed out connecting to " + serverIP;
			g_client.Disconnect();
			resetNetworkState();
		}



		// Update particles
		advanceParticles(dt);
		//particleSource.spawnParticle();
		sortParticles(g_cam);
		packParticles();
		send_particle_data_to_gpu();

		// get cam matrices
		glm::mat4 view = glm::lookAt(g_cam.pos, g_cam.pos + g_cam.lookDir(), cam_up);
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)(g_width) / g_height, 0.1f, 1000.0f);

		// Update the light direction
		sun.setLightDirVec3(lightDir);

		// Update the shadowmap texture
		render_shadows(sun);

		// Update the reflection texture
		{
			glm::vec3 modified_pos = g_cam.pos; modified_pos.y = 2 * WATER_HEIGHT - modified_pos.y;
			glm::vec3 modified_look_dir = glm::vec3(sin(g_cam.theta) * cos(-g_cam.y_theta), sin(-g_cam.y_theta), cos(g_cam.theta) * cos(-g_cam.y_theta));
			glm::mat4 modified_view = glm::lookAt(modified_pos, modified_pos + modified_look_dir, glm::vec3(0.0f, -1.0f, 0.0f));

			glBindFramebuffer(GL_FRAMEBUFFER, framebuffers.reflection_framebuffer);
			glViewport(0, 0, g_width, g_height);
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
				auto _model = glm::rotate(glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(g_cam.pos.x, g_cam.pos.y, g_cam.pos.z)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.2f, 0.2f, 0.2f)), g_cam.theta, glm::vec3(0.0, 0.0, 1.0));
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
		glViewport(0, 0, g_width, g_height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		draw_skybox(projection, view);
		draw_objects(projection, view, sun, g_cam.pos);
		draw_water(projection, view, sun, g_cam.pos, curTimeSec, water);
		draw_particles(projection, view);

		// TEMP UI FIX
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;

		healthbars(flags, g_width, g_height, g_myHealth, g_enemyHealth, g_maxHealth, g_gameOver, g_winner);

		if (menuOpen) {
			menu(flags, g_width, g_height, serverIP, g_connecting, g_lastConnectError, &volume, window);
			ma_engine_set_volume(&audioEngine, volume / 100.f);
		}
		else {
			menuClose(flags, window);
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

void throwCats() {
	bool send = false;
	std::vector<uint8_t> cats;

	for (int i = 0; i < numCats; i++) {
		if (catsThrown[i]) {
			if (cannonCanFire[i] || g_INFINITE_FIRE) { // TODO remove this later
				throw_cat(i, true); // local projectile + sfx
				cats.push_back(static_cast<uint8_t>(i));
				send = true;
				cannonCanFire[i] = false;
			}
			catsThrown[i] = false;
		}
	}

	if (send && g_client.IsConnected() && g_playerID != 0xffff) {
		g_client.send_player_cat_fire(g_playerID, curTimeSec, cats);
	}
}

void throw_cat(int catNum, bool owned, double the_note_that_this_cat_was_played_to_is_supposed_to_be_played_at_time) {
	F64 start_time = curTimeSec;
	F64 time_diff = curTimeSec - the_note_that_this_cat_was_played_to_is_supposed_to_be_played_at_time;

	// Prefer a cannon whose owned flag matches, otherwise use any matching cat_id.
	int best = -1, fallback = -1;
	for (int j = 0; j < (int)g_objects.size(); ++j) {
		if (g_objects[j].type == CANNON && g_objects[j].catID == catNum) {
			if (g_objects[j].owned == owned) { best = j; break; }
			if (fallback == -1) fallback = j;
		}
	}
	const int i = (best != -1 ? best : fallback);
	if (i == -1) return; // no suitable cannon found

	playSound(&audioEngine, "asset/cat-meow-401729-2.wav", false, noteMultiplier((U8)72,cannonNote[catNum]));
	playSoundVolume(&audioEngine, "asset/cannon.wav", false, 0.3);

	glm::vec4 pos = g_objects[i].model[3];
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
	g_objects.push_back(Entity::create(&meshes.seagBall, textures.seagull, g_objects[i].model, PROECTILE));
	Entity& p = g_objects.back();

	p.startTime = start_time;
	p.preTransModel = p.model;
	p.shootAngle = owned ? 0.0f : PI;

	p.update = [catNum](Entity& cat, F64 curTime) {
		cat.model = toModel(
			(curTime - cat.startTime) * 50, // distance along lane
			0,                               // lane index
			DIST_BETWEEN_ENEMY_SHIPS,
			cat.shootAngle
		) * cat.preTransModel;

		glm::vec3 self_pos = glm::vec3(cat.model[3]);
		for (int i = 0; i < g_objects.size(); i++) {
			if (g_objects[i].type != PROECTILE || // only collide with seagulls
				//objects[i].owned == cat.owned // don't collide with own faction
				g_objects[i].shootAngle == cat.shootAngle
			) continue;
			glm::vec3 enemy_pos = glm::vec3(g_objects[i].model[3]);
			F32 dist = glm::length(self_pos - enemy_pos);
			if (dist < 5.0) {
				g_featherSource.pos = self_pos;
				g_featherSource.spawnParticles(75);
				g_objects[i].markedForDeath = true;
				return false;
			}
		}

		// Die if marked for death
		if (cat.markedForDeath) {
			g_featherSource.pos = self_pos;
			g_featherSource.spawnParticles(75);
			return false;
		}
		// keep alive while above ground
		if (cat.model[3][1] < 2.0f && (curTime - cat.startTime) > 1.0) {
			g_particleSource.pos = self_pos;
			g_particleSource.spawnParticles(75);
			return false;
		}

		return true;
	};
}

void cleanupFinishedSounds() {
	for (auto it = g_liveSounds.begin(); it != g_liveSounds.end();) {
		ma_sound* s = *it;
		if (!ma_sound_is_playing(s) && ma_sound_at_end(s)) {
			ma_sound_uninit(s);
			delete s;
			it = g_liveSounds.erase(it);
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
		ma_sound_set_pitch(s, g_randomPitch(g_rng));
		ma_sound_start(s);
		g_liveSounds.push_back(s);
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
		g_liveSounds.push_back(s);
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
		g_liveSounds.push_back(s);
	}
	else {
		delete s;
	}
}



/* Graphics Functions */

void windowSizeCallback(GLFWwindow* window, int newWidth, int newHeight) {
	g_width = newWidth;
	g_height = newHeight;
	initWaterFramebuffer(g_width, g_height);
}
