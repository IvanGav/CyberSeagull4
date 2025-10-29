#version 460 core

layout(location = 0) in vec3 inPos;

layout(binding = 2) uniform samplerCube skyboxTex;

out vec4 fColor;

void main() {
	fColor = texture(skyboxTex, inPos);
}