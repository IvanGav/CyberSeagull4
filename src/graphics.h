# pragma once

#include <vector>
#include <string>

// GLAD: OpenGL function loader
#include <glad/glad.h>

// GLFW: cross-platform windowing and input
#include <GLFW/glfw3.h>

// STB Image: load image files such as .png and .jpg
#include <stb/stb_image.h>

#include "util.h"
#include "debug.h"
#include "world_object.h"
#include "particle.h"
#include "light.h"
#include "cam.h"

/* Global data */

int shadowmap_width = 4096;
int shadowmap_height = 4096;

// Global array of all entities in the world
std::vector<Entity> objects;

// Global array of vertex data (meshes)
std::vector<Vertex> vertices;

// List of meshes (indexing into `vertices`)
static struct {
	Mesh test_scene;
	Mesh cat;
	Mesh quad;
	Mesh seagWalk2;
	Mesh seagWalk3;
	Mesh cannon;
	Mesh cannon_door;
	Mesh seagBall;
	Mesh ship;
	Mesh shipNoMast;
} meshes;

// List of all textures
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
		GLuint color;
		GLuint norm;
		GLuint arm;
	} ship;
	struct {
		GLuint menu_logo;
		GLuint context;
		GLuint page;
		GLuint connect;
		GLuint leave;
		GLuint closeMenu;
		GLuint P1Ready;
		GLuint P1NotReady;
		GLuint P2Ready;
		GLuint P2NotReady;
		GLuint startGame;
		GLuint waitForPlayer;
	} menu;
	GLuint feather;
	GLuint cannonExplosion;
} textures;

struct {
	GLuint reflection_framebuffer;
	GLuint shadowmap;
} framebuffers;

struct {
	GLuint shadowmap;
	GLuint reflection_tex;
	GLuint reflection_depth_tex;
} dyn_textures;

struct {
	GLuint vertices;
	GLuint particle_vertices;
	GLuint particle_data;
} buffers;

struct {
	GLuint program;
	GLuint shadow;
	GLuint skybox;
	GLuint particle;
	GLuint water;
} programs;

/* Other graphics functions */

