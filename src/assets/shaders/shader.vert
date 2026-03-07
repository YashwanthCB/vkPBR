#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec3 normal;

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

void main() {
    gl_Position = ubo.projection * ubo.view * pushed.model * vec4(position, 1.0);

    worldPosition = vec3(pushed.model * vec4(position, 1.0));
    normal = inNormal;
}