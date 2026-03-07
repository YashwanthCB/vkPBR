#pragma once
#include "Global.h"

#include <vector>
#include <array>

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};


struct VertexData
{
    glm::vec3 position;
    glm::vec3 normal;
    
    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(VertexData);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        return bindingDescription;
    }
    
    static std::array<VkVertexInputAttributeDescription, 2> GetVertexAttributeDescription()
    {
        VkVertexInputAttributeDescription vertexAttributeDescription = {};
        
        vertexAttributeDescription.binding = 0;
        vertexAttributeDescription.location = 0;
        vertexAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributeDescription.offset = offsetof(VertexData, position);
        
        VkVertexInputAttributeDescription colorAttributeDescription = {};
        colorAttributeDescription.binding = 0;
        colorAttributeDescription.location = 1;
        colorAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
        colorAttributeDescription.offset = offsetof(VertexData, normal);
        
        return {vertexAttributeDescription, colorAttributeDescription};
    }
    
};

static struct VertexBufferUnit
{
    VkDeviceSize deviceSize;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
};

struct UniformBlock
{
    glm::vec3 cameraPosition;
    float ao;
    glm::mat4 view;
    glm::mat4 projection;
    
    glm::vec4 lightPositions[4];
    glm::vec4 lightColors[4];
};

struct PushConstants
{
    glm::mat4 model;
    glm::vec4 albedo;
    
    float metallic;
    float roughness;
};