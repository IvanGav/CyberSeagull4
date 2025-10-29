// STL
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include <cmath>

// GLAD: OpenGL function loader
#include <glad/glad.h>

// GLFW: cross-platform windowing and input
#include <GLFW/glfw3.h>

// STB Image: load image files such as .png and .jpg
#include <stb/stb_image.h>

// tiny_obj_loader: load 3D models in the .obj format
#include <tinyobjloader/tiny_obj_loader.h>

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ImGUI: gui library
#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

// This project
#include "util.h"
#include "cam.h"
#include "debug.h"
#include "light.h"
#include "world_object.h"

// Static data
static constexpr int width = 1920;
static constexpr int height = 1080;
static GLuint vao;

// forward declarations
GLFWwindow* init();
void cleanup(GLFWwindow* window);
std::string readFile(const char* path);
glm::mat4 baseTransform(const std::vector<Vertex>& vertices);
void genTangents(std::vector<Vertex>& vertices);

// Create a shader from vertex and fragment shader files
GLuint createShader(const char* vsPath, const char* fsPath = nullptr) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = 0;

    std::string vsSource = readFile(vsPath);
    const char* vsSource_cstr = vsSource.c_str();

    glShaderSource(vs, 1, &vsSource_cstr, nullptr);
    glCompileShader(vs);
    getCompileStatus(vs);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);

    if (fsPath) {
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

        std::string fsSource = readFile(fsPath);
        const char* fsSource_cstr = fsSource.c_str();

        glShaderSource(fs, 1, &fsSource_cstr, nullptr);

        glCompileShader(fs);
        getCompileStatus(fs);

        glAttachShader(program, fs);
    }

    glLinkProgram(program);
    getLinkStatus(program);

    glDetachShader(program, vs);
    glDeleteShader(vs);

    return program;
}

int main() {
    GLFWwindow* window = init();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    initMouse(window); // function in cam.h
    initDefaultTexture();

    GLuint program = createShader("src/shader/triangle.vert", "src/shader/triangle.frag");
    GLuint shadowShader = createShader("src/shader/shadow.vert");

    GLuint framebuffer;
    glCreateFramebuffers(1, &framebuffer);

    // Create textures (and frame buffers)

    /*GLuint tex;
    {
        tex = createTextureFromImage("asset/green.jpg");
        glBindTextureUnit(0, tex);
    }*/

    GLuint shadowmap; int shadowmap_width = 2048; int shadowmap_height = 2048;
    {
        shadowmap = createTexture(shadowmap_width, shadowmap_height, GL_DEPTH_COMPONENT32F, false, nullptr);
        glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, shadowmap, 0);
        glBindTextureUnit(1, shadowmap);
    }

    // Create geometry

    std::vector<WorldObj> objects;
    std::vector<Vertex> vertices;

    objects.push_back(WorldObj::create(vertices, "asset/test_scene.obj", "asset/green.jpg"));
    objects.push_back(WorldObj::create(vertices, "asset/cat.obj", "asset/cat.jpg"));
    objects.back().model = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f)), (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f));

    genTangents(vertices);

    GLuint buffer;
    glCreateBuffers(1, &buffer);
    glNamedBufferStorage(buffer, vertices.size() * sizeof(Vertex), vertices.data(), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);

    FreeCam cam = FreeCam { Cam { glm::vec3(0.0f,1.0f,0.0f), 0.0, 0.0 } };
    DirectionalLight sun = DirectionalLight{};
    sun.illuminateArea(10.0);
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

    double last_time_sec = 0.0;

    // event loop (each iteration of this loop is one frame of the application)
    while (!glfwWindowShouldClose(window)) {
        // calculate delta time
        double cur_time_sec = glfwGetTime();
        double dt = cur_time_sec - last_time_sec;
        last_time_sec = cur_time_sec;
        double lightAzimuth = glfwGetTime() / 3.0;
        glm::vec3 lightDir = getAngle(lightAzimuth, -PI / 4.0);

        // check for user input
        glfwPollEvents();

        windowFocusControl(window);
        moveFreeCam(window, cam, dt);

        glm::mat4 view = glm::lookAt(cam.cam.pos, cam.cam.pos + cam.cam.lookDir(), cam_up);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float) (width) / height, 0.1f, 100.0f);

        // Update the light direction
        sun.setLightDirVec3(lightDir);

        // Draw to framebuffer (shadow map)
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, shadowmap_width, shadowmap_height);
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(shadowShader);

        for (int i = 0; i < objects.size(); i++) {
            WorldObj& o = objects[i];

            glProgramUniformMatrix4fv(shadowShader, 0, 1, GL_FALSE, glm::value_ptr(o.model));
            glProgramUniformMatrix4fv(shadowShader, 4, 1, GL_FALSE, glm::value_ptr(sun.combined));

            glDrawArrays(GL_TRIANGLES, o.offset, o.size);
        }

        // Draw to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        glProgramUniformMatrix4fv(program, 4, 1, GL_FALSE, glm::value_ptr(projection * view));
        glProgramUniformMatrix4fv(program, 11, 1, GL_FALSE, glm::value_ptr(sun.combined));
        glProgramUniform3fv(program, 15, 1, glm::value_ptr(cam.cam.pos));
        glProgramUniform3fv(program, 16, 1, glm::value_ptr(lightDir));
        glProgramUniform3fv(program, 17, 1, glm::value_ptr(lightColor));

        for (int i = 0; i < objects.size(); i++) {
            WorldObj& o = objects[i];
            glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
            glBindTextureUnit(0, objects[i].tex);

            glProgramUniformMatrix4fv(program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
            glProgramUniformMatrix3fv(program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

            glDrawArrays(GL_TRIANGLES, o.offset, o.size);
        }

        // tell the OS to display the frame
        glfwSwapBuffers(window);
    }

    //glDeleteFrameBuffers(1, &framebuffer);
    glDeleteBuffers(1, &buffer);
    //glDeleteTextures(1, &tex); // TODO delete all textures here
    glDeleteProgram(program);
    glDeleteProgram(shadowShader);

    cleanup(window);
}

