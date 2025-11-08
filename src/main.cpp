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

//#include "server.h"
#include "client.h"
#include "message.h"

#include "cam.h"
#include "debug.h"
#include "light.h"
#include "particle.h"
#include "game.h"
#include "music.h"
#include "input.h"

#include "midi.h"



#include <random>
static std::mt19937 rng{ std::random_device{}() };

static std::uniform_real_distribution<float> randomPitch(0.02f, 1.15f);


std::vector<ma_sound*> liveSounds;

FreeCam cam = FreeCam{ Cam { glm::vec3(0.0f,1.0f,0.0f), 0.0, 0.0 } };

Entity* cannons_friend[6];
Entity* cannons_enemy[6];
// F32 weezer[] = { 1.f, 1.05943508007, 1.f, 1.33482398807, 1.4982991247, 1.33482398807, 1.f, 0.89087642854, 0.79367809502, 1.f };

// Static data
static int width = 1920;
static int height = 1080;
static GLuint vao;
ma_engine engine;
double cur_time_sec;
bool menu_open = true;
std::vector<Entity> objects;
static struct {
	Mesh test_scene;
	Mesh cat;
	Mesh quad;
	Mesh seagWalk2;
	Mesh seagWalk3;
	Mesh cannon;
	Mesh cannon_door;
	Mesh seagBall;
} meshes;
static struct {
	GLuint green;
	GLuint cat;
	GLuint skybox;
	GLuint banner;
	GLuint weezer;
	GLuint waterNormal;
	GLuint waterOffset;
	GLuint particleExplosion;
	struct {
		GLuint color;
		GLuint norm;
		GLuint arm;
	} cannon;
	GLuint seagull;
  struct {
    GLuint menu_logo;
		GLuint context;
		GLuint page;
		GLuint connect;
		GLuint leave;
		GLuint closeMenu;
  } menu;
} textures;

// Networking global stuff

//servergull server(1951);
//bool is_server = false;
U16 player_id = 0xffff;
seaclient client;

// overlay state
int  g_my_health = 5;
int  g_enemy_health = 5;
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
	g_connecting = true;
	g_last_connect_error.clear();

	std::thread([ip, port]() {
		try {
			player_id = 0xffff;           // ensure HELLO is sent after reconnect
			client.Connect(ip, port);
		}
		catch (const std::exception& e) {
			g_last_connect_error = e.what();
		}
		catch (...) {                   // FIXED from catch (.)
			g_last_connect_error = "Unknown error while connecting";
		}
		g_connecting = false;
		}).detach();
}

// Reflections
GLuint reflection_framebuffer;
GLuint reflection_tex, reflection_depth_tex;

// forward declarations
GLFWwindow* init();
void cleanup(GLFWwindow* window);
std::string readFile(const char* path);
glm::mat4 baseTransform(const std::vector<Vertex>& vertices);
void genTangents(std::vector<Vertex>& vertices);
void cleanupFinishedSounds();
void playWithRandomPitch(ma_engine* engine, const char* filePath);
void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch = 1);
void playSoundVolume(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 volume = 1);
void throw_cats();
//void throw_cat(int cat_num, bool owned, F64);
void initWaterFramebuffer();

const F64 distancebetweenthetwoshipswhichshallherebyshootateachother = 100;
const glm::vec3 catstartingpos(10.0, 0.0, 10.0);
const F64 distbetweencats = -5.0; // offset to catstartingpos's x axis
constexpr U32 catnumber = 6;
const F64 beats_grace = .5;
B8 cannon_can_fire[catnumber];


glm::mat4 get_cannon_pos(U32 cannon_num, bool friendly) {
	if (friendly)
		return glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(catstartingpos.x + (distbetweencats * cannon_num), catstartingpos.y, catstartingpos.z)), (F32) -PI/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	else
		return glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(catstartingpos.x + (distbetweencats * cannon_num), catstartingpos.y, catstartingpos.z + distancebetweenthetwoshipswhichshallherebyshootateachother)), (F32) PI/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
}

