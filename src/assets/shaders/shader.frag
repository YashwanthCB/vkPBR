#version 450

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBlock
{
	vec3 cameraPosition;
	float ao;
	mat4 view;
	mat4 projection;

	vec4 lightPositions[4];
	vec4 lightColors[4];
} ubo;

layout(push_constant) uniform PushConstants
{
	mat4 model;
	vec4 albedo;

	float metallic;
	float roughness;
} pushed;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

void main() {
	
	vec3 N = normalize(normal);
	vec3 V = normalize(ubo.cameraPosition - worldPosition);
	
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, pushed.albedo.rgb, pushed.metallic);
	vec3 Lo = vec3(0.0);
	
	for(int i = 0; i < 2; i++)
	{
		vec3 L = normalize(ubo.lightPositions[i].xyz);
		//for point light, use: normalize(ubo.lightPositions[i].xyz - worldPosition);
		vec3 H = normalize(V + L);

		float distance    = length(ubo.lightPositions[i].xyz - worldPosition);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance     = ubo.lightColors[i].rgb * ubo.lightPositions[i].w;
		//for point light, use: ubo.lightColors[i].rgb * attenuation;

		float NDF = DistributionGGX(N, H, pushed.roughness);
		float G   = GeometrySmith(N, V, L, pushed.roughness);
		vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= 1.0 - pushed.metallic;

		vec3 numerator    = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
		vec3 specular     = numerator / denominator;

		float NdotL = max(dot(N, L), 0.0);
		Lo += (kD * pushed.albedo.rgb / PI + specular) * radiance * NdotL;
	}

	vec3 ambient = vec3(0.003) * pushed.albedo.rgb * ubo.ao;
	vec3 color = ambient + Lo;

	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0/2.2));

	outColor = vec4(color, 1.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a      = roughness*roughness;
	float a2     = a*a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;

	float num   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;

	float num   = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}