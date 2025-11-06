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

#include "server.h"
#include "client.h"
#include "message.h"

#include "cam.h"
#include "debug.h"
#include "light.h"
#include "world_object.h"
#include "particle.h"
#include "game.h"
#include "music.h"
#include "input.h"



#include <random>
static std::mt19937 rng{ std::random_device{}() };

static std::uniform_real_distribution<float> randomPitch(0.02f, 1.15f);


std::vector<ma_sound*> liveSounds;

FreeCam cam = FreeCam{ Cam { glm::vec3(0.0f,1.0f,0.0f), 0.0, 0.0 } };


// F32 weezer[] = { 1.f, 1.05943508007, 1.f, 1.33482398807, 1.4982991247, 1.33482398807, 1.f, 0.89087642854, 0.79367809502, 1.f };

// Static data
static int width = 1920;
static int height = 1080;
static GLuint vao;
ma_engine engine;
double cur_time_sec;
std::vector<Entity> objects;
static struct {
	Mesh test_scene;
	Mesh cat;
} meshes;
static struct {
		GLuint green;
		GLuint cat;
		GLuint skybox;
		GLuint banner;
		GLuint weezer;
} textures;
servergull server(1951);
bool is_server = false;
U16 player_id = 0xffff;
seaclient client;

// forward declarations
GLFWwindow* init();
void cleanup(GLFWwindow* window);
std::string readFile(const char* path);
glm::mat4 baseTransform(const std::vector<Vertex>& vertices);
void genTangents(std::vector<Vertex>& vertices);
void cleanupFinishedSounds();
void playWithRandomPitch(ma_engine* engine, const char* filePath);
void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch = 1);
void throw_cats();
void throw_cat(int cat_num, bool owned, F64);

