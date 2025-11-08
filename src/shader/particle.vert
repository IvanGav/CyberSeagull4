#version 460 core

vec3 vertex_offsets[6] = {
	{-1.0f, -1.0f, 0.0f},
	{1.0f, 1.0f, 0.0f},
	{-1.0f, 1.0f, 0.0f},
	{-1.0f, -1.0f, 0.0f},
	{1.0f, -1.0f, 0.0f},
	{1.0f, 1.0f, 0.0f}
};

float cylindricalScaleX[6] = { -1.0, 1.0, -1.0, -1.0, 1.0, 1.0 };
float cylindricalScaleY[6] = { 0.0, 1.0, 1.0, 0.0, 0.0, 1.0 };

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
	vec3 dir;
	float size;
	float age;
	uint sheetRes;
	uint texNum;
};

layout(binding = 1, std430) readonly buffer VertexBuffer {
	ParticleVertex vertices[];
};

layout(binding = 2, std430) readonly buffer ParticleBuffer {
	Particle particles[];
};

layout(location = 0) out vec2 outUV;
layout(location = 1) flat out uint sheetRes;
layout(location = 2) out float age;
layout(location = 3) flat out uint texNum;

layout(location = 0) uniform mat4 view;
layout(location = 4) uniform mat4 projection;

void main() {
	ParticleVertex v = vertices[gl_VertexID];
	Particle p = particles[v.particleid];

	vec4 worldSpacePos = vec4(p.pos, 1.0f);
	vec4 viewSpacePos = view * worldSpacePos;

	outUV = uvs[v.vertexid];
	sheetRes = p.sheetRes;
	age = p.age;
	texNum = p.texNum;

	if (length(p.dir) < 0.01) {
		gl_Position = projection * (viewSpacePos + vec4(vertex_offsets[v.vertexid], 0.0f) * p.size);
	} else {
		vec3 viewSpaceDir = (view * vec4(p.dir, 0.0)).xyz;
		vec3 offset = -normalize(cross(viewSpaceDir, viewSpacePos.xyz)) * 0.5;
		gl_Position = projection * (viewSpacePos + vec4(offset * cylindricalScaleX[v.vertexid] + viewSpaceDir * cylindricalScaleY[v.vertexid], 0.0f) * p.size);
	}
}