void make_seagull(U8 cannon, F64 timestamp) {
	// create an entity a while away from the cannon and move towards the cannon
	objects.push_back(Entity::create(&meshes.seagWalk2, textures.seagull, glm::translate(glm::rotate(get_cannon_pos(cannon, true), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(0.0,1.0,0.0)), PROECTILE));
	objects.back().start_time = timestamp;
	objects.back().pretransmodel = objects.back().model;
	objects.back().update = [cannon](Entity& cat, F64 curtime) {
		F64 beats_from_fire = (((song_start_time + cat.start_time) - cur_time_sec) / song_spb);
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
		cannon_can_fire[cannon] = ccf;

		//return (cat.model[3][1] >= 0.0f);
		return beats_from_fire > 1-beats_grace;
		};
}
// Create a shader from vertex and fragment shader files

GLuint createShader(const char* vsPath, const char* fsPath = nullptr) {
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = 0;

	std::string vsSource = readFile(vsPath);
	const char* vsSource_cstr = vsSource.c_str();

	glShaderSource(vs, 1, &vsSource_cstr, nullptr);
	glCompileShader(vs);
	getCompileStatus(vs);

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);

	if (fsPath) {
		GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

		std::string fsSource = readFile(fsPath);
		const char* fsSource_cstr = fsSource.c_str();

		glShaderSource(fs, 1, &fsSource_cstr, nullptr);

		glCompileShader(fs);
		getCompileStatus(fs);

		glAttachShader(program, fs);
	}

	glLinkProgram(program);
	getLinkStatus(program);

	glDetachShader(program, vs);
	glDeleteShader(vs);

	return program;
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
	GLFWwindow* window = init();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	initMouse(window); // function in cam.h
	glfwUpdateGamepadMappings("03000000ba1200004b07000000000000,Guitar Hero,platform:Windows,a:b0,b:b1,x:b2,y:b3,dpdown:+a1,dpup:-a1");
	initDefaultTexture();

	GLuint program = createShader("src/shader/triangle.vert", "src/shader/triangle.frag");
	GLuint shadowShader = createShader("src/shader/shadow.vert");
	GLuint cubeProgram = createShader("src/shader/cube.vert", "src/shader/cube.frag");
	GLuint particleProgram = createShader("src/shader/particle.vert", "src/shader/particle.frag");
	GLuint waterProgram = createShader("src/shader/water.vert", "src/shader/water.frag");

	// Create textures (and frame buffers)

	GLuint framebuffer;
	glCreateFramebuffers(1, &framebuffer);
	GLuint shadowmap; int shadowmap_width = 2048; int shadowmap_height = 2048;
	{
		shadowmap = createTexture(shadowmap_width, shadowmap_height, GL_DEPTH_COMPONENT32F, false, true, nullptr);
		glTextureParameteri(shadowmap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(shadowmap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		F32 border[]{ 9999999.0F, 9999999.0F, 9999999.0F, 9999999.0F };
		glTextureParameterfv(shadowmap, GL_TEXTURE_BORDER_COLOR, border);

		glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, shadowmap, 0);
		glBindTextureUnit(1, shadowmap);
	}

	// create buffer objects for water reflection and refraction; later on they will be rendered to and combined to create the water texture
	glCreateFramebuffers(1, &reflection_framebuffer);
	initWaterFramebuffer();

	// Create geometry

	std::vector<Vertex> vertices;

	meshes.test_scene = Mesh::create(vertices, "asset/test_scene.obj");
	meshes.cat = Mesh::create(vertices, "asset/cat.obj");
	meshes.quad = Mesh::xzQuad(vertices);
	meshes.seagWalk2 = Mesh::create(vertices, "asset/seagull/seagull_walk2.obj");
	meshes.seagWalk3 = Mesh::create(vertices, "asset/seagull/seagull_walk3.obj");
	meshes.cannon = Mesh::create(vertices, "asset/cannon/cannon.obj");
	meshes.cannon_door = Mesh::create(vertices, "asset/cannon/cannon_door.obj");
	meshes.seagBall = Mesh::create(vertices, "asset/seagull/seagull.obj");

	textures.green = createTextureFromImage("asset/green.jpg");
	textures.cat = createTextureFromImage("asset/cat.jpg");
	textures.seagull = createTextureFromImage("asset/seagull/seag_tex.png");
	textures.particleExplosion = createTextureFromImage("asset/particle_explosion.png");

	textures.waterNormal = createTextureFromImage("asset/waterNormal.png");
	textures.waterOffset = createTextureFromImage("asset/waterOffset.png");

	stbi_set_flip_vertically_on_load(false);
	textures.menu.menu_logo = createTextureFromImage("asset/menu_logo.png");
	textures.menu.page = createTextureFromImage("asset/page.png");
	textures.menu.context = createTextureFromImage("asset/context.png");
	textures.menu.connect = createTextureFromImage("asset/ConnectButton.png");
	textures.menu.leave = createTextureFromImage("asset/LeaveButton.png");
	textures.menu.closeMenu = createTextureFromImage("asset/Close-Menu.png");

	textures.weezer = createTextureFromImage("asset/weezer.jfif");
	textures.banner = createTextureFromImage("asset/seagull_banner.png");
	stbi_set_flip_vertically_on_load(true);

	textures.cannon.color = createTextureFromImage("asset/cannon/cannon_BaseColor.jpg");
	textures.cannon.norm = createTextureFromImage("asset/cannon/cannon_Normal.jpg");
	textures.cannon.arm = createTextureFromImage("asset/cannon/cannon_ARM.jpg");

	std::string song_name;
	std::vector<midi_note> notes = midi_parse_file("asset/Buddy Holly riff.mid", song_name);

	for (int i = 0; i < notes.size(); i++) {
		std::cout << notes[i].time << "s: " << (int)notes[i].note << "\n";
	}


	objects.push_back(Entity::create(&meshes.test_scene, textures.green));
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

	Entity water = Entity::create(&meshes.quad, default_tex, glm::scale(glm::mat4(1.0f), glm::vec3(500.0, 500.0, 500.0)), NONEMITTER); // TODO water should have its own normal map thing

	genTangents(vertices);

	{
		const char* cubemap_files[6] = {
			"asset/skybox/right.jpg",
			"asset/skybox/left.jpg",
			"asset/skybox/top.jpg",
			"asset/skybox/bottom.jpg",
			"asset/skybox/front.jpg",
			"asset/skybox/back.jpg"
		};
		textures.skybox = createCubeTexture(cubemap_files);
	}

	// Create static particle sources (later change this to be dynamic or something)

	ParticleSource particleSource{ glm::vec3(0.0F, 2.0F, 0.0F), glm::vec3(0.1f), RGBA8 { 255,255,255,255 }, 1.0f, 1.0f }; // live for 5 seconds

	GLuint buffer;
	glCreateBuffers(1, &buffer);
	glNamedBufferStorage(buffer, vertices.size() * sizeof(Vertex), vertices.data(), 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);

	GLuint pvertex_buffer;
	glCreateBuffers(1, &pvertex_buffer);
	glNamedBufferData(pvertex_buffer, sizeof(pvertex_vertex), pvertex_vertex, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pvertex_buffer);

	GLuint pdata_buffer;
	glCreateBuffers(1, &pdata_buffer);
	glNamedBufferData(pdata_buffer, sizeof(pvertex_data), pvertex_data, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pdata_buffer);


	DirectionalLight sun = DirectionalLight{};
	sun.illuminateArea(50.0);
	glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

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

	ma_engine_init(NULL, &engine);
	ma_engine_set_volume(&engine, volume/100.f);
	playSoundVolume(&engine, "asset/seagull-flock-sound-effect-206610.wav", MA_TRUE, 0.25f);
	//int val1 = 10, val2 = 0, val3 = 0, val4 = 153;
	static char buf[64];
	windowMouseRelease(window);
	// event loop (each iteration of this loop is one frame of the application)
	while (!glfwWindowShouldClose(window)) {
		// calculate delta time
		cur_time_sec = glfwGetTime();
		double dt = cur_time_sec - last_time_sec;
		last_time_sec = cur_time_sec;
		double lightAzimuth = glfwGetTime() / 3.0;
		glm::vec3 lightDir = getAngle(lightAzimuth, -PI / 4.0);

		// per-frame input
		glfwPollEvents();

		GLFWgamepadstate state{};
		if (!menu_open) {
			if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
				moveFreeCamGamepad(window, cam, dt, state);
			}
			else {
				moveFreeCam(window, cam, dt);
			}
			if (midi_exists) {
				extern std::vector<char> midi_keys_velocity;
				moveFreeCamMidi(window, cam, dt);
			}
		}

		for (int i = 0; i < objects.size(); i++) {
			if (objects[i].update) {
				if (!objects[i].update(objects[i], cur_time_sec)) {
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
		}


		// Update particles
		advanceParticles(dt);
		particleSource.spawnParticle();
		sortParticles(cam.cam, cam.cam.lookDir());
		packParticles();

		glNamedBufferSubData(pvertex_buffer, 0, sizeof(ParticleVertex) * lastUsedParticle * VERTICES_PER_PARTICLE, pvertex_vertex);
		glNamedBufferSubData(pdata_buffer, 0, sizeof(ParticleData) * lastUsedParticle, pvertex_data);

		// get cam matrices
		glm::mat4 view = glm::lookAt(cam.cam.pos, cam.cam.pos + cam.cam.lookDir(), cam_up);
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)(width) / height, 0.1f, 1000.0f);

		// Update the light direction
		sun.setLightDirVec3(lightDir);

		// Draw to shadow map framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glViewport(0, 0, shadowmap_width, shadowmap_height);
		glClearDepth(9999999.0);
		glClear(GL_DEPTH_BUFFER_BIT);
		glClearDepth(1.0);

		glUseProgram(shadowShader);

		for (int i = 0; i < objects.size(); i++) {
			Entity& o = objects[i];

			glProgramUniformMatrix4fv(shadowShader, 0, 1, GL_FALSE, glm::value_ptr(o.model));
			glProgramUniformMatrix4fv(shadowShader, 4, 1, GL_FALSE, glm::value_ptr(sun.combined));

			glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
		}

		// Draw to water texture framebuffers
		{
			glm::vec3 modified_pos = cam.cam.pos; modified_pos.y = 0.0f - modified_pos.y;
			glm::vec3 modified_look_dir = glm::vec3(sin(cam.cam.theta) * cos(-cam.cam.y_theta), sin(-cam.cam.y_theta), cos(cam.cam.theta) * cos(-cam.cam.y_theta));
			glm::mat4 modified_view = glm::lookAt(modified_pos, modified_pos + modified_look_dir, glm::vec3(0.0f, -1.0f, 0.0f));

			glBindFramebuffer(GL_FRAMEBUFFER, reflection_framebuffer);

			glViewport(0, 0, width, height);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);

			// Draw the skybox
			glUseProgram(cubeProgram);

			glBindTextureUnit(2, textures.skybox);

			glProgramUniformMatrix4fv(cubeProgram, 0, 1, GL_FALSE, glm::value_ptr(projection * glm::mat4(glm::mat3(modified_view))));

			glDrawArrays(GL_TRIANGLES, 0, 36); // number of vertices in a cube; aka a magic number

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			glEnable(GL_CLIP_DISTANCE0);

			// Draw scene
			glUseProgram(program);

			glProgramUniformMatrix4fv(program, 4, 1, GL_FALSE, glm::value_ptr(projection * modified_view));
			glProgramUniformMatrix4fv(program, 11, 1, GL_FALSE, glm::value_ptr(sun.combined));
			glProgramUniform3fv(program, 15, 1, glm::value_ptr(modified_pos));
			glProgramUniform3fv(program, 16, 1, glm::value_ptr(lightDir));
			glProgramUniform3fv(program, 17, 1, glm::value_ptr(lightColor));
			glProgramUniform2f(program, 18, (F32)shadowmap_height, (F32)shadowmap_width);
			glProgramUniform1i(program, 19, true);

			for (int i = 0; i < objects.size(); i++) {
				Entity& o = objects[i];
				glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
				glBindTextureUnit(0, objects[i].tex);
				glBindTextureUnit(2, objects[i].normal);

				glProgramUniformMatrix4fv(program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
				glProgramUniformMatrix3fv(program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

				glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
			}

			// Draw the player as a cat
			{
				auto _model = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), cam.cam.pos), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.2f, 0.2f, 0.2f));
				Entity o = Entity::create(&meshes.cat, textures.cat, _model, NONEMITTER);
				glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
				glBindTextureUnit(0, o.tex);
				glBindTextureUnit(2, o.normal);

				glProgramUniformMatrix4fv(program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
				glProgramUniformMatrix3fv(program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

				glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
			}

			// Draw particles
			glUseProgram(particleProgram);

			glProgramUniformMatrix4fv(particleProgram, 0, 1, GL_FALSE, glm::value_ptr(modified_view));
			glProgramUniformMatrix4fv(particleProgram, 4, 1, GL_FALSE, glm::value_ptr(projection));
			glBindTextureUnit(0, textures.particleExplosion);

			glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_PARTICLE* lastUsedParticle); // where lastUsedParticle is the number of particles

			glDisable(GL_CLIP_DISTANCE0);
		}

		// Draw to screen
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		// Draw the skybox
		glUseProgram(cubeProgram);

		glBindTextureUnit(2, textures.skybox);

		glProgramUniformMatrix4fv(cubeProgram, 0, 1, GL_FALSE, glm::value_ptr(projection * glm::mat4(glm::mat3(view))));

		glDrawArrays(GL_TRIANGLES, 0, 36); // number of vertices in a cube; aka a magic number

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);

		// Draw scene
		glUseProgram(program);

		glProgramUniformMatrix4fv(program, 4, 1, GL_FALSE, glm::value_ptr(projection * view));
		glProgramUniformMatrix4fv(program, 11, 1, GL_FALSE, glm::value_ptr(sun.combined));
		glProgramUniform3fv(program, 15, 1, glm::value_ptr(cam.cam.pos));
		glProgramUniform3fv(program, 16, 1, glm::value_ptr(lightDir));
		glProgramUniform3fv(program, 17, 1, glm::value_ptr(lightColor));
		glProgramUniform2f(program, 18, (F32)shadowmap_height, (F32)shadowmap_width);
		glProgramUniform1i(program, 19, false);

		for (int i = 0; i < objects.size(); i++) {
			Entity& o = objects[i];
			glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
			glBindTextureUnit(0, objects[i].tex);
			glBindTextureUnit(2, objects[i].normal);

			glProgramUniformMatrix4fv(program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
			glProgramUniformMatrix3fv(program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

			glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
		}

		// Draw water
		glUseProgram(waterProgram);

		glProgramUniformMatrix4fv(waterProgram, 4, 1, GL_FALSE, glm::value_ptr(projection* view));
		glProgramUniformMatrix4fv(waterProgram, 11, 1, GL_FALSE, glm::value_ptr(sun.combined));
		glProgramUniform3fv(waterProgram, 15, 1, glm::value_ptr(cam.cam.pos));
		glProgramUniform3fv(waterProgram, 16, 1, glm::value_ptr(lightDir));
		glProgramUniform3fv(waterProgram, 17, 1, glm::value_ptr(lightColor));
		glProgramUniform2f(waterProgram, 18, (F32)shadowmap_height, (F32)shadowmap_width);
		glProgramUniform1f(waterProgram, 19, cur_time_sec);

		glBindTextureUnit(2, textures.waterNormal);
		glBindTextureUnit(3, textures.waterOffset);

		{
			Entity& o = water;
			glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
			glBindTextureUnit(0, reflection_tex);

			glProgramUniformMatrix4fv(waterProgram, 0, 1, GL_FALSE, glm::value_ptr(o.model));
			glProgramUniformMatrix3fv(waterProgram, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

			glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
		}

		// Draw particles
		glUseProgram(particleProgram);

		glProgramUniformMatrix4fv(particleProgram, 0, 1, GL_FALSE, glm::value_ptr(view));
		glProgramUniformMatrix4fv(particleProgram, 4, 1, GL_FALSE, glm::value_ptr(projection));
		glBindTextureUnit(0, textures.particleExplosion);

		glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_PARTICLE * lastUsedParticle); // where lastUsedParticle is the number of particles

		// TEMP UI FIX
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;

		ImGui::SetNextWindowSize(ImVec2(1000, 1000));
		ImGui::SetNextWindowPos(ImVec2((width - 728.0f) / 2, height * 0.01));
		ImGui::Begin("State", NULL, flags);
		ImGui::Image((ImTextureID)textures.banner, ImVec2(728.0f, 90.0f));
		ImGui::End();


		/*if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
			songSelect(textures.weezer, "asset/weezer-riff.wav", ImVec2(637, 640));
			playSound(&engine, "asset/weezer-riff.wav", MA_FALSE);
		}*/


		if (menu_open) {
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
						g_connecting = true;
						g_connect_started = glfwGetTime();
						try_connect(server_ip, 1951);
					}
				}
				else {
					ImGui::SetCursorPos(ImVec2(inputx, inputy + spacing));
					ImGui::Text("Connecting to %s...", server_ip.c_str());
					ImGui::SetCursorPos(ImVec2(inputx, inputy + (2 *  spacing)));
					if (ImGui::Button("Cancel")) { client.Disconnect(); } // leave g_connecting; thread will clear it :     )
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
			

			ImGui::Text("Player 0: %s  [%s]",
				g_p0_id == 0xffff ? "(empty)" : std::to_string(g_p0_id).c_str(),
				g_p0_ready ? "Ready" : "Not Ready");
			ImGui::Text("Player 1: %s  [%s]",
				g_p1_id == 0xffff ? "(empty)" : std::to_string(g_p1_id).c_str(),
				g_p1_ready ? "Ready" : "Not Ready");

			const bool i_am_player0 = (player_id != 0xffff && player_id == g_p0_id);
			const bool i_am_player1 = (player_id != 0xffff && player_id == g_p1_id);
			const bool i_am_player = i_am_player0 || i_am_player1;
			const bool slot_available = (g_p0_id == 0xffff) || (g_p1_id == 0xffff);

			if (!g_song_active && (i_am_player || slot_available))
			{
				if (!g_sent_ready)
				{
					if (ImGui::Button("Start Game"))
					{
						cgull::net::message<message_code> m;
						m.header.id = message_code::PLAYER_READY;
						U16 pid = player_id; // 16-bit id
						m << pid;
						if (client.IsConnected()) client.Send(m);
						g_sent_ready = true;
					}
					ImGui::SameLine(); ImGui::TextDisabled("(press when ready)");
				}
				else
				{
					ImGui::TextDisabled("Waiting for the other player...");
				}
			}
			else
			{
				ImGui::TextDisabled(g_song_active ? "Match in progress" : "Spectating (button disabled)");
			}
			
			
			ImGui::SetCursorPos(ImVec2(inputx, inputy + (spacing * 2) + buttonh));
			ImGui::PushItemWidth(inputw*4);
			ImGui::SliderFloat("Volume", &volume, 0, 256);
			ImGui::PopItemWidth();
			
			ImGui::SetCursorPos(ImVec2((width - 200 - 100) / 2, height - 350));
			ImGui::Image((ImTextureID)textures.menu.closeMenu, ImVec2(100, 100));
			ImGui::SetCursorPos(ImVec2((width - 200 - 100) / 2, height - 350));
			if (ImGui::InvisibleButton("Close Menu", ImVec2(100, 100))) {
				menu_open = false;
				windowMouseFocus(window);
			}
			ImGui::End();
			ma_engine_set_volume(&engine, volume / 100.f);
		}
		else {
			ImGui::SetNextWindowSize(ImVec2(50, 50));
			ImGui::SetNextWindowPos(ImVec2(50, 50));
			ImGui::Begin("open menu", NULL, flags);
			if (ImGui::Button("Menu")) {
				menu_open = true;
				windowMouseRelease(window);
			}
			ImGui::End();

		}

		ImGui::SetNextWindowSize(ImVec2(500, 500));
		ImGui::SetNextWindowPos(ImVec2(20, 20));
		ImGui::Begin("ip", NULL, flags);
		
		ImGui::End();

		ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("My HP: %d", g_my_health);
		ImGui::Text("Enemy HP: %d", g_enemy_health);
		if (g_game_over) {
			ImGui::Separator();
			if (g_winner == 0xffff) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Game Over");
			else if (g_winner == player_id) ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "You Win!");
			else ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "You Lose!");
		}
		ImGui::End();

		ImGui::SetNextWindowSize(ImVec2(420, 170), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Connection", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static bool ipbuf_init = false;
			static char ipbuf[64]{};
			if (!ipbuf_init) { std::snprintf(ipbuf, sizeof(ipbuf), "%s", server_ip.c_str()); ipbuf_init = true; }
			if (ImGui::InputText("Server IP", ipbuf, sizeof(ipbuf))) { server_ip = ipbuf; }

			if (!client.IsConnected())
			{
				if (!g_connecting)
				{
					if (!g_last_connect_error.empty())
						ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", g_last_connect_error.c_str());

					if (ImGui::Button("Join Game")) {
						g_connecting = true;
						g_connect_started = glfwGetTime();
						try_connect(server_ip, 1951);
					}
				}
				else
				{
					ImGui::Text("Connecting to %s...", server_ip.c_str());
					if (ImGui::Button("Cancel")) { client.Disconnect(); } 
				}
			}
			else
			{
				if (ImGui::Button("Disconnect")) { client.Disconnect(); }
			}
		}
		ImGui::End();

		// Status
		if (ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("My HP: %d", g_my_health);
			ImGui::Text("Enemy HP: %d", g_enemy_health);
			if (g_game_over)
			{
				ImGui::Separator();
				if (g_winner == 0xffff) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Game Over");
				else if (g_winner == player_id) ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "You Win!");
				else ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "You Lose!");
			}
		}
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// present
		glfwSwapBuffers(window);

	}

	//glDeleteFrameBuffers(1, &framebuffer);
	glDeleteBuffers(1, &buffer);
	//glDeleteTextures(1, &tex); // TODO delete all textures here
	glDeleteProgram(program);
	glDeleteProgram(shadowShader);

	ma_engine_uninit(&engine);

	cleanup(window);
}

