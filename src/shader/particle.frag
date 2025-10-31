#version 460 core

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fCol;

layout(binding = 0) uniform sampler2D particleTex;

void main() {

	fCol = texture(baseColorTex, inUV);
}