#version 460 core

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in vec4 inLightspacePos;

layout(location = 0) out vec4 fCol;

layout(binding = 0) uniform sampler2D baseColorTex;
layout(binding = 1) uniform sampler2D normalMap;
layout(binding = 2) uniform sampler2D shadowMap;

layout(location = 15) uniform vec3 cameraPos;
layout(location = 16) uniform vec3 lightAngle;
layout(location = 17) uniform vec3 lightColor;

bool inShadow(vec4 lightSpacePos) {
	return false; // TODO temporary
	vec3 projectedPos = lightSpacePos.xyz/lightSpacePos.w; // is done automatically for gl_Position, but need manually here
	projectedPos.xy = projectedPos.xy * 0.5f + 0.5f; // to sample the texture, it's (0,1), but drawing box is (-1,1)
	float depth = texture(shadowMap, projectedPos.xy).r; // r from rgba

	return lightSpacePos.z > depth + 0.005f;
}

// Phong
vec4 directionalLightAlt(vec4 baseColor, vec3 toLightDir, vec3 lightColor, vec3 cameraPos, vec3 normal) {
	vec4 diffuse = baseColor * max(dot(normal, -toLightDir), 0.0f) * vec4(lightColor, 1.0f);
	vec4 ambient = 0.1f * baseColor;
	
    // Specular (Phong)
	const float specularIntensity = 0.5f;
    vec3 toCameraDir = normalize(cameraPos - inPos);
    vec3 reflectedLightDir = reflect(-toLightDir, normal);
    float specularBase = clamp(dot(reflectedLightDir, toCameraDir), 0.0, 1.0);
    float specular = pow(specularBase, 32.0f) * specularIntensity;

	if(inShadow(inLightspacePos)) {
		return ambient;
	} else {
		return diffuse + specular + ambient;
	}
}

// Blinn-Phong (BROKEN)
vec4 directionalLight(vec4 baseColor, vec3 toLightDir, vec3 lightColor, vec3 cameraPos, vec3 normal) {
	vec4 diffuse = baseColor * max(dot(normal, -toLightDir), 0.0f) * vec4(lightColor, 1.0f);
	vec4 ambient = 0.1f * baseColor;

	const float specularIntensity = 0.5f;
	
	// Specular (Blinn-Phong)
    vec3 toCameraDir = normalize(cameraPos - inPos);
    vec3 halfVector = normalize(toCameraDir - toLightDir);
    float specularBase = clamp(dot(halfVector, normal), 0.0, 1.0);
    float specular = pow(specularBase, 32.0f) * specularIntensity;

	if(inShadow(inLightspacePos)) {
		return ambient;
	} else {
		return diffuse + specular + ambient;
	}
}

void main() {
	vec3 normal = normalize(inNormal);

	/*
	// use normal map
	vec3 tangent = normalize(inTangent);
	vec3 bitangent = normalize(inBitangent);
	mat3 TBN = mat3(tangent, bitangent, normal);
	normal = normalize(TBN * (texture(normalMap, inUV).rgb * 2.0f - 1.0f));
	*/

	fCol = directionalLightAlt(texture(baseColorTex, inUV), normalize(-lightAngle), lightColor, cameraPos, normal);
}