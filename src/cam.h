#pragma once

#include <cmath>
#include <algorithm>

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

static constexpr auto PI = 3.14159265359;
static constexpr glm::vec3 cam_up = glm::vec3(0.0f, 1.0f, 0.0f);
static constexpr auto epsilon = 0.00001;

double mouse_sensitivity = 0.005;
double lastx; double lasty;

enum KeyboardAction {
    MOVE_FORWARD,
    MOVE_BACKWARD,
    STRAFE_RIGHT,
    STRAFE_LEFT,
    TURN_RIGHT,
    TURN_LEFT,
    TURN_UP,
    TURN_DOWN,
    MOVE_UP,
    MOVE_DOWN
};

// Generic camera
struct Cam {
    glm::vec3 pos; // world position
    double theta; // rotation of camera in xz plane
    double y_theta; // angle of camera from xz plane to y axis

    glm::vec3 lookDir() {
        float xmag = sin(theta) * cos(y_theta);
        float zmag = cos(theta) * cos(y_theta);
        float ymag = sin(y_theta);
        return glm::vec3(xmag, ymag, zmag);
    }
};

// First person free camera controls
struct FreeCam {
    Cam cam;
    void buttonPress(KeyboardAction action, double dt) {
        int numRotations = 30; // basically speed of rotating
        if (action == MOVE_FORWARD) {
            cam.pos.x += sin(cam.theta) * dt;
            cam.pos.z += cos(cam.theta) * dt;
        }
        if (action == MOVE_BACKWARD) {
            cam.pos.x -= sin(cam.theta) * dt;
            cam.pos.z -= cos(cam.theta) * dt;
        }
        if (action == STRAFE_LEFT) {
            cam.pos.x += sin(cam.theta + PI / 2.0) * dt;
            cam.pos.z += cos(cam.theta + PI / 2.0) * dt;
        }
        if (action == STRAFE_RIGHT) {
            cam.pos.x -= sin(cam.theta + PI / 2.0) * dt;
            cam.pos.z -= cos(cam.theta + PI / 2.0) * dt;
        }
        if (action == TURN_RIGHT) {
            cam.theta -= (PI * 2.0) / numRotations * dt;
        }
        if (action == TURN_LEFT) {
            cam.theta += (PI * 2.0) / numRotations * dt;
        }
        if (action == TURN_DOWN) {
            cam.y_theta -= (PI * 2.0) / numRotations * dt;
            cam.y_theta = std::max(-PI / 2.0 + 0.01, cam.y_theta);
        }
        if (action == TURN_UP) {
            cam.y_theta += (PI * 2.0) / numRotations * dt;
            cam.y_theta = std::min(PI / 2.0 - 0.01, cam.y_theta);
        }
        if (action == MOVE_UP) {
            cam.pos.y += 0.5 * dt;
        }
        if (action == MOVE_DOWN) {
            cam.pos.y -= 0.5 * dt;
        }
    }
    void mouseMove(double dx, double dy) {
        cam.theta -= dx * mouse_sensitivity;
        cam.y_theta -= dy * mouse_sensitivity;
        cam.y_theta = std::clamp(cam.y_theta, -PI / 2.0 + epsilon, PI / 2.0 - epsilon);
    }
};

// RTS camera (SC2, BAR, etc) controls
struct RTSCam {
    Cam cam;
    glm::vec3 anchor; // rotation origin
    double anchor_dist; // distance from origin

    // TODO: Unimplemented and possibly unnecessary; just here to demonstrate why I have a "generic" `Cam`
};

// Disable mouse and make sure that the initial mouse position is correct
void initMouse(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // DISABLE MOUSE MOVEMENT
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    } else {
        printf("Cannot enable GLFW_RAW_MOUSE_MOTION, aborting");
        exit(1);
    }
    glfwGetCursorPos(window, &lastx, &lasty);
}

// When ESCAPE pressed, give mouse control. When mouse clicked, disable mouse again.
void windowFocusControl(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // ENABLE MOUSE MOVEMENT
        glfwGetCursorPos(window, &lastx, &lasty);
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // DISABLE MOUSE MOVEMENT
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        glfwGetCursorPos(window, &lastx, &lasty);
    }
}

void moveFreeCam(GLFWwindow* window, FreeCam& cam, double dt) {
    dt *= 5.0;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::MOVE_FORWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::MOVE_BACKWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::STRAFE_LEFT, dt);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::STRAFE_RIGHT, dt);

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::TURN_UP, dt);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::TURN_DOWN, dt);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::TURN_LEFT, dt);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::TURN_RIGHT, dt);

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::MOVE_DOWN, dt);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::MOVE_UP, dt);

    // mouse input
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    cam.mouseMove(xpos - lastx, ypos - lasty);
    lastx = xpos; lasty = ypos;
}