void draw_cannon(glm::vec3 pos) {
	glDrawArrays(GL_TRIANGLES, meshes.cannon.offset, meshes.cannon.size);
}

void throw_cats() {
	bool send = false;
	std::vector<uint8_t> cats;

	for (int i = 0; i < numcats; i++) {
		if (cats_thrown[i]) {
			if (cannon_can_fire[i]) {
				throw_cat(i, true); // local projectile + sfx
				cats.push_back(static_cast<uint8_t>(i));
				send = true;
				cannon_can_fire[i] = false;
			}
			cats_thrown[i] = false;
		}
	}

	if (send && client.IsConnected() && player_id != 0xffff) {
		client.send_player_cat_fire(player_id, cur_time_sec, cats);
	}
}

void throw_cat(int cat_num, bool owned, double start_time) {
	if (start_time < 0.0) start_time = cur_time_sec;

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

	playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[cat_num]);

	// Spawn projectile using the chosen cannon's transform
	objects.push_back(Entity::create(&meshes.seagBall, textures.seagull, objects[i].model, PROECTILE));
	Entity& p = objects.back();

	p.start_time = start_time;
	p.pretransmodel = p.model;
	p.shoot_angle = owned ? 0.0f : PI;

	p.update = [](Entity& cat, F64 curtime) {
		cat.model = toModel(
			(curtime - cat.start_time) * 50, // distance along lane
			0,                               // lane index
			distancebetweenthetwoshipswhichshallherebyshootateachother,
			cat.shoot_angle
		) * cat.pretransmodel;

		// keep alive while above ground
		return (cat.model[3][1] >= 0.0f);
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


/* Gameplay related functions */

// Given an object, give a transform matrix that roughly scales it down to appropriate size; don't use for percise transformations
glm::mat4 baseTransform(const std::vector<Vertex>& vertices) {
	glm::vec3 minPoint(INFINITY);
	glm::vec3 maxPoint(-INFINITY);

	for (Vertex v : vertices) {
		minPoint = glm::min(minPoint, v.position);
		maxPoint = glm::max(maxPoint, v.position);
	}

	const glm::vec3 center = (maxPoint + minPoint) / 2.0f;
	const glm::vec3 size = maxPoint - minPoint;
	const float scale = 1.0f / std::max(size.x, std::max(size.y, size.z));

	return glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -center);
}

