#pragma once

#include <vector>
#include <queue>
#include <algorithm>
#include "util.h"

// Given a lane and dist, return the model matrix.
glm::mat4 positionOnArc(F64 timeFromLaunch, U32 lane, F64 distBetweenEnemyShips, F32 angle, F32 height = 0.45) {
	return glm::translate(glm::vec3(sin(angle) * timeFromLaunch, (-height / distBetweenEnemyShips * (timeFromLaunch) * (timeFromLaunch - distBetweenEnemyShips)), cos(angle) * timeFromLaunch));
}
