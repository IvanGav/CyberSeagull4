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

extern bool menuOpen;
extern std::vector<char> midi_keys_velocity;
extern std::vector<char> midi_control_velocity;

F32 mouse_sensitivity = 0.005;
F64 lastx; F64 lasty;

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

enum CamType {
    STATIC,
    FREECAM
};

/* Camera proper */

// Generic camera
struct Cam {
    glm::vec3 pos; // world position
    F32 theta; // rotation of camera in xz plane
    F32 y_theta; // angle of camera from xz plane to y axis
    CamType type;

    glm::vec3 lookDir() {
        float xmag = sin(theta) * cos(y_theta);
        float zmag = cos(theta) * cos(y_theta);
        float ymag = sin(y_theta);
        return glm::vec3(xmag, ymag, zmag);
    }
    void buttonPress(KeyboardAction action, F32 dt) {
        if (type == CamType::FREECAM) buttonPressFreeCam(action, dt);
    }
    void mouseMove(F32 dx, F32 dy) {
        if (type == CamType::FREECAM) mouseMoveFreeCam(dx, dy);
    }
private:
    void buttonPressFreeCam(KeyboardAction action, F32 dt) {
        int numRotations = 30; // basically speed of rotating
        if (action == MOVE_FORWARD) {
            pos.x += sin(theta) * dt;
            pos.z += cos(theta) * dt;
        }
        if (action == MOVE_BACKWARD) {
            pos.x -= sin(theta) * dt;
            pos.z -= cos(theta) * dt;
        }
        if (action == STRAFE_LEFT) {
            pos.x += sin(theta + PI / 2.0) * dt;
            pos.z += cos(theta + PI / 2.0) * dt;
        }
        if (action == STRAFE_RIGHT) {
            pos.x -= sin(theta + PI / 2.0) * dt;
            pos.z -= cos(theta + PI / 2.0) * dt;
        }
        if (action == TURN_RIGHT) {
            theta -= (PI * 2.0) / numRotations * dt;
        }
        if (action == TURN_LEFT) {
            theta += (PI * 2.0) / numRotations * dt;
        }
        if (action == TURN_DOWN) {
            y_theta -= (PI * 2.0) / numRotations * dt;
            y_theta = std::max<F32>(-PI / 2.0 + 0.01, y_theta);
        }
        if (action == TURN_UP) {
            y_theta += (PI * 2.0) / numRotations * dt;
            y_theta = std::min<F32>(PI / 2.0 - 0.01, y_theta);
        }
        if (action == MOVE_UP) {
            pos.y += 0.5 * dt;
        }
        if (action == MOVE_DOWN) {
            pos.y -= 0.5 * dt;
        }
    }
    void mouseMoveFreeCam(F32 dx, F32 dy) {
        theta -= dx * mouse_sensitivity;
        y_theta -= dy * mouse_sensitivity;
        y_theta = std::clamp<F32>(y_theta, -PI / 2.0 + epsilon, PI / 2.0 - epsilon);
    }
};

/* Other functions */

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

// allow free mouse movement
void windowMouseRelease(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // ENABLE MOUSE MOVEMENT
    glfwGetCursorPos(window, &lastx, &lasty);
}

// lock mouse (and listen to mouse movement)
void windowMouseFocus(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // DISABLE MOUSE MOVEMENT
    glfwGetCursorPos(window, &lastx, &lasty);
}

/* Move camera; call from main */

void moveCamGamepad(GLFWwindow* window, Cam& cam, double dt, GLFWgamepadstate state) {
    dt *= 5.0;
    if (state.buttons[0] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::MOVE_FORWARD, dt);
    if (state.buttons[1] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::MOVE_BACKWARD, dt);
    if (state.buttons[3] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::STRAFE_LEFT, dt);
    if (state.buttons[2] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::STRAFE_RIGHT, dt);

    if (state.buttons[13] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::TURN_UP, dt);
    if (state.buttons[11] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::TURN_DOWN, dt);
    if (state.buttons[14] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::TURN_LEFT, dt);
    if (state.buttons[12] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::TURN_RIGHT, dt);

    if (state.buttons[6] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::MOVE_DOWN, dt);
    if (state.buttons[7] == GLFW_PRESS)
        cam.buttonPress(KeyboardAction::MOVE_UP, dt);
}

void moveCamMidi(GLFWwindow* window, Cam& cam, double dt) {
    double look_multiplier = 10.f;
    double move_multiplier = 10.f;
    if (midi_keys_velocity[55]) {
        double ndt = (dt * midi_keys_velocity[55] / 128.f) * move_multiplier;
		cam.buttonPress(KeyboardAction::MOVE_FORWARD, ndt);
    }
    if (midi_keys_velocity[57]) {
        double ndt = (dt * midi_keys_velocity[57] / 128.f) * move_multiplier;
		cam.buttonPress(KeyboardAction::MOVE_BACKWARD, ndt);
    }
    if (midi_keys_velocity[59]) {
        double ndt = (dt * midi_keys_velocity[59] / 128.f) * move_multiplier;
		cam.buttonPress(KeyboardAction::STRAFE_LEFT, ndt);
    }
    if (midi_keys_velocity[60]) {
        double ndt = (dt * midi_keys_velocity[60] / 128.f) * move_multiplier;
		cam.buttonPress(KeyboardAction::STRAFE_RIGHT, ndt);
    }
    if (midi_control_velocity[74]) {
        double ndt = (dt * midi_control_velocity[74] / 128.f) * look_multiplier;
		cam.buttonPress(KeyboardAction::TURN_LEFT, ndt);
    }
    if (midi_control_velocity[71]) {
        double ndt = (dt * midi_control_velocity[71] / 128.f) * look_multiplier;
		cam.buttonPress(KeyboardAction::TURN_DOWN, ndt);
    }
    if (midi_control_velocity[5]) {
        double ndt = (dt * midi_control_velocity[5] / 128.f) * look_multiplier;
		cam.buttonPress(KeyboardAction::TURN_UP, ndt);
    }
    if (midi_control_velocity[84]) {
        double ndt = (dt * midi_control_velocity[84] / 128.f) * look_multiplier;
		cam.buttonPress(KeyboardAction::TURN_RIGHT, ndt);
    }

    if (midi_control_velocity[78]) {
        double ndt = (dt * midi_control_velocity[78] / 128.f) * move_multiplier;
		cam.buttonPress(KeyboardAction::MOVE_DOWN, ndt);
    }
    if (midi_control_velocity[76]) {
        double ndt = (dt * midi_control_velocity[76] / 128.f) * move_multiplier;
		cam.buttonPress(KeyboardAction::MOVE_UP, ndt);
    }
}

void moveCamKeyboard(GLFWwindow* window, Cam& cam, double dt) {
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
    F64 xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    cam.mouseMove(xpos - lastx, ypos - lasty);
    lastx = xpos; lasty = ypos;
}