/* Gameplay related functions */

// Given an object, give a transform matrix that roughly scales it down to appropriate size; don't use for percise transformations
glm::mat4 baseTransform(const std::vector<Vertex>& vertices) {
    glm::vec3 minPoint(INFINITY);
    glm::vec3 maxPoint(-INFINITY);

    for (Vertex v : vertices) {
        minPoint = glm::min(minPoint, v.position);
        maxPoint = glm::max(maxPoint, v.position);
    }

    const glm::vec3 center = (maxPoint + minPoint) / 2.0f;
    const glm::vec3 size = maxPoint - minPoint;
    const float scale = 1.0f / std::max(size.x, std::max(size.y, size.z));

    return glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -center);
}

/* Graphics Functions */

void genTangents(std::vector<Vertex>& vertices) {
    std::unordered_map<VertexKey, glm::vec3> accumulatedTangents;
    std::unordered_map<VertexKey, int> counts;

    for (size_t i = 0; i < vertices.size(); i += 3) {
        Vertex& v0 = vertices[i];
        Vertex& v1 = vertices[i + 1];
        Vertex& v2 = vertices[i + 2];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec2 deltaUV1 = v1.uv - v0.uv;
        glm::vec2 deltaUV2 = v2.uv - v0.uv;

        float det = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
        glm::vec3 tangent(1.0f, 0.0f, 0.0f);

        if (det != 0.0f) {
            float invDet = 1.0f / det;
            tangent = invDet * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
        }

        VertexKey keys[3] = {
            {v0.position, v0.normal, v0.uv},
            {v1.position, v1.normal, v1.uv},
            {v2.position, v2.normal, v2.uv}
        };

        for (const auto& key : keys) {
            accumulatedTangents[key] += tangent;
            counts[key]++;
        }
    }

    for (auto& vertex : vertices) {
        VertexKey key = { vertex.position, vertex.normal, vertex.uv };
        if (counts[key] > 0) {
            vertex.tangent = glm::normalize(accumulatedTangents[key]);
        }
    }
}

/* Files */

std::string readFile(const char* path) {
    std::ifstream infile(path);
    return std::string(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>());
}

/* Window */

// Create window
GLFWwindow* init() {
    // initialize GLFW and let it know the OpenGL version we intend to use
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwSwapInterval(1);

    // create window using GLFW and set it as the active OpenGL context for the current thread
    GLFWwindow* window = glfwCreateWindow(width, height, "Cyber Seagull 4", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // load OpenGL function pointers from the graphics driver
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));

    // enable OpenGL debug messages
    // DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

    // make OpenGL normal (lol)
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // initialize ImGUI
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    // make stb flip images
    stbi_set_flip_vertically_on_load(true);

    return window;
}

// Delete window
void cleanup(GLFWwindow* window) {
    glDeleteVertexArrays(1, &vao);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}