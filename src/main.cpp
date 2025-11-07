// STL
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include <cmath>
#include <stdlib.h>
#include <time.h>
#include <atomic>
#include <thread>

#define NOMINMAX

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
#include <glm/gtx/matrix_decompose.hpp>

// miniaudio
#include <miniaudio.h>

// This project
#include "util.h"

#include "client.h"
#include "message.h"

#include "cam.h"
#include "debug.h"
#include "light.h"
#include "world_object.h"
#include "particle.h"
#include "game.h"
#include "music.h"
#include "input.h"

#include <random>
static std::mt19937 rng{ std::random_device{}() };
static std::uniform_real_distribution<float> randomPitch(0.02f, 1.15f);

std::vector<ma_sound*> liveSounds;

FreeCam cam = FreeCam{ Cam { glm::vec3(0.0f,1.0f,0.0f), 0.0, 0.0 } };

// Static data
static int width = 1920;
static int height = 1080;
static GLuint vao;
ma_engine engine;
double cur_time_sec;
std::vector<Entity> objects;

static struct {
    Mesh test_scene;
    Mesh cat;
} meshes;

static struct {
    GLuint green;
    GLuint cat;
    GLuint skybox;
    GLuint banner;
    GLuint weezer;
} textures;

// Networking globals (client side only)
uint16_t player_id = 0xffff;
seaclient  client;

// connect gate
static std::atomic<bool> g_connecting = false;
static std::string g_last_connect_error;
static double g_connect_started = 0.0;

void try_connect(const std::string& ip, uint16_t port) {
    if (client.IsConnected() || g_connecting.load()) return;
    g_connecting = true;
    g_last_connect_error.clear();

    std::thread([ip, port]() {
        try {
            client.Connect(ip, port);
        }
        catch (const std::exception& e) {
            g_last_connect_error = e.what();
        }
        catch (...) {
            g_last_connect_error = "Unknown error while connecting";
        }
        g_connecting = false;
        }).detach();
}


// forward declarations
GLFWwindow* init();
void cleanup(GLFWwindow* window);
std::string readFile(const char* path);
glm::mat4 baseTransform(const std::vector<Vertex>& vertices);
void genTangents(std::vector<Vertex>& vertices);
void cleanupFinishedSounds();
void playWithRandomPitch(ma_engine* engine, const char* filePath);
void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch = 1);

// gameplay
void throw_cats();
void throw_cat(int cat_num, bool owned, double start_time);

const F64  distancebetweenthetwoshipswhichshallherebyshootateachother = 100;
const glm::vec3 catstartingpos(10.0f, 0, 10.0f);
const F64  distbetweencats = -5;

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