const F64 distancebetweenthetwoshipswhichshallherebyshootateachother = 100;
const glm::vec3 catstartingpos(10.0f, 0, 10.0f);
const F64 distbetweencats = -5;


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
	if (argc <= 1) {
		std::cout << "To host please pass -S\nTo connect to a host please pass ip\n";
		return 1;
	}
	GLFWwindow* window = init();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	initMouse(window); // function in cam.h
	glfwUpdateGamepadMappings("03000000ba1200004b07000000000000,Guitar Hero,platform:Windows,a:b0,b:b1,x:b2,y:b3,dpdown:+a1,dpup:-a1");
	initDefaultTexture();

	GLuint program = createShader("src/shader/triangle.vert", "src/shader/triangle.frag");
	GLuint shadowShader = createShader("src/shader/shadow.vert");
	GLuint cubeProgram = createShader("src/shader/cube.vert", "src/shader/cube.frag");
	GLuint particleProgram = createShader("src/shader/particle.vert", "src/shader/particle.frag");

	// Create textures (and frame buffers)

	GLuint framebuffer;
	glCreateFramebuffers(1, &framebuffer);
	GLuint shadowmap; int shadowmap_width = 2048; int shadowmap_height = 2048;
	{
		shadowmap = createTexture(shadowmap_width, shadowmap_height, GL_DEPTH_COMPONENT32F, false, nullptr);
		glTextureParameteri(shadowmap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(shadowmap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		F32 border[]{ 9999999.0F, 9999999.0F, 9999999.0F, 9999999.0F };
		glTextureParameterfv(shadowmap, GL_TEXTURE_BORDER_COLOR, border);

		glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, shadowmap, 0);
		glBindTextureUnit(1, shadowmap);
	}

	// Create geometry

	std::vector<Vertex> vertices;

	meshes.test_scene = Mesh::create(vertices, "asset/test_scene.obj");
	meshes.cat = Mesh::create(vertices, "asset/cat.obj");

	textures.green = createTextureFromImage("asset/green.jpg");
	textures.cat = createTextureFromImage("asset/cat.jpg");

	stbi_set_flip_vertically_on_load(false);
	textures.weezer = createTextureFromImage("asset/weezer.jfif");
	textures.banner = createTextureFromImage("asset/seagull_banner.png");
	stbi_set_flip_vertically_on_load(true);


	objects.push_back(Entity::create(&meshes.test_scene, textures.green));
	objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), NONEMITTER));

	for (int i = 0; i < 6; i++) {
		objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(catstartingpos.x + (distbetweencats * i), catstartingpos.y, catstartingpos.z)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), CANNON, true));
		objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(catstartingpos.x + (distbetweencats * i), catstartingpos.y, catstartingpos.z + distancebetweenthetwoshipswhichshallherebyshootateachother)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), (float)PI, glm::vec3(0.0f, 0.0f, 1.0f)  ), glm::vec3(0.1f, 0.1f, 0.1f)), CANNON));
	}

	std::string server_ip = argv[1];
	if (argv[1][0] == '-' && argv[1][1] == 'S') {
		server.Start();
		server_ip = "127.0.0.1";
		is_server = true;
	}

	client.Connect(server_ip, 1951);

	objects.push_back(Entity::create(&meshes.cat, textures.cat, glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(-15.0f, 0.0f, 10.0f)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), CANNON));


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

  ParticleSource particleSource{ glm::vec3(0.0f), glm::vec3(0.01f), RGBA8 { 255,255,255,255 }, 0.1f, 5.0f }; // live for 5 seconds

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
	sun.illuminateArea(10.0);
	glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

	double last_time_sec = 0.0;
	int songstart;

	ma_engine_init(NULL, &engine);
	ma_engine_set_volume(&engine, 0.1f);
	playSound(&engine, "asset/seagull-flock-sound-effect-206610.wav", MA_TRUE);

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
		windowFocusControl(window);

		throw_cats();

		GLFWgamepadstate state{};
		if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
			moveFreeCamGamepad(window, cam, dt, state);
		}
		else {
			moveFreeCam(window, cam, dt);
		}

		for (int i = 0; i < objects.size(); i++) {
			if (objects[i].update) {
				if (!objects[i].update(objects[i], cur_time_sec)) {
					objects.erase(objects.begin() + i);
					i--;
				}
			}
		}

		cleanupFinishedSounds();


		if (is_server) {
			server.Update();
		}
		client.check_messages();

		// Update particles

		advanceParticles(dt);
		particleSource.spawnParticle();
		sortParticles(cam.cam, cam.cam.lookDir());
		packParticles();

		glNamedBufferSubData(pvertex_buffer, 0, sizeof(ParticleVertex) * lastUsedParticle * VERTICES_PER_PARTICLE, pvertex_vertex);
		glNamedBufferSubData(pdata_buffer, 0, sizeof(ParticleData) * lastUsedParticle, pvertex_data);
    
		// get cam matrices

		glm::mat4 view = glm::lookAt(cam.cam.pos, cam.cam.pos + cam.cam.lookDir(), cam_up);
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float) (width) / height, 0.1f, 1000.0f);

		// Update the light direction
		sun.setLightDirVec3(lightDir);

		// Draw to framebuffer (shadow map)
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

		for (int i = 0; i < objects.size(); i++) {
			Entity& o = objects[i];
			glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
			glBindTextureUnit(0, objects[i].tex);

			glProgramUniformMatrix4fv(program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
			glProgramUniformMatrix3fv(program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

			glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
		}

        // Draw particles
        glUseProgram(particleProgram);

        glProgramUniformMatrix4fv(particleProgram, 0, 1, GL_FALSE, glm::value_ptr(view));
        glProgramUniformMatrix4fv(particleProgram, 4, 1, GL_FALSE, glm::value_ptr(projection));
        glBindTextureUnit(0, textures.green);

        glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_PARTICLE * lastUsedParticle); // where lastUsedParticle is the number of particles


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


		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// tell the OS to display the frame
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

void throw_cats() {
	F64 timestep = cur_time_sec + 1;
	std::vector<U8> message = {(U8)(player_id & 0xff), (U8)((player_id >> 8) && 0xff)};
	for (int i = 0; i < sizeof(timestep); i++) {
		message.push_back(((*(U64*)&timestep) >> (8 * i)) & 0xff);
	}
	bool send = false;
	for (int i = 0; i < numcats; i++) {
		if (cats_thrown[i]) {
			throw_cat(i, true);
			message.push_back(i);
			cats_thrown[i] = false;
			send = true;
		}
	}
	if (send) {
		client.send_message(PLAYER_CAT_FIRE, message);
	}
}

void throw_cat(int cat_num, bool owned, F64 start_time) {
	playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[cat_num]);
	if (start_time == 0) {
		start_time = cur_time_sec;
	}
	for (int i = 0; i < objects.size(); i++) {
		if (objects[i].type == CANNON && cat_num == objects[i].cat_id && objects[i].owned == owned) {
			objects.push_back(Entity::create(&meshes.cat, textures.cat, objects[i].model, PROECTILE
			));
			objects.back().start_time = cur_time_sec;
			objects.back().pretransmodel = objects.back().model;
			objects.back().shoot_angle = owned ? 0.0f : PI;
			objects.back().update = [](Entity& cat, F64 curtime) {
				cat.model = toModel((curtime - cat.start_time) * 50, 0, distancebetweenthetwoshipswhichshallherebyshootateachother, cat.shoot_angle) * cat.pretransmodel;
				return (cat.model[3][1] >= 0.0f);
				};

		}
	}
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

void playWithRandomPitch(ma_engine* engine, const char *filePath) {
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
		ma_sound_set_pitch(s, pitch);
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
