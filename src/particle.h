#pragma once

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
U32 lastUsedParticle = -1; // Inclusive; `lastUsedParticle == 2` means that there are 3 particles that may be alive: particles[0], [1] and [2]

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
		return lastUsedParticle;
	}
	for (U32 i = 0; i <= lastUsedParticle; i++) {
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
		}
	}
}

// Call after adding new particles, before drawing them
void sortParticles(const Cam& c, const glm::vec3 cam_forward) {
	for (U32 i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life > 0.0f) {
			particles[i].camdist = glm::dot(cam_forward, (particles[i].pos - c.pos));
		}
	}
	std::sort(particles, &particles[lastUsedParticle]);
	for (; lastUsedParticle >= 0 && particles[lastUsedParticle].life <= 0.0f; lastUsedParticle--);
}

void packParticles() {

}