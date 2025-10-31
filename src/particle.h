#pragma once

/*
	README usage guide

	advanceParticles(dt);
	particleSource.spawnParticle();
	sortParticles(cam, cam.lookDir());
	packParticles();
*/

#include "util.h"
#include "rng.h"

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "util.h"

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
};

// For cpu

U32 getUnusedParticlePos();

struct Particle {
	glm::vec3 pos, speed;
	RGBA8 color;
	F32 life;

	F32 camdist;

	bool operator<(Particle& other) {
		// Sort in reverse order : far particles drawn first.
		return this->camdist > other.camdist;
	}
};

// Global particles array; all existing particles are stored here
Particle particles[MAX_PARTICLES];
U32 lastUsedParticle = 0; // Exclusive; `lastUsedParticle == 2` means that there are 2 particles that may be alive: particles[0] and [1]

ParticleData pvertex_data[MAX_PARTICLES];
ParticleVertex pvertex_vertex[MAX_PARTICLES * VERTICES_PER_PARTICLE];

struct ParticleSource {
	glm::vec3 pos;

	glm::vec3 init_speed;
	RGBA8 init_color;
	F32 init_life;

	void spawnParticle() {
		// TODO add random
		glm::vec3 off = glm::vec3(randf01() * 2.0f - 1.0f, randf01() * 2.0f - 1.0f, randf01() * 2.0f - 1.0f);
		particles[getUnusedParticlePos()] = Particle{ pos, init_speed + off, init_color, init_life};
	}
};

// Assume that requested index will now be used
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
			if (particles[i].life <= 0.0f) particles[i].pos = glm::vec3(-INFINITY); // so that when doing std::sort, it'll be the last particle
			particles[i].pos += particles[i].speed * dt;
		}
	}
}

// Call after adding new particles, before drawing them
// After calling this, `lastUsedParticle + 1` is the total number of particles
void sortParticles(const Cam& c, const glm::vec3 cam_forward) {
	for (U32 i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life > 0.0f) {
			particles[i].camdist = glm::dot(cam_forward, (particles[i].pos - c.pos));
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
			pvertex_vertex[i * VERTICES_PER_PARTICLE + j].particleid = i;
			pvertex_vertex[i * VERTICES_PER_PARTICLE + j].vertexid = j;
		}
		pvertex_data[i].pos = particles[i].pos;
		pvertex_data[i].color = particles[i].color.to_v4f32();
	}
}