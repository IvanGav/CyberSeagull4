#pragma once

// GLAD: OpenGL function loader
#include <glad/glad.h>

// glm: linear algebra library
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

// Add a given .obj object to the `vertices`. Return the number of vertices added.
int loadObj(std::vector<Vertex>& vertices, const char* path) {
    int offset = vertices.size();

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path);
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex v;

            v.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            v.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };

            v.uv = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertices.push_back(v);
        }
    }

    return vertices.size() - offset;
}

// Create a texture; if no pixels specified, blank texture
GLuint createTexture(int texWidth, int texHeight, GLenum internalFormat, bool genMips, void* pixels = nullptr, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE) {
    GLuint tex;
    int numMips = genMips ? std::log2(std::max(texWidth, texHeight)) + 1 : 1;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, numMips, internalFormat, texWidth, texHeight);

    if (pixels) {
        glTextureSubImage2D(tex, 0, 0, 0, texWidth, texHeight, format, type, pixels);
        glGenerateTextureMipmap(tex);

        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY, 16.0f);
    }
    else {
        glTextureParameteri(tex, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE); // TODO SHADOW HERE
        glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    return tex;
}

GLuint createTextureFromImage(const char* path) {
    int texWidth, texHeight;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, nullptr, STBI_rgb_alpha);
    GLuint tex = createTexture(texWidth, texHeight, GL_RGBA8, true, pixels, GL_RGBA, GL_UNSIGNED_BYTE);
    stbi_image_free(pixels);
    return tex;
}

// Create a cube texture. Filenames should have right, left, top, bottom, front, back filenames
GLuint createCubeTexture(const char** filenames) {
    int texWidth, texHeight;

    GLuint cubemap;
    //glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemap);
    //glTextureStorage2D(cubemap, 1, GL_RGBA8, texWidth, texHeight);
    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    glTextureParameteri(cubemap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(cubemap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(false);
    for (int i = 0; i < 6; i++) {
        stbi_uc* pixels = stbi_load(filenames[i], &texWidth, &texHeight, nullptr, STBI_rgb);
        //glTextureSubImage3D(cubemap, 0, 0, 0, i, texWidth, texHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, cubemapData[i]);
        //glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, texWidth, texHeight, GL_RGB, GL_UNSIGNED_BYTE, cubemapData[i]);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
    }
    stbi_set_flip_vertically_on_load(true);
    return cubemap;
}

// default texture
GLuint default_tex;

// call at the beginning of main
void initDefaultTexture() {
    int hardcodedTextureData[16 * 16];
    // The classic purple and black checker pattern
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            hardcodedTextureData[y * 16 + x] = x < 8 == y < 8 ? 0b11111111111111110000000011111111 : 0b11111111000000000000000000000000;
        }
    }
    default_tex = createTexture(16, 16, GL_RGBA8, false, hardcodedTextureData);
}

struct Mesh {
    int offset;
    int size;

    static Mesh create(std::vector<Vertex>& vertices, const char* obj_path) {
        Mesh o;
        o.offset = vertices.size();
        o.size = loadObj(vertices, obj_path);
        return o;
    }
};

struct Entity {
    Mesh* mesh;
    GLuint tex;
    glm::mat4 model;
    glm::mat4 pretransmodel;
    F64 start_time;

    // You can use "default_tex" if you don't need a texture
    static Entity create(Mesh* mesh, GLuint tex) {
        Entity o{};
        o.mesh = mesh;
        o.tex = tex;
        o.model = glm::mat4(1.0f);
        return o;
    }

    static Entity create(Mesh* mesh, GLuint tex, glm::mat4 initial_transform) {
        Entity o{};
        o.mesh = mesh;
        o.tex = tex;
        o.model = initial_transform;
        return o;
    }

    Entity copy() {
        return Entity{ mesh, tex, model };
    }

    Entity copyWithModel(glm::mat4 model) {
        return Entity{ mesh, tex, model };
    }

    void (*update)(Entity& object, F64 dt);
};