int main(int argc, char** argv) {
    // Init graphics window and GL/ImGui
    GLFWwindow* window = init();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    initMouse(window); // function in cam.h
    glfwUpdateGamepadMappings("03000000ba1200004b07000000000000,Guitar Hero,platform:Windows,a:b0,b:b1,x:b2,y:b3,dpdown:+a1,dpup:-a1");
    initDefaultTexture();

    GLuint program = createShader("src/shader/triangle.vert", "src/shader/triangle.frag");
    GLuint shadowShader = createShader("src/shader/shadow.vert");
    GLuint cubeProgram = createShader("src/shader/cube.vert", "src/shader/cube.frag");
    GLuint particleProgram = createShader("src/shader/particle.vert", "src/shader/particle.frag");

    // Create textures (and frame buffers)
    GLuint framebuffer;
    glCreateFramebuffers(1, &framebuffer);
    GLuint shadowmap; int shadowmap_width = 2048; int shadowmap_height = 2048;
    {
        shadowmap = createTexture(shadowmap_width, shadowmap_height, GL_DEPTH_COMPONENT32F, false, nullptr);
        glTextureParameteri(shadowmap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(shadowmap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        F32 border[]{ 9999999.0F, 9999999.0F, 9999999.0F, 9999999.0F };
        glTextureParameterfv(shadowmap, GL_TEXTURE_BORDER_COLOR, border);

        glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, shadowmap, 0);
        glBindTextureUnit(1, shadowmap);
    }

    // Create geometry
    std::vector<Vertex> vertices;

    meshes.test_scene = Mesh::create(vertices, "asset/test_scene.obj");
    meshes.cat = Mesh::create(vertices, "asset/cat.obj");

    textures.green = createTextureFromImage("asset/green.jpg");
    textures.cat = createTextureFromImage("asset/cat.jpg");

    stbi_set_flip_vertically_on_load(false);
    textures.weezer = createTextureFromImage("asset/weezer.jfif");
    textures.banner = createTextureFromImage("asset/seagull_banner.png");
    stbi_set_flip_vertically_on_load(true);

    objects.push_back(Entity::create(&meshes.test_scene, textures.green));
    objects.push_back(Entity::create(&meshes.cat, textures.cat,
        glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f)),
            (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f, 0.1f, 0.1f)), NONEMITTER));

    for (int i = 0; i < 6; i++) {
        objects.push_back(Entity::create(&meshes.cat, textures.cat,
            glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f),
                glm::vec3(catstartingpos.x + (distbetweencats * i), catstartingpos.y, catstartingpos.z)),
                (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f)), CANNON, true));

        objects.push_back(Entity::create(&meshes.cat, textures.cat,
            glm::scale(glm::rotate(glm::rotate(glm::translate(glm::mat4(1.0f),
                glm::vec3(catstartingpos.x + (distbetweencats * i), catstartingpos.y,
                    catstartingpos.z + distancebetweenthetwoshipswhichshallherebyshootateachother)),
                (float)-PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), (float)PI, glm::vec3(0.0f, 0.0f, 1.0f)),
                glm::vec3(0.1f)), CANNON));
    }

    // Connect to a server, starts with local by default 
    std::string server_ip = "136.112.101.5";
    if (argc > 1 && argv[1][0] != '-') server_ip = argv[1];
    
    try_connect(server_ip, 1951);

    objects.push_back(Entity::create(&meshes.cat, textures.cat,
        glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f),
            glm::vec3(-15.0f, 0.0f, 10.0f)), (float)-PI / 2.0f,
            glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.1f)), CANNON));

    genTangents(vertices);

    {
        const char* cubemap_files[6] = {
            "asset/skybox/right.jpg",
            "asset/skybox/left.jpg",
            "asset/skybox/top.jpg",
            "asset/skybox/bottom.jpg",
            "asset/skybox/front.jpg",
            "asset/skybox/back.jpg"
        };
        textures.skybox = createCubeTexture(cubemap_files);
    }

    // Make static particle sources
    ParticleSource particleSource{ glm::vec3(0.0f), glm::vec3(0.01f),
        RGBA8 { 255,255,255,255 }, 0.1f, 5.0f };

    GLuint buffer;
    glCreateBuffers(1, &buffer);
    glNamedBufferStorage(buffer, vertices.size() * sizeof(Vertex), vertices.data(), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);

    GLuint pvertex_buffer;
    glCreateBuffers(1, &pvertex_buffer);
    glNamedBufferData(pvertex_buffer, sizeof(pvertex_vertex), pvertex_vertex, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pvertex_buffer);

    GLuint pdata_buffer;
    glCreateBuffers(1, &pdata_buffer);
    glNamedBufferData(pdata_buffer, sizeof(pvertex_data), pvertex_data, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pdata_buffer);

    DirectionalLight sun = DirectionalLight{};
    sun.illuminateArea(10.0);
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

    double last_time_sec = 0.0;

    ma_engine_init(NULL, &engine);
    ma_engine_set_volume(&engine, 0.1f);
    playSound(&engine, "asset/seagull-flock-sound-effect-206610.wav", MA_TRUE);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // calculate delta time
        cur_time_sec = glfwGetTime();
        double dt = cur_time_sec - last_time_sec;
        last_time_sec = cur_time_sec;
        double lightAzimuth = glfwGetTime() / 3.0;
        glm::vec3 lightDir = getAngle(lightAzimuth, -PI / 4.0);

        // per-frame input
        glfwPollEvents();
        windowFocusControl(window);

        // local fire + network send
        throw_cats();

        // movement
        GLFWgamepadstate state{};
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
            moveFreeCamGamepad(window, cam, dt, state);
        }
        else {
            moveFreeCam(window, cam, dt);
        }

        for (int i = 0; i < (int)objects.size(); i++) {
            if (objects[i].update) {
                if (!objects[i].update(objects[i], cur_time_sec)) {
                    objects.erase(objects.begin() + i);
                    i--;
                }
            }
        }

        cleanupFinishedSounds();

        // pump(fornite shotgun) client incoming messages (assigns player_id, handles remote cat fire)
        if (client.IsConnected()) {
            client.check_messages();
        }

        // gate: connected or timed out (lets the button work again)
        if (client.IsConnected()) {
            g_connecting = false;
        }
        else if (g_connecting && (glfwGetTime() - g_connect_started) > 5.0) {
            // assume the attempt failed
            g_connecting = false;
        }

        // Update particles
        advanceParticles(dt);
        particleSource.spawnParticle();
        sortParticles(cam.cam, cam.cam.lookDir());
        packParticles();

        glNamedBufferSubData(pvertex_buffer, 0,
            sizeof(ParticleVertex) * lastUsedParticle * VERTICES_PER_PARTICLE, pvertex_vertex);
        glNamedBufferSubData(pdata_buffer, 0,
            sizeof(ParticleData) * lastUsedParticle, pvertex_data);

        // get cam matrices
        glm::mat4 view = glm::lookAt(cam.cam.pos, cam.cam.pos + cam.cam.lookDir(), cam_up);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)(width) / height, 0.1f, 1000.0f);

        // Update the light direction
        sun.setLightDirVec3(lightDir);

        // Draw to framebuffer (shadow map)
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, shadowmap_width, shadowmap_height);
        glClearDepth(9999999.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glClearDepth(1.0);

        glUseProgram(shadowShader);

        for (int i = 0; i < (int)objects.size(); i++) {
            Entity& o = objects[i];

            glProgramUniformMatrix4fv(shadowShader, 0, 1, GL_FALSE, glm::value_ptr(o.model));
            glProgramUniformMatrix4fv(shadowShader, 4, 1, GL_FALSE, glm::value_ptr(sun.combined));

            glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
        }

        // Draw to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        // Draw the skybox
        glUseProgram(cubeProgram);
        glBindTextureUnit(2, textures.skybox);
        glProgramUniformMatrix4fv(cubeProgram, 0, 1, GL_FALSE,
            glm::value_ptr(projection * glm::mat4(glm::mat3(view))));
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        // Draw scene
        glUseProgram(program);

        glProgramUniformMatrix4fv(program, 4, 1, GL_FALSE, glm::value_ptr(projection * view));
        glProgramUniformMatrix4fv(program, 11, 1, GL_FALSE, glm::value_ptr(sun.combined));
        glProgramUniform3fv(program, 15, 1, glm::value_ptr(cam.cam.pos));
        glProgramUniform3fv(program, 16, 1, glm::value_ptr(lightDir));
        glProgramUniform3fv(program, 17, 1, glm::value_ptr(lightColor));
        glProgramUniform2f(program, 18, (F32)shadowmap_height, (F32)shadowmap_width);

        for (int i = 0; i < (int)objects.size(); i++) {
            Entity& o = objects[i];
            glm::mat3 normalTransform = glm::inverse(glm::transpose(glm::mat3(o.model)));
            glBindTextureUnit(0, objects[i].tex);

            glProgramUniformMatrix4fv(program, 0, 1, GL_FALSE, glm::value_ptr(o.model));
            glProgramUniformMatrix3fv(program, 8, 1, GL_FALSE, glm::value_ptr(normalTransform));

            glDrawArrays(GL_TRIANGLES, o.mesh->offset, o.mesh->size);
        }

        // Draw particles
        glUseProgram(particleProgram);
        glProgramUniformMatrix4fv(particleProgram, 0, 1, GL_FALSE, glm::value_ptr(view));
        glProgramUniformMatrix4fv(particleProgram, 4, 1, GL_FALSE, glm::value_ptr(projection));
        glBindTextureUnit(0, textures.green);
        glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_PARTICLE * lastUsedParticle);

        // UI (banner + connect controls)
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;

        ImGui::SetNextWindowSize(ImVec2(1000, 1000));
        ImGui::SetNextWindowPos(ImVec2((width - 728.0f) / 2, height * 0.01f));
        ImGui::Begin("State", NULL, flags);
        ImGui::Image((ImTextureID)textures.banner, ImVec2(728.0f, 90.0f));
        ImGui::End();

        ImGui::SetNextWindowSize(ImVec2(500, 500));
        ImGui::SetNextWindowPos(ImVec2(20, 20));
        ImGui::Begin("ip", NULL, flags);

        // Editable server IP and connect disconnect buttons
        static bool ipbuf_init = false;
        static char ipbuf[64];
        if (!ipbuf_init) {
            std::snprintf(ipbuf, sizeof(ipbuf), "%s", server_ip.c_str());
            ipbuf_init = true;
        }
        if (ImGui::InputText("Server IP", ipbuf, sizeof(ipbuf))) {
            server_ip = ipbuf;
        }

        if (!client.IsConnected()) {
            if (!g_connecting) {
                if (!g_last_connect_error.empty()) {
                    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", g_last_connect_error.c_str());
                }
                if (ImGui::Button("Connect")) {
                    g_connecting = true;
                    g_connect_started = glfwGetTime();
                    try_connect(server_ip, 1951);
                }
            }
            else {
                ImGui::Text("Connecting to %s...", server_ip.c_str());
                if (ImGui::Button("Cancel")) { client.Disconnect(); } // leave g_connecting; thread will clear it :     )
            }
        }

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // tell the OS to display the frame
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteBuffers(1, &buffer);
    glDeleteProgram(program);
    glDeleteProgram(shadowShader);

    ma_engine_uninit(&engine);
    cleanup(window);
    return 0;
}

