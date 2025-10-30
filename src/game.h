#pragma once

#include <vector>
#include <queue>
#include <algorithm>
#include "util.h"

#define BASE_COOLDOWN 2.0 // seconds

struct Lane {
	std::vector<F64> dists;
};

struct Player {
	std::vector<F64> cooldowns;
	std::vector<Lane> projectiles;

	// Advance the time by `dt`
	void advanceTime(F64 dt) {
		for (U32 i = 0; i < cooldowns.size(); i++) {
			cooldowns[i] = std::max(0.0, cooldowns[i] - dt);
			std::vector<F64>& balls = projectiles[i].dists;
			for (U32 j = 0; j < balls.size(); j++) {
				balls[j] += dt;
			}
		}
	}
	// Shoot a ball in the specified lane. If it was still on cooldown, return the cooldown (0.0 or less otherwise)
	F64 shoot(U32 lane) {
		if (cooldowns[lane] > 0.0) return cooldowns[lane];
		cooldowns[lane] = BASE_COOLDOWN;
		projectiles[lane].dists.push_back(0.0);
		return -1.0;
	}

	// When receiving the enemy shot packet, use this
	void forceShoot(U32 lane) {
		projectiles[lane].dists.push_back(0.0);
	}
};

struct Game {
	Player players[2]; // players[0] is you; players[1] is the enemy
	U32 lanes;
	F64 playerDist;
};

// Given a lane and dist, return the model matrix.
glm::mat4 toModel(F64 dist, U32 lane) {
	// TODO
}

// Given a matrix returned by `toModel` and distance between players, return a model matrix from enemy's perspective
glm::mat4 friendToEnemyModel(glm::mat4 friendModel, F64 playersDist) {
	// TODO
}