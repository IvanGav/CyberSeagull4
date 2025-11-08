#version 460 core

layout(location = 0) in vec2 inUV;
layout(location = 1) flat in uint sheetRes;
layout(location = 2) in float age;

layout(location = 0) out vec4 fCol;

layout(binding = 0) uniform sampler2D particleTex;

void main() {
	uint resX = sheetRes & 0xFFu;
	uint resY = sheetRes >> 8u;
	uint idx = clamp(uint(age * float(resX * resY)), 0u, resX * resY);
	vec2 uv = inUV / vec2(float(resX), float(resY));
	uv += vec2(float(idx % resX) / float(resX), -float(idx / resX) / float(resY));
	fCol = texture(particleTex, uv);
}