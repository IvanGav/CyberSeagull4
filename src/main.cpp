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
#include "cam.h"

// Static data
static constexpr int width = 1920;
static constexpr int height = 1080;
static GLuint vao;

// forward declarations (probably replace later by .h files in current project)
struct Vertex;
GLFWwindow* init();
void cleanup(GLFWwindow* window);
std::string readFile(const char* path);
void getCompileStatus(GLuint shader);
void getLinkStatus(GLuint program);
void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam);
glm::mat4 baseTransform(const std::vector<Vertex>& vertices);

struct Vertex {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 normal;
    alignas(16) glm::vec3 tangent;
    alignas(8) glm::vec2 uv;
};

struct VertexKey {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    bool operator==(const VertexKey& other) const {
        return position == other.position && normal == other.normal && uv == other.uv;
    }
};

namespace std {
    template<>
    struct hash<VertexKey> {
        size_t operator()(const VertexKey& key) const {
            size_t h1 = hash<float>()(key.position.x) ^ hash<float>()(key.position.y) << 1 ^ hash<float>()(key.position.z) << 2;
            size_t h2 = hash<float>()(key.normal.x) ^ hash<float>()(key.normal.y) << 1 ^ hash<float>()(key.normal.z) << 2;
            size_t h3 = hash<float>()(key.uv.x) ^ hash<float>()(key.uv.y) << 1;
            return h1 ^ h2 ^ h3;
        }
    };
}

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

// Create a texture
GLuint createTexture(int texWidth, int texHeight, GLenum internalFormat, void* pixels = nullptr, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE) {
    GLuint tex;
    int numMips = std::log2(std::max(texWidth, texHeight)) + 1;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, numMips, internalFormat, texWidth, texHeight);

    if (pixels) {
        glTextureSubImage2D(tex, 0, 0, 0, texWidth, texHeight, format, type, pixels);
        glGenerateTextureMipmap(tex);

        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY, 16.0f);
    }

    return tex;
}

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

int main() {
    GLFWwindow* window = init();
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    initMouse(window); // function in cam.h

    GLuint program = createShader("src/shader/triangle.vert", "src/shader/triangle.frag");
    GLuint shadowShader = createShader("src/shader/shadow.vert");

    GLuint framebuffer;
    glCreateFramebuffers(1, &framebuffer);

    GLuint tex;
    {
        int texWidth, texHeight;
        stbi_uc* pixels = stbi_load("asset/cat_hot_dog.png", &texWidth, &texHeight, nullptr, STBI_rgb_alpha);
        tex = createTexture(texWidth, texHeight, GL_RGBA8, pixels, GL_RGBA, GL_UNSIGNED_BYTE);
        stbi_image_free(pixels);
        glBindTextureUnit(0, tex);
    }

    GLuint shadowmap;
    {
        shadowmap = createTexture(2048, 2048, GL_DEPTH_COMPONENT32F);

        glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, shadowmap, 0);

        glBindTextureUnit(2, shadowmap);
    }

    std::vector<Vertex> vertices;

    // Temporary code to create a quad
    {
        Vertex v;
        v.position = glm::vec3(-0.5f, -0.5f, 0.0f);
        v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        v.uv = glm::vec2(0.0f, 0.0f);
        vertices.push_back(v);

        v.position = glm::vec3(-0.5f, 0.5f, 0.0f);
        v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        v.uv = glm::vec2(0.0f, 1.0f);
        vertices.push_back(v);
        
        v.position = glm::vec3(0.5f, 0.5f, 0.0f);
        v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        v.uv = glm::vec2(1.0f, 1.0f);
        vertices.push_back(v);
        
        v.position = glm::vec3(0.5f, 0.5f, 0.0f);
        v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        v.uv = glm::vec2(1.0f, 1.0f);
        vertices.push_back(v);

        v.position = glm::vec3(-0.5f, -0.5f, 0.0f);
        v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        v.uv = glm::vec2(0.0f, 0.0f);
        vertices.push_back(v);

        v.position = glm::vec3(0.5f, -0.5f, 0.0f);
        v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        v.uv = glm::vec2(1.0f, 0.0f);
        vertices.push_back(v);
    }

    genTangents(vertices);

    GLuint buffer;
    glCreateBuffers(1, &buffer);
    glNamedBufferStorage(buffer, vertices.size() * sizeof(Vertex), vertices.data(), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    //glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 2.0f);
    FreeCam cam = FreeCam{ Cam { glm::vec3(0.0f,0.0f,-2.0f), 0.0, 0.0 } };
    glm::vec3 lightAngle = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

    int useNormalMap = 1;
    double last_time_sec = 0.0;

    // event loop (each iteration of this loop is one frame of the application)
    while (!glfwWindowShouldClose(window)) {
        // calculate delta time
        double cur_time_sec = glfwGetTime();
        double dt = cur_time_sec - last_time_sec;
        last_time_sec = cur_time_sec;

        // check for user input
        glfwPollEvents();

        /*if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
            useNormalMap = 1;
        }
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
            useNormalMap = 0;
        }*/

        windowFocusControl(window);
        moveFreeCam(window, cam, dt);

        // TODO: don't create these in a loop; only when necessary
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 view = glm::lookAt(cam.cam.pos, cam.cam.pos + cam.cam.lookDir(), cam_up);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float) (width) / height, 0.1f, 100.0f);

        // TODO: same here; only when necessary
        lightAngle = glm::vec3(cosf(glfwGetTime()), 0.5f, sinf(glfwGetTime()));
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), lightAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightProjection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

        glm::mat3 normal = glm::inverse(glm::transpose(glm::mat3(model)));

        // Draw to framebuffer (shadow map)
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(shadowShader);
        glProgramUniformMatrix4fv(shadowShader, 0, 1, GL_FALSE, glm::value_ptr(model));
        glProgramUniformMatrix4fv(shadowShader, 4, 1, GL_FALSE, glm::value_ptr(lightProjection * lightView));

        glDrawArrays(GL_TRIANGLES, 0, vertices.size());

        // Draw to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        glProgramUniformMatrix4fv(program, 0, 1, GL_FALSE, glm::value_ptr(model));
        glProgramUniformMatrix4fv(program, 4, 1, GL_FALSE, glm::value_ptr(projection * view));
        glProgramUniformMatrix3fv(program, 8, 1, GL_FALSE, glm::value_ptr(normal));

        glProgramUniform3fv(program, 11, 1, glm::value_ptr(cam.cam.pos));
        glProgramUniform3fv(program, 12, 1, glm::value_ptr(lightAngle));
        glProgramUniform3fv(program, 13, 1, glm::value_ptr(lightColor));

        glProgramUniform1i(program, 14, useNormalMap);

        glProgramUniformMatrix4fv(program, 15, 1, GL_FALSE, glm::value_ptr(lightProjection * lightView));

        glDrawArrays(GL_TRIANGLES, 0, vertices.size());

        // tell the OS to display the frame
        glfwSwapBuffers(window);
    }

    //glDeleteFrameBuffers(1, &framebuffer);
    glDeleteBuffers(1, &buffer);
    glDeleteTextures(1, &tex);
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

