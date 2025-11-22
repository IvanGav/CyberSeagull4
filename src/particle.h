#pragma once

/*
	README usage guide

	advanceParticles(dt);
	particleSource.spawnParticle();
	sortParticles(cam, cam.lookDir());
	packParticles();
*/

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "util.h"
#include "rng.h"
#include "cam.h"

#define MAX_PARTICLES 1000000
#define VERTICES_PER_PARTICLE 6

// For gpu

struct ParticleVertex {
	alignas(4) U32 particleid;
	alignas(4) U32 vertexid;
};

struct ParticleData {
	alignas(16) glm::vec3 pos;
	alignas(16) glm::vec4 color;
	alignas(16) glm::vec3 dir;
	alignas(4) F32 size;
	F32 age;
	U32 sheetRes;
	F32 rotation;
	U32 tex; // 0 to 3
};

GLuint particle_textures[4]{};

// For cpu

U32 getUnusedParticlePos();

struct Particle {
	glm::vec3 pos, speed;
	RGBA8 color;
	F32 size;
	F32 life;
	F32 maxLife;
	U8 tex; // 0 to 3
	U8 sheetResX;
	U8 sheetResY;
	F32 scaleOverTime;
	glm::vec3 axis;
	F32 rotation;
	F32 rotationOverTime;
	F32 gravity;
	F32 drag;

	F32 camDist;

	bool operator<(Particle& other) {
		// Sort in reverse order : far particles drawn first.
		return this->camDist > other.camDist;
	}
};

// Global particles array; all existing particles are stored here
Particle particles[MAX_PARTICLES];
U32 lastUsedParticle = 0; // Exclusive; `lastUsedParticle == 2` means that there are 2 particles that may be alive: particles[0] and [1]

ParticleData pVertexData[MAX_PARTICLES];
ParticleVertex pVertexVertex[MAX_PARTICLES * VERTICES_PER_PARTICLE]; // WHAT DOES THIS MEAN!!!?

struct ParticleSource {
	glm::vec3 pos;

	glm::vec3 initSpeed;
	RGBA8 initColor;
	F32 initSize;
	F32 initLife;
	U8 texIndex = 0xff; // "out of bounds"

	U8 sheetResX = 1;
	U8 sheetResY = 1;

	F32 scaleOverTime;
	F32 randomRotation;
	F32 randomRotationOverTime;
	F32 gravity;
	F32 drag;
	F32 initVelScale;

	void spawnParticle() {
		glm::vec3 off = glm::normalize(glm::vec3(randf01() * 2.0f - 1.0f, randf01() * 2.0f - 1.0f, randf01() * 2.0f - 1.0f)) * randf01();
		Particle p{ pos, initSpeed + off * 4.0F * initVelScale, initColor, initSize, initLife, initLife, texIndex, sheetResX, sheetResY, scaleOverTime };
		p.rotation = randf01() * randomRotation;
		p.rotationOverTime = (randf01() * 2.0F - 1.0F) * randomRotationOverTime;
		p.gravity = gravity;
		p.drag = drag;
		particles[getUnusedParticlePos()] = p;
	}
	void spawnParticles(int num) {
		for (int i = 0; i < num; i++)
			spawnParticle();
	}
	void setSheetRes(U8 x, U8 y) {
		sheetResX = x;
		sheetResY = y;
	}
};

void addParticle(const Particle& p) {
	particles[getUnusedParticlePos()] = p;
}

// Assume that requested index will now be used
// Optimization possible
U32 getUnusedParticlePos() {
	if (lastUsedParticle <= MAX_PARTICLES) {
		lastUsedParticle++;
		return lastUsedParticle - 1;
	}
	for (U32 i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life <= 0.0f) return i;
	}
	return 0;
}

// Call once per frame to move particles
void advanceParticles(F32 dt) {
	for (U32 i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life > 0.0f) {
			particles[i].life -= dt;
			if (particles[i].life <= 0.0f) particles[i].pos = glm::vec3(INFINITY); // so that when doing std::sort, it'll be the last particle
			particles[i].pos += particles[i].speed * dt;
			particles[i].speed.y -= particles[i].gravity * dt;
			particles[i].speed *= powf(1.0F - particles[i].drag, dt);
			particles[i].rotationOverTime *= powf(1.0F - particles[i].drag * 0.25F, dt);
			particles[i].size += dt * particles[i].scaleOverTime;
			particles[i].rotation += dt * particles[i].rotationOverTime;
		}
	}
}

// Call after adding new particles, before drawing them
// After calling this, `lastUsedParticle + 1` is the total number of particles
void sortParticles(Cam& c) {
	for (U32 i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life > 0.0f) {
			particles[i].camDist = glm::dot(c.lookDir(), (particles[i].pos - c.pos));
		}
	}
	std::sort(particles, &particles[lastUsedParticle]);
	for (; lastUsedParticle > 0 && particles[lastUsedParticle-1].life <= 0.0f; lastUsedParticle--);
}

// Call `sortParticles` before this
// Updates the global `pvertex_data` and `pvertex_vertex` arrays
void packParticles() {
	for (U32 i = 0; i < lastUsedParticle; i++) {
		// pvertex_data, pvertex_vertex
		for (U32 j = 0; j < VERTICES_PER_PARTICLE; j++) {
			pVertexVertex[i * VERTICES_PER_PARTICLE + j].particleid = i;
			pVertexVertex[i * VERTICES_PER_PARTICLE + j].vertexid = j;
		}
		pVertexData[i].pos = particles[i].pos;
		pVertexData[i].color = particles[i].color.to_v4f32();
		pVertexData[i].dir = particles[i].axis;
		pVertexData[i].size = particles[i].size;
		pVertexData[i].age = (particles[i].maxLife - particles[i].life) / particles[i].maxLife;
		pVertexData[i].sheetRes = particles[i].sheetResY << 8 | particles[i].sheetResX;
		pVertexData[i].rotation = particles[i].rotation;
		pVertexData[i].tex = particles[i].tex;
	}
}