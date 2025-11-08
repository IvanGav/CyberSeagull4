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

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec3 outBitangent;
layout(location = 5) out vec4 outLightspacePos;

layout(location = 0) uniform mat4 modelMatrix;
layout(location = 4) uniform mat4 camMatrix;
layout(location = 8) uniform mat3 normalTransform;
layout(location = 11) uniform mat4 lightTransform;
layout(location = 19) uniform bool doClip;
layout(location = 20) uniform float clipY;

void main() {
	Vertex v = vertices[gl_VertexID];

	vec4 worldSpacePos = modelMatrix * vec4(v.position, 1.0f);

	outPos = worldSpacePos.xyz;
	outUV = v.uv;
	outNormal = normalTransform * v.normal;
	outTangent = normalTransform * v.tangent;
	outBitangent = cross(outNormal, outTangent);
	outLightspacePos = lightTransform * worldSpacePos;
	
	gl_Position = camMatrix * worldSpacePos;
	if(doClip)
		gl_ClipDistance[0] = worldSpacePos.y - clipY;
}