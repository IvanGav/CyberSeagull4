#pragma once

#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <limits>
#include "util.h"

// Get a random float in range (0,1)
F32 randf01() {
    return static_cast<F32>(rand()) / static_cast<F32>(RAND_MAX);
}