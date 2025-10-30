#pragma once

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "util.h"

#define MAX_PARTICLES 1000000

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

struct ParticleSource {
	glm::vec3 pos;
};

Particle particles[MAX_PARTICLES];
U32 lastUsedParticle = 0;

U32 getUnusedParticlePos() {
	if (lastUsedParticle < MAX_PARTICLES) {
		lastUsedParticle++;
		return lastUsedParticle - 1;
	}
	for (U32 i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life <= 0.0f) return i;
	}
	return 0;
}

void advanceParticles(F32 dt) {
	for (U32 i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life > 0.0f) {
			particles[i].life -= dt;
			if (particles[i].life <= 0.0f) particles[i].pos = glm::vec3(INFINITY); // so that when doing std::sort, it'll be the last particle
			particles[i].pos += particles[i].speed * dt;
		}
	}
}

void sortParticles(const Cam& c, const glm::vec3 cam_forward) {
	for (U32 i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life > 0.0f) {
			particles[i].camdist = glm::dot(cam_forward, (particles[i].pos - c.pos));
		}
	}
	std::sort(particles, &particles[lastUsedParticle]);
	for (; lastUsedParticle >= 0 && particles[lastUsedParticle].life <= 0.0f; lastUsedParticle--);
}