/* Graphics Functions */

void initWaterFramebuffer() {
	reflection_tex = createTexture(width, height, GL_RGBA8, false, false, nullptr);
	reflection_depth_tex = createTexture(width, height, GL_DEPTH_COMPONENT32F, false, true, nullptr);
	glTextureParameteri(reflection_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(reflection_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glNamedFramebufferTexture(reflection_framebuffer, GL_COLOR_ATTACHMENT0, reflection_tex, 0);
	glNamedFramebufferTexture(reflection_framebuffer, GL_DEPTH_ATTACHMENT, reflection_depth_tex, 0);
	glBindTextureUnit(0, reflection_tex);
}

void genTangents(std::vector<Vertex>& vertices) {
	std::unordered_map<VertexKey, glm::vec3> accumulatedTangents;
	std::unordered_map<VertexKey, int> counts;

	for (size_t i = 0; i < vertices.size(); i += 3) {
		Vertex& v0 = vertices[i];
		Vertex& v1 = vertices[i + 1];
		Vertex& v2 = vertices[i + 2];

		glm::vec3 edge1 = v1.position - v0.position;
		glm::vec3 edge2 = v2.position - v0.position;
		glm::vec2 deltaUV1 = v1.uv - v0.uv;
		glm::vec2 deltaUV2 = v2.uv - v0.uv;

		float det = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
		glm::vec3 tangent(1.0f, 0.0f, 0.0f);

		if (det != 0.0f) {
			float invDet = 1.0f / det;
			tangent = invDet * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
		}

		VertexKey keys[3] = {
			{v0.position, v0.normal, v0.uv},
			{v1.position, v1.normal, v1.uv},
			{v2.position, v2.normal, v2.uv}
		};

		for (const auto& key : keys) {
			accumulatedTangents[key] += tangent;
			counts[key]++;
		}
	}

	for (auto& vertex : vertices) {
		VertexKey key = { vertex.position, vertex.normal, vertex.uv };
		if (counts[key] > 0) {
			vertex.tangent = glm::normalize(accumulatedTangents[key]);
		}
	}
}

/* Files */

std::string readFile(const char* path) {
	std::ifstream infile(path);
	return std::string(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>());
}

void window_size_callback(GLFWwindow* window, int new_width, int new_height) {
	width = new_width;
	height = new_height;
	initWaterFramebuffer();
}

/* Window */

// Create window
GLFWwindow* init() {
	// initialize GLFW and let it know the OpenGL version we intend to use
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 8);
	glfwSwapInterval(1);

	// create window using GLFW and set it as the active OpenGL context for the current thread
	GLFWwindow* window = glfwCreateWindow(width, height, "Cyber Seagull 4", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// load OpenGL function pointers from the graphics driver
	gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));

	// enable OpenGL debug messages
	// DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(glDebugOutput, nullptr);
	glfwSetWindowSizeCallback(window, window_size_callback);
	glfwSetKeyCallback(window, key_callback);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

	// make OpenGL normal-style (laugh out loud)
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// initialize ImGUI
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460 core");

	// make stb flip images
	stbi_set_flip_vertically_on_load(true);

	return window;
}

// Delete window
void cleanup(GLFWwindow* window) {
	glDeleteVertexArrays(1, &vao);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
}
