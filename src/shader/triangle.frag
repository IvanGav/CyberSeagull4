#version 460 core

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in vec4 inLightspacePos;

layout(location = 0) out vec4 fCol;

layout(binding = 0) uniform sampler2D baseColorTex;
layout(binding = 1) uniform sampler2DShadow shadowMap;
layout(binding = 2) uniform sampler2D normalMap;

layout(location = 15) uniform vec3 cameraPos;
layout(location = 16) uniform vec3 lightAngle;
layout(location = 17) uniform vec3 lightColor;
layout(location = 18) uniform vec2 shadowMapResolution;

float inShadow(vec4 lightSpacePos) {
	vec3 projectedPos = lightSpacePos.xyz/lightSpacePos.w; // is done automatically for gl_Position, but need to do manually here
	projectedPos.xy = projectedPos.xy * 0.5f + 0.5f; // to sample the texture, it's (0,1), but drawing box is (-1,1)

	vec2 texel_size = 1.0f/shadowMapResolution;

	float accumulated = 0.0f;

	accumulated += texture(shadowMap, projectedPos.xyz + vec3(vec2(0,0) * texel_size, -0.005f)).r;
	accumulated += texture(shadowMap, projectedPos.xyz + vec3(vec2(1.0f,1.0f) * texel_size, -0.005f)).r;
	accumulated += texture(shadowMap, projectedPos.xyz + vec3(vec2(-1.0f,1.0f) * texel_size, -0.005f)).r;
	accumulated += texture(shadowMap, projectedPos.xyz + vec3(vec2(1.0f,-1.0f) * texel_size, -0.005f)).r;
	accumulated += texture(shadowMap, projectedPos.xyz + vec3(vec2(-1.0f,-1.0f) * texel_size, -0.005f)).r;

	return accumulated/5.0f;
}

// Phong
vec4 directionalLightAlt(vec4 baseColor, vec3 toLightDir, vec3 lightColor, vec3 cameraPos, vec3 normal) {
	vec4 diffuse = baseColor * max(dot(normal, toLightDir), 0.0f) * vec4(lightColor, 1.0f);
	vec4 ambient = 0.3f * baseColor;
	
    // Specular (Phong)
	const float specularIntensity = 0.5f;
    vec3 toCameraDir = normalize(cameraPos - inPos);
    vec3 reflectedLightDir = reflect(-toLightDir, normal);
    float specularBase = clamp(dot(reflectedLightDir, toCameraDir), 0.0, 1.0);
    float specular = pow(specularBase, 32.0f) * specularIntensity;

	return (diffuse + specular) * inShadow(inLightspacePos) + ambient;
}

// Blinn-Phong
vec4 directionalLight(vec4 baseColor, vec3 toLightDir, vec3 lightColor, vec3 cameraPos, vec3 normal) {
	vec4 diffuse = baseColor * max(dot(normal, toLightDir), 0.0f) * vec4(lightColor, 1.0f);
	vec4 ambient = 0.3f * baseColor;

	const float specularIntensity = 0.5f;
	
	// Specular (Blinn-Phong)
    vec3 toCameraDir = normalize(cameraPos - inPos);
    vec3 halfVector = normalize(toCameraDir + toLightDir);
    float specularBase = clamp(dot(halfVector, normal), 0.0, 1.0);
    float specular = pow(specularBase, 64.0f) * specularIntensity;

	return (diffuse + specular) * inShadow(inLightspacePos) + ambient;
}

void main() {
	vec3 normal = normalize(inNormal);

	// use normal map
	vec3 tangent = normalize(inTangent);
	vec3 bitangent = normalize(inBitangent);
	mat3 TBN = mat3(tangent, bitangent, normal);
	normal = normalize(TBN * (texture(normalMap, inUV).rgb * 2.0f - 1.0f));
	//fCol = vec4(texture(normalMap, inUV).rgb * 2.0f - 1.0f, 1.0);

	fCol = vec4(vec3(directionalLight(texture(baseColorTex, inUV), normalize(-lightAngle), lightColor, cameraPos, normal)), 1.0);
}