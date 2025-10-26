#version 460 core

struct Vertex {
    vec3 position;
	vec3 normal;
	vec3 tangent;
    vec2 uv;
};

layout(binding = 0, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout(location = 0) uniform mat4 modelMatrix;
layout(location = 4) uniform mat4 camMatrix;

void main() {
	Vertex v = vertices[gl_VertexID];

	vec4 worldSpacePos = modelMatrix * vec4(v.position, 1.0f);

	gl_Position = camMatrix * worldSpacePos;
}