/* Misc */

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam) {
    // ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
        return;
    }

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << "): " << message << std::endl;

    switch (source) {
    case GL_DEBUG_SOURCE_API:
        std::cout << "Source: API" << std::endl;
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        std::cout << "Source: Window System" << std::endl;
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        std::cout << "Source: Shader Compiler" << std::endl;
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        std::cout << "Source: Third Party" << std::endl;
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        std::cout << "Source: Application" << std::endl;
        break;
    case GL_DEBUG_SOURCE_OTHER:
        std::cout << "Source: Other" << std::endl;
        break;
    }

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        std::cout << "Type: Error" << std::endl;
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        std::cout << "Type: Deprecated Behaviour" << std::endl;
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        std::cout << "Type: Undefined Behaviour" << std::endl;
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        std::cout << "Type: Portability" << std::endl;
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        std::cout << "Type: Performance" << std::endl;
        break;
    case GL_DEBUG_TYPE_MARKER:
        std::cout << "Type: Marker" << std::endl;
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        std::cout << "Type: Push Group" << std::endl;
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        std::cout << "Type: Pop Group" << std::endl;
        break;
    case GL_DEBUG_TYPE_OTHER:
        std::cout << "Type: Other" << std::endl;
        break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        std::cout << "Severity: high" << std::endl;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        std::cout << "Severity: medium" << std::endl;
        break;
    case GL_DEBUG_SEVERITY_LOW:
        std::cout << "Severity: low" << std::endl;
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        std::cout << "Severity: notification" << std::endl;
        break;
    }
}

void getCompileStatus(GLuint shader) {
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success == GL_FALSE) {
        GLint logSize = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);

        GLchar* buf = new GLchar[logSize];
        glGetShaderInfoLog(shader, logSize, &logSize, buf);

        std::cout << buf << std::endl;

        delete[] buf;
    }
}

void getLinkStatus(GLuint program) {
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (success == GL_FALSE) {
        GLint logSize = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);

        GLchar* buf = new GLchar[logSize];
        glGetProgramInfoLog(program, logSize, &logSize, buf);

        std::cout << buf << std::endl;

        delete[] buf;
    }
}