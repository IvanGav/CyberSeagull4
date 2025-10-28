#pragma once

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Get a vec3 direction defined by looking from origin to a direction (azimuth,altitude)
glm::vec3 getAngle(double azimuth, double altitude) {
    float xmag = sin(azimuth) * cos(altitude);
    float zmag = cos(azimuth) * cos(altitude);
    float ymag = sin(altitude);
    return glm::vec3(xmag, ymag, zmag);
}

// To use:
// - Set the area to be illuminated (`illuminateArea`), which will set the `projection` matrix.
// - Set the light direction (`setLightDir` or `setLightDirVec3`), which will set the angle at which the light shines
struct DirectionalLight {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 combined;

    // Set an area to be illuminated
    void illuminateArea(float unit) {
        projection = glm::ortho(-unit, unit, -unit, unit, -unit, unit);
        combined = projection * view;
    }

    // Set a light angle
    void setLightDirVec3(glm::vec3 angle) {
        view = glm::lookAt(glm::vec3(0.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));
        combined = projection * view;
    }

    // Set a light angle based on azimuth and altitude. Angle in radians.
    void setLightDir(double azimuth, double altitude) {
        view = glm::lookAt(glm::vec3(0.0f), getAngle(azimuth, altitude), glm::vec3(0.0f, 1.0f, 0.0f));
        combined = projection * view;
    }
};