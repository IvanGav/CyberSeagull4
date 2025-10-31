#version 460 core

struct Vertex {
	int particleid;
	int vertexid; // from 0 to 5 - which vertex in this particle quad
};

struct Particle {
    vec3 position;
    vec4 color;
};

layout(binding = 0, std430) readonly buffer VertexBuffer {
	Vertex vertices[];
};

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
}