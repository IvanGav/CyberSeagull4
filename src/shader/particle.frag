#version 460 core

layout(location = 0) in vec2 inUV;
layout(location = 1) flat in uint sheetRes;
layout(location = 2) in float age;
layout(location = 3) flat in uint texNum;

layout(location = 0) out vec4 fCol;

layout(binding = 0) uniform sampler2D particleTex0;
layout(binding = 1) uniform sampler2D particleTex1;
layout(binding = 2) uniform sampler2D particleTex2;
layout(binding = 3) uniform sampler2D particleTex3;

void main() {
	uint resX = sheetRes & 0xFFu;
	uint resY = sheetRes >> 8u;
	uint idx = clamp(uint(age * float(resX * resY)), 0u, resX * resY);
	vec2 uv = inUV / vec2(float(resX), float(resY));
	uv += vec2(float(idx % resX) / float(resX), -float(idx / resX) / float(resY));
	if(texNum == 0)
		fCol = texture(particleTex0, uv);
	else if(texNum == 1)
		fCol = texture(particleTex1, uv);
	else if(texNum == 2)
		fCol = texture(particleTex2, uv);
	else if(texNum == 3)
		fCol = texture(particleTex3, uv);
	else
		fCol = vec4(1.0,0.0,1.0,1.0);
}