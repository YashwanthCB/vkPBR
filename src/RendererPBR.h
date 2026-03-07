#pragma once

#include "Global.h"
#include "RendererTypes.h"
#include "Scene.h"
#include <optional>
#include <vector>
#include <array>

class RendererPBR
{
public:
    RendererPBR(struct GLFWwindow* window, const Scene& scene);
    ~RendererPBR();
    void Draw();
    
private:
    void CreateInstance();
    void InstanceInfo();
    void PhysicalDeviceInfo(VkPhysicalDevice& device);
    void SelectPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSurface();
    void CreateSwapChain();
    void CreateImageViews();
    void CreateVertexBuffer(std::vector<VertexData>& vertexData, VertexBufferUnit& vertexBufferUnit);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CreateDescriptorSetLayout();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateGraphicsPipeline();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CreateShaderResources();
    void UpdateShaderResources();
    
    void RecordCommandBuffer(const VkCommandBuffer& commandBuffer, uint32_t imageIndex);
    
    void GetOperationQueueFamilyIndex(const VkPhysicalDevice& physicalDevice, std::optional<uint32_t>& graphicsFamilyIndex, std::optional<uint32_t>& presentFamilyIndex) const;
    bool IsDeviceSuitable(const VkPhysicalDevice& physicalDevice);
    bool CheckExtensionSupport(const VkPhysicalDevice& physicalDevice, const char* extensionName);
    SwapChainSupportDetails QuerySwapChainSupportDetails(const VkPhysicalDevice& physicalDevice) const;
    VkSurfaceFormatKHR ChooseSwapChainFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImageView> swapchainImageViews;
    VkDescriptorPool descriptorPool;
    
    std::vector<std::pair<VertexBufferUnit, PushConstants>> bufferUnits;
    VkDescriptorSetLayout descriptorSetLayout;    
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uniformBuffersMemory;
    std::array<void*, MAX_FRAMES_IN_FLIGHT> uniformBuffersMapped;        
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
    
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkCommandPool commandPool;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences;
    
    uint32_t swapchainImageCount;
    uint32_t currentFrameInFlight = 0;
    GLFWwindow* window;
    
    const Scene& scene;
    UniformBlock uBlock{};
};