void initWaterFramebuffer(int width, int height) {
	dyn_textures.reflection_tex = createTexture(width, height, GL_RGBA8, false, false, nullptr);
	dyn_textures.reflection_depth_tex = createTexture(width, height, GL_DEPTH_COMPONENT32F, false, true, nullptr);
	glTextureParameteri(dyn_textures.reflection_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(dyn_textures.reflection_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glNamedFramebufferTexture(framebuffers.reflection_framebuffer, GL_COLOR_ATTACHMENT0, dyn_textures.reflection_tex, 0);
	glNamedFramebufferTexture(framebuffers.reflection_framebuffer, GL_DEPTH_ATTACHMENT, dyn_textures.reflection_depth_tex, 0);
	glBindTextureUnit(0, dyn_textures.reflection_tex);
}

/* Initialization functions */

void init_meshes() {
	meshes.test_scene = Mesh::create(vertices, "asset/test_scene.obj");
	meshes.cat = Mesh::create(vertices, "asset/cat.obj");
	meshes.quad = Mesh::xzQuad(vertices);
	meshes.seagWalk2 = Mesh::create(vertices, "asset/seagull/seagull_walk2.obj");
	meshes.seagWalk3 = Mesh::create(vertices, "asset/seagull/seagull_walk3.obj");
	meshes.cannon = Mesh::create(vertices, "asset/cannon/cannon.obj");
	meshes.cannon_door = Mesh::create(vertices, "asset/cannon/cannon_door.obj");
	meshes.seagBall = Mesh::create(vertices, "asset/seagull/seagull.obj");
	meshes.ship = Mesh::create(vertices, "asset/ship/ship.obj");
	meshes.shipNoMast = Mesh::create(vertices, "asset/ship/ship_no_mast.obj");

	genTangents();
}

void init_textures() {
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
	textures.menu.P1Ready = createTextureFromImage("asset/P1Ready.png");
	textures.menu.P1NotReady = createTextureFromImage("asset/P1NotReady.png");
	textures.menu.P2Ready = createTextureFromImage("asset/P2Ready.png");
	textures.menu.P2NotReady = createTextureFromImage("asset/P2NotReady.png");
	textures.menu.startGame = createTextureFromImage("asset/startGame.png");
	textures.menu.waitForPlayer = createTextureFromImage("asset/waitForPlayer.png");

	textures.weezer = createTextureFromImage("asset/weezer.jfif");
	textures.banner = createTextureFromImage("asset/seagull_banner.png");
	stbi_set_flip_vertically_on_load(true);

	textures.cannon.color = createTextureFromImage("asset/cannon/cannon_BaseColor.jpg");
	textures.cannon.norm = createTextureFromImage("asset/cannon/cannon_Normal.jpg");
	textures.cannon.arm = createTextureFromImage("asset/cannon/cannon_ARM.jpg");

	textures.ship.color = createTextureFromImage("asset/ship/ship_BaseColor.png");
	textures.ship.norm = createTextureFromImage("asset/ship/ship_Normal.png");
	textures.ship.arm = createTextureFromImage("asset/ship/ship_ARM.png");

	textures.feather = createTextureFromImage("asset/feather.png");

	textures.cannonExplosion = createTextureFromImage("asset/cannon_explosion.png");

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
}

void init_framebuffers(int width, int height) {
	glCreateFramebuffers(1, &framebuffers.shadowmap);
	{
		dyn_textures.shadowmap = createTexture(shadowmap_width, shadowmap_height, GL_DEPTH_COMPONENT32F, false, true, nullptr);
		glTextureParameteri(dyn_textures.shadowmap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(dyn_textures.shadowmap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		F32 border[]{ 9999999.0F, 9999999.0F, 9999999.0F, 9999999.0F };
		glTextureParameterfv(dyn_textures.shadowmap, GL_TEXTURE_BORDER_COLOR, border);

		glNamedFramebufferTexture(framebuffers.shadowmap, GL_DEPTH_ATTACHMENT, dyn_textures.shadowmap, 0);
		glBindTextureUnit(1, dyn_textures.shadowmap);
	}

	// create buffer objects for water reflection and refraction; later on they will be rendered to and combined to create the water texture
	glCreateFramebuffers(1, &framebuffers.reflection_framebuffer);
	initWaterFramebuffer(width, height);
}

// Create buffers
void init_buffers() {
	glCreateBuffers(1, &buffers.vertices);
	glNamedBufferStorage(buffers.vertices, vertices.size() * sizeof(Vertex), vertices.data(), 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.vertices);

	glCreateBuffers(1, &buffers.particle_vertices);
	glNamedBufferData(buffers.particle_vertices, sizeof(pvertex_vertex), pvertex_vertex, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers.particle_vertices);

	glCreateBuffers(1, &buffers.particle_data);
	glNamedBufferData(buffers.particle_data, sizeof(pvertex_data), pvertex_data, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffers.particle_data);
}

void init_programs() {
	programs.program = createShader("src/shader/triangle.vert", "src/shader/triangle.frag");
	programs.shadow = createShader("src/shader/shadow.vert");
	programs.skybox = createShader("src/shader/cube.vert", "src/shader/cube.frag");
	programs.particle = createShader("src/shader/particle.vert", "src/shader/particle.frag");
	programs.water = createShader("src/shader/water.vert", "src/shader/water.frag");
}

// CALL THIS FUNCTION FROM MAIN ONCE, NO OTHER GRAPHICS INIT FUNCTIONS
void init_graphics(int width, int height) {
	init_programs();
	init_meshes();
	init_textures();
	init_framebuffers(width, height);
	init_buffers();
}

/* Game loop draw functions */

void draw_skybox(glm::mat4& proj, glm::mat4& view) {
	// Draw the skybox
	glUseProgram(programs.skybox);

	glBindTextureUnit(2, textures.skybox);

	glProgramUniformMatrix4fv(programs.skybox, 0, 1, GL_FALSE, glm::value_ptr(proj * glm::mat4(glm::mat3(view))));

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glDrawArrays(GL_TRIANGLES, 0, 36); // number of vertices in a cube; aka a magic number

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

void draw_objects(const glm::mat4& proj, const glm::mat4& view, const DirectionalLight& sun, const Cam& cam) {
	// Draw scene
	glUseProgram(programs.program);

	glProgramUniformMatrix4fv(programs.program, 4, 1, GL_FALSE, glm::value_ptr(proj * view));
	glProgramUniformMatrix4fv(programs.program, 11, 1, GL_FALSE, glm::value_ptr(sun.combined));
	glProgramUniform3fv(programs.program, 15, 1, glm::value_ptr(cam.pos));
	glProgramUniform3fv(programs.program, 16, 1, glm::value_ptr(sun.dir));
	glProgramUniform3fv(programs.program, 17, 1, glm::value_ptr(glm::vec3(1.0)));
	glProgramUniform2f(programs.program, 18, (F32)shadowmap_height, (F32)shadowmap_width);
	glProgramUniform1i(programs.program, 19, false);
	glBindTextureUnit(1, dyn_textures.shadowmap);

	for (int i = 0; i < objects.size(); i++) {
		Entity& o = objects[i];
		glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
		glBindTextureUnit(0, objects[i].tex);
		glBindTextureUnit(2, objects[i].normal);

		glProgramUniformMatrix4fv(programs.program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
		glProgramUniformMatrix3fv(programs.program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

		glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
	}
}

void draw_particles() {
}

void draw_water(const glm::mat4& proj, const glm::mat4& view, const DirectionalLight& sun, const Cam& cam, F64 time, Entity& water) {
	// Draw water
	glUseProgram(programs.water);

	glProgramUniformMatrix4fv(programs.water, 4, 1, GL_FALSE, glm::value_ptr(proj * view));
	glProgramUniformMatrix4fv(programs.water, 11, 1, GL_FALSE, glm::value_ptr(sun.combined));
	glProgramUniform3fv(programs.water, 15, 1, glm::value_ptr(cam.pos));
	glProgramUniform3fv(programs.water, 16, 1, glm::value_ptr(sun.dir));
	glProgramUniform3fv(programs.water, 17, 1, glm::value_ptr(glm::vec3(1.0)));
	glProgramUniform2f(programs.water, 18, (F32)shadowmap_height, (F32)shadowmap_width);
	glProgramUniform1f(programs.water, 19, time);

	glBindTextureUnit(2, textures.waterNormal);
	glBindTextureUnit(3, textures.waterOffset);

	{
		glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(water.model)));
		glBindTextureUnit(0, dyn_textures.reflection_tex);

		glProgramUniformMatrix4fv(programs.water, 0, 1, GL_FALSE, glm::value_ptr(water.model));
		glProgramUniformMatrix3fv(programs.water, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

		glDrawArrays(GL_TRIANGLES, water.mesh->offset, water.mesh->size);
	}
}

void draw_shadows(glm::mat4 const& combined) {
	// Draw to shadow map framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffers.shadowmap);
	glViewport(0, 0, shadowmap_width, shadowmap_height);
	glClearDepth(9999999.0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glClearDepth(1.0);

	glUseProgram(programs.shadow);

	glProgramUniformMatrix4fv(programs.shadow, 4, 1, GL_FALSE, glm::value_ptr(combined));
	for (int i = 0; i < objects.size(); i++) {
		Entity& o = objects[i];

		glProgramUniformMatrix4fv(programs.shadow, 0, 1, GL_FALSE, glm::value_ptr(o.model));

		glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
	}
}

void draw_shadows(glm::mat4& proj, glm::mat4& view) {
	draw_shadows(proj * view);
}

/* Util graphics functions */

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

void genTangents() {
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

void send_particle_data_to_gpu() {
	glNamedBufferSubData(buffers.particle_vertices, 0, sizeof(ParticleVertex) * lastUsedParticle * VERTICES_PER_PARTICLE, pvertex_vertex);
	glNamedBufferSubData(buffers.particle_data, 0, sizeof(ParticleData) * lastUsedParticle, pvertex_data);
}