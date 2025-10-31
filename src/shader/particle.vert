#version 460 core

vec3 vertex_offsets[6] = {
	{-1.0f, -1.0f, 0.0f},
	{1.0f, 1.0f, 0.0f},
	{-1.0f, 1.0f, 0.0f},
	{-1.0f, -1.0f, 0.0f},
	{1.0f, -1.0f, 0.0f},
	{1.0f, 1.0f, 0.0f}
};

vec2 uvs[6] = {
	{0.0f, 0.0f},
	{1.0f, 1.0f},
	{0.0f, 1.0f},
	{0.0f, 0.0f},
	{1.0f, 0.0f},
	{1.0f, 1.0f}
};

struct ParticleVertex {
	int particleid;
	int vertexid; // from 0 to 5 - which vertex in this particle quad
};

struct Particle {
    vec3 pos;
    vec4 color;
	float size;
};

layout(binding = 1, std430) readonly buffer VertexBuffer {
	ParticleVertex vertices[];
};

layout(binding = 2, std430) readonly buffer ParticleBuffer {
	Particle particles[];
};

layout(location = 0) out vec2 outUV;

layout(location = 0) uniform mat4 view;
layout(location = 4) uniform mat4 projection;

void main() {
	ParticleVertex v = vertices[gl_VertexID];
	Particle p = particles[v.particleid];

	vec4 worldSpacePos = vec4(p.pos, 1.0f);

	outUV = uvs[v.vertexid];
	
	gl_Position = projection * (view * worldSpacePos + vec4(vertex_offsets[v.vertexid], 0.0f) * p.size);
}