//Gameplay/Networking

void throw_cats() {
    bool send = false;
    std::vector<uint8_t> cats;

    for (int i = 0; i < numcats; i++) {
        if (cats_thrown[i]) {
            throw_cat(i, true); // local projectile + sfx 
            cats.push_back(static_cast<uint8_t>(i));
            cats_thrown[i] = false;
            send = true;
        }
    }

    if (send && client.IsConnected() && player_id != 0xffff) {
        client.send_player_cat_fire(player_id, cur_time_sec, cats);
    }
}

void throw_cat(int cat_num, bool owned, double start_time) {
    if (start_time < 0.0) start_time = cur_time_sec;

    // Prefer a cannon whose owned flag matches, otherwise use any matching cat_id.
    int best = -1, fallback = -1;
    for (int j = 0; j < (int)objects.size(); ++j) {
        if (objects[j].type == CANNON && objects[j].cat_id == cat_num) {
            if (objects[j].owned == owned) { best = j; break; }
            if (fallback == -1) fallback = j;
        }
    }
    const int i = (best != -1 ? best : fallback);
    if (i == -1) return; // no suitable cannon found

    playSound(&engine, "asset/cat-meow-401729-2.wav", false, weezer_notes[cat_num]);

    // Spawn projectile using the chosen cannon’s transform
    objects.push_back(Entity::create(&meshes.cat, textures.cat, objects[i].model, PROECTILE));
    Entity& p = objects.back();

    p.start_time = start_time;
    p.pretransmodel = p.model;
    p.shoot_angle = owned ? 0.0f : PI;

    p.update = [](Entity& cat, F64 curtime) {
        cat.model = toModel(
            (curtime - cat.start_time) * 50, // distance along lane
            0,                               // lane index
            distancebetweenthetwoshipswhichshallherebyshootateachother,
            cat.shoot_angle
        ) * cat.pretransmodel;

        // keep alive while above ground
        return (cat.model[3][1] >= 0.0f);
        };
}

void cleanupFinishedSounds() {
    for (auto it = liveSounds.begin(); it != liveSounds.end();) {
        ma_sound* s = *it;
        if (!ma_sound_is_playing(s) && ma_sound_at_end(s)) {
            ma_sound_uninit(s);
            delete s;
            it = liveSounds.erase(it);
        }
        else {
            ++it;
        }
    }
}

void playWithRandomPitch(ma_engine* engine, const char* filePath) {
    ma_sound* s = new ma_sound{};
    if (ma_sound_init_from_file(engine, filePath,
        MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE,
        nullptr, nullptr, s) == MA_SUCCESS) {
        ma_sound_set_pitch(s, randomPitch(rng));
        ma_sound_start(s);
        liveSounds.push_back(s);
    }
    else {
        delete s;
    }
}

void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch) {
    ma_sound* s = new ma_sound{};
    if (ma_sound_init_from_file(engine, filePath,
        MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE,
        nullptr, nullptr, s) == MA_SUCCESS) {
        ma_sound_set_pitch(s, pitch);
        ma_sound_start(s);
        liveSounds.push_back(s);
    }
    else {
        delete s;
    }
}

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

    return glm::scale(glm::mat4(1.0f), glm::vec3(scale)) *
        glm::translate(glm::mat4(1.0f), -center);
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

std::string readFile(const char* path) {
    std::ifstream infile(path);
    return std::string(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>());
}

void window_size_callback(GLFWwindow* window, int new_width, int new_height) {
    width = new_width;
    height = new_height;
}

GLFWwindow* init() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwSwapInterval(1);

    GLFWwindow* window = glfwCreateWindow(width, height, "Cyber Seagull 4", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, nullptr);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    stbi_set_flip_vertically_on_load(true);

    return window;
}

void cleanup(GLFWwindow* window) {
    glDeleteVertexArrays(1, &vao);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
