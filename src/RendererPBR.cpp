#include "RendererPBR.h"

#include "Util.h"

#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <unordered_set>
#include <fstream>
#include <cassert>


RendererPBR::RendererPBR(GLFWwindow* window, const Scene& scene) : window(window), scene(scene)
{
    InitAssimp();
    InstanceInfo();
    CreateInstance();
    CreateSurface();
    SelectPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    
    for (auto& entity : scene.entities)
    {
        std::vector<VertexData> vertexData = LoadModel((std::string(MODELS_PATH) + entity.first));
        VertexBufferUnit bufferUnit;
        PushConstants pushConstants;
        CreateVertexBuffer(vertexData, bufferUnit);
        // having different structs is better to cope up with padding issues
        pushConstants.model = entity.second.model;
        pushConstants.albedo = entity.second.albedo;
        pushConstants.metallic = entity.second.metallic;
        pushConstants.roughness = entity.second.roughness;        
        bufferUnits.emplace_back(bufferUnit, pushConstants);
    }
    
    CreateDescriptorSetLayout();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateGraphicsPipeline();
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSyncObjects();
    CreateShaderResources();
}

RendererPBR::~RendererPBR()
{
    if (instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance, nullptr);
    }
}

void RendererPBR::Draw()
{
    vkWaitForFences(device, 1, &inFlightFences[currentFrameInFlight], VK_TRUE, UINT64_MAX);
    
    uint32_t swapchainImageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrameInFlight], VK_NULL_HANDLE, &swapchainImageIndex);
    
    UpdateShaderResources();
    
    vkResetFences(device, 1, &inFlightFences[currentFrameInFlight]);
    
    vkResetCommandBuffer(commandBuffers[currentFrameInFlight], 0);
    RecordCommandBuffer(commandBuffers[currentFrameInFlight], swapchainImageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrameInFlight]};
    submitInfo.pWaitSemaphores = waitSemaphores;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[swapchainImageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrameInFlight];
    
    
    assert(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrameInFlight]) == VK_SUCCESS);
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapchains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &swapchainImageIndex;
    
    vkQueuePresentKHR(presentQueue, &presentInfo);
    currentFrameInFlight = (currentFrameInFlight + 1) % MAX_FRAMES_IN_FLIGHT;
}


void RendererPBR::CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Minimal Vulkan Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

#if _DEBUG
    const char* validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
    instanceInfo.enabledLayerCount = 1;
    instanceInfo.ppEnabledLayerNames = validationLayers;
#else
    instanceInfo.enabledLayerCount = 0;
    instanceInfo.ppEnabledLayerNames = nullptr;
#endif

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    instanceInfo.enabledExtensionCount = glfwExtensionCount;
    instanceInfo.ppEnabledExtensionNames = glfwExtensions;

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
    assert(result == VK_SUCCESS && instance != VK_NULL_HANDLE);
}

void RendererPBR::InstanceInfo()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr);
    VkExtensionProperties* outProperties = new VkExtensionProperties[extensionCount];
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, outProperties);
    
    std::cout << "instance extensions: " << std::endl;
    for (int index = 0; index < extensionCount; index++)
    {
        std::cout << outProperties[index].extensionName << std::endl;
    }
    delete[] outProperties;
    
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    VkLayerProperties* layerProperties = new VkLayerProperties[layerCount];
    vkEnumerateInstanceLayerProperties(&layerCount, layerProperties);    
    
    std::cout << "instance layers: ";
    for (int i = 0; i < layerCount; i++)
    {
        std::cout << layerProperties[i].layerName << std::endl;
    }
    delete[] layerProperties;
}

void RendererPBR::PhysicalDeviceInfo(VkPhysicalDevice& device)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    VkExtensionProperties* outProperties = new VkExtensionProperties[extensionCount];
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, outProperties);
    
    std::cout << "physical device extensions: " << std::endl;
    for (int i = 0; i < extensionCount; i++)
    {
        std::cout << outProperties[i].extensionName << std::endl;
    }
    delete[] outProperties;
    
    uint32_t layerCount = 0;
    vkEnumerateDeviceLayerProperties(device, &layerCount, nullptr);
    VkLayerProperties* layerProperties = new VkLayerProperties[layerCount];
    vkEnumerateDeviceLayerProperties(device, &layerCount, layerProperties);    
    
    std::cout << "physical device layers: " << std::endl;
    for (int i = 0; i < layerCount; i++)
    {
        std::cout << layerProperties[i].layerName << std::endl;
    }
    delete[] layerProperties;
}

void RendererPBR::SelectPhysicalDevice()
{
    uint32_t devicesCount = 0;
    vkEnumeratePhysicalDevices(this->instance, &devicesCount, nullptr);
    VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[devicesCount];
    vkEnumeratePhysicalDevices(this->instance, &devicesCount, physicalDevices);
    
    bool SupportedDeviceFound = false;
    
    for (int deviceIndex = 0; deviceIndex < devicesCount; deviceIndex++)
    {
        std::cout << physicalDevices[deviceIndex] << std::endl;
        PhysicalDeviceInfo(physicalDevices[deviceIndex]);
        
        if (IsDeviceSuitable(physicalDevices[deviceIndex]))
        {
            std::cout << "Suitable Device: " << physicalDevices[deviceIndex] << std::endl;
            this->physicalDevice = physicalDevices[deviceIndex];
            SupportedDeviceFound = true;
            break;
        }
    }
    assert(SupportedDeviceFound);
    delete[] physicalDevices;
}

void RendererPBR::CreateLogicalDevice()
{
    std::optional<uint32_t> graphicsQueueFamilyIndex;
    std::optional<uint32_t> presentQueueFamilyIndex;
    float queuePriority = 1.0f;
    GetOperationQueueFamilyIndex(this->physicalDevice, graphicsQueueFamilyIndex, presentQueueFamilyIndex);
    
    assert(graphicsQueueFamilyIndex.has_value() && presentQueueFamilyIndex.has_value());
    
    std::unordered_set<uint32_t> uniqueQueueFamilyIndices = {graphicsQueueFamilyIndex.value(), presentQueueFamilyIndex.value()};
    
    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
    for (auto& uniqueQueueFamilyIndex : uniqueQueueFamilyIndices)
    {
        VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
        deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfo.queueFamilyIndex = uniqueQueueFamilyIndex;
        deviceQueueCreateInfo.queueCount = 1;
        deviceQueueCreateInfo.pQueuePriorities = &queuePriority;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }
    
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
    std::vector<const char*> extensionNames = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.enabledExtensionCount = extensionNames.size();
    deviceCreateInfo.ppEnabledExtensionNames = extensionNames.data();
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = deviceQueueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.pNext = &dynamicRenderingFeatures;
    
    assert(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) == VK_SUCCESS);
    
    vkGetDeviceQueue(device, graphicsQueueFamilyIndex.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamilyIndex.value(), 0, &presentQueue);
}

void RendererPBR::CreateSurface()
{
    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    assert(result == VK_SUCCESS);
}

void RendererPBR::CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupportDetails(physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = ChooseSwapChainFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapChainPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);
    
    swapchainImageCount= swapChainSupport.capabilities.minImageCount + 1;
    
    if (swapChainSupport.capabilities.maxImageCount > 0 && swapchainImageCount > swapChainSupport.capabilities.maxImageCount)
    {
        swapchainImageCount = swapChainSupport.capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.minImageCount = swapchainImageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    std::optional<uint32_t> graphicsIndex;
    std::optional<uint32_t> presentIndex;
    GetOperationQueueFamilyIndex(physicalDevice, graphicsIndex, presentIndex);
    if (graphicsIndex != presentIndex)
    {
        uint32_t familyIndices[] = {graphicsIndex.value(), presentIndex.value()};
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = familyIndices;
    }
    else
    {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    
    assert(vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapchain) == VK_SUCCESS);
    
    uint32_t retrivalImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &retrivalImageCount, nullptr);
    swapchainImages.resize(retrivalImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &retrivalImageCount, swapchainImages.data());
    
    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
    
}

void RendererPBR::CreateImageViews()
{
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImageViews.size(); ++i)
    {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = swapchainImageFormat;
        
        imageViewCreateInfo.image = swapchainImages[i];
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i]);
    }
}

void RendererPBR::CreateVertexBuffer(std::vector<VertexData>& vertexData, VertexBufferUnit& vertexBufferUnit)
{
    VkDeviceSize vertexBufferSize = sizeof(vertexData[0]) * vertexData.size();
    vertexBufferUnit.deviceSize = vertexBufferSize;
    CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferUnit.vertexBuffer, vertexBufferUnit.vertexBufferMemory);
    
    void* deviceMemoryPointer;
    vkMapMemory(device, vertexBufferUnit.vertexBufferMemory, 0, vertexBufferSize, 0, &deviceMemoryPointer);
    memcpy(deviceMemoryPointer, vertexData.data(), vertexBufferSize);
    vkUnmapMemory(device, vertexBufferUnit.vertexBufferMemory);
}

void RendererPBR::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    assert(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer) == VK_SUCCESS);
    
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
    
    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);
    
    assert(vkAllocateMemory(device, &allocateInfo, nullptr, &bufferMemory) == VK_SUCCESS);
    assert(vkBindBufferMemory(device, buffer, bufferMemory, 0) == VK_SUCCESS);
}

void RendererPBR::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &uboLayoutBinding;
    
    assert(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) == VK_SUCCESS);
    
}

void RendererPBR::CreateUniformBuffers()
{
    VkDeviceSize uniformBufferSize = sizeof(UniformBlock);
    
    for (uint32_t i = 0; i < uniformBuffers.size(); i++)
    {
        CreateBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
        vkMapMemory(device, uniformBuffersMemory[i], 0, uniformBufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void RendererPBR::CreateDescriptorPool()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.poolSizeCount = 1;
    poolCreateInfo.pPoolSizes = &poolSize;
    poolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    
    vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool);
}

void RendererPBR::CreateDescriptorSets()
{
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> descriptorSetLayouts; 
    descriptorSetLayouts.fill(descriptorSetLayout);
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = descriptorSetLayouts.size();
    descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();
    
    assert(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, descriptorSets.data()) == VK_SUCCESS);
    
    for (uint32_t index = 0; index < descriptorSets.size(); index++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[index];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBlock);
        
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[index];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
    
}

void RendererPBR::CreateGraphicsPipeline()
{
    auto vertexShaderCode = ReadFile("assets/shaders/vert.spv");
    auto fragmentShaderCode = ReadFile("assets/shaders/frag.spv");
    
    VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderCode);
    
    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertexShaderModule;
    vertexShaderStageCreateInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = fragmentShaderModule;
    fragmentShaderStageCreateInfo.pName = "main";        
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo};
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    VkPipelineViewportStateCreateInfo viewportState{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineMultisampleStateCreateInfo multisample{};
    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    
    auto bindingDesc = VertexData::GetBindingDescription();
    auto attribDesc =  VertexData::GetVertexAttributeDescription();
    
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.pVertexAttributeDescriptions = attribDesc.data();
    
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchainExtent.width;
    viewport.height = (float) swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;
    
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttachment;
    
    VkPipelineRenderingCreateInfo renderingCreateInfo{};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = 1;
    renderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;
    
    VkPushConstantRange pipelineLayoutPushConstantRange{};
    pipelineLayoutPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineLayoutPushConstantRange.offset = 0;
    pipelineLayoutPushConstantRange.size = sizeof(PushConstants);
    
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pipelineLayoutPushConstantRange;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    
    assert(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) == VK_SUCCESS);
            
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = &renderingCreateInfo;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisample;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.layout = pipelineLayout;
            
    assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) == VK_SUCCESS);
    
    vkDestroyShaderModule(device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
}

void RendererPBR::CreateCommandPool()
{
    std::optional<uint32_t> graphicsQueueFamilyIndex;
    std::optional<uint32_t> presentQueueFamilyIndex;        
    GetOperationQueueFamilyIndex(physicalDevice, graphicsQueueFamilyIndex, presentQueueFamilyIndex);
    
    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = graphicsQueueFamilyIndex.value();
    
    assert(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool) == VK_SUCCESS);
}

void RendererPBR::CreateCommandBuffers()
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    commandBufferAllocateInfo.commandPool = commandPool;
    
    assert(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data()) == VK_SUCCESS);
}

void RendererPBR::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    renderFinishedSemaphores.resize(swapchainImageCount);

    for (int i = 0 ; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[i]) == VK_SUCCESS);
        assert(vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFences[i]) == VK_SUCCESS);
    }
    for (int i = 0; i < swapchainImageCount; i++)
    {
        assert(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) == VK_SUCCESS);
    }
}

void RendererPBR::CreateShaderResources()
{
    uBlock.cameraPosition = scene.cameraPosition;
    uBlock.lightPositions[0] = glm::vec4{-100.0f, -60.0f, 20.0f, 1.0f};
    uBlock.lightPositions[1] = glm::vec4{100.0f, 60.0f, 20.0f, 0.001f};
    uBlock.lightPositions[2] = glm::vec4{3.0f, -1.0f, 2.0f, 1.0f};
    uBlock.lightPositions[3] = glm::vec4{-3.0f, 1.0f, 2.0f, 1.0f};
    
    uBlock.lightColors[0] = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
    uBlock.lightColors[1] = glm::vec4{0.5f, 0.05f, 0.05f, 1.0f};
    uBlock.lightColors[2] = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
    uBlock.lightColors[3] = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
    
    float aspectRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
    uBlock.view = glm::lookAt(scene.cameraPosition, glm::vec3{0.0f, 0.0f, 0.0f}, 
       glm::vec3(0.0f, 1.0f, 0.0f));
    uBlock.projection = glm::perspective(glm::radians(35.0f), aspectRatio, 
        0.01f, 100.0f);
    
    uBlock.ao = 0.5f;
}

void RendererPBR::UpdateShaderResources()
{        
    uBlock.lightPositions[0] = glm::vec4{glm::sin(glfwGetTime()) * -100.0f, -60.0f, 20.0f, 1.0f};
    uBlock.lightPositions[1] = glm::vec4{glm::sin(glfwGetTime()) * 100.0f, 60.0f, 20.0f, 0.04f};
    memcpy(uniformBuffersMapped[currentFrameInFlight], &uBlock, sizeof(uBlock));
}

void RendererPBR::RecordCommandBuffer(const VkCommandBuffer& commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier.image = swapchainImages[imageIndex];
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    
    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = swapchainImageViews[imageIndex];
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    VkClearValue clearColor{};
    clearColor.color ={} ;
    colorAttachmentInfo.clearValue = clearColor;
    
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = swapchainExtent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    
    vkCmdBeginRendering(commandBuffer, &renderingInfo);        
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
      
    VkViewport viewport{};
    viewport.width = swapchainExtent.width;
    viewport.height = swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    
    VkDeviceSize offsets[] = {0};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 
        0, 1, &descriptorSets[currentFrameInFlight], 0, nullptr);
    
    for (auto& bufferUnitPair : bufferUnits)
    {
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &bufferUnitPair.first.vertexBuffer, offsets);
        vkCmdPushConstants(commandBuffer, pipelineLayout, 
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 
            sizeof(PushConstants), &bufferUnitPair.second);
        vkCmdDraw(commandBuffer, bufferUnitPair.first.deviceSize, 1, 0, 0);
    }
    
    
    
    vkCmdEndRendering(commandBuffer);
    
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
    imageMemoryBarrier.image = swapchainImages[imageIndex];
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    
    vkEndCommandBuffer(commandBuffer);
}

VkShaderModule RendererPBR::CreateShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = code.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);
    
    return shaderModule;
}

uint32_t RendererPBR::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        bool IsPhysicalMemCompatible = (typeFilter & (1 << i));
        bool DoesPropertiesMatch = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (IsPhysicalMemCompatible && DoesPropertiesMatch)
        {
            return i;
        }
    }
    assert(false);
}

void RendererPBR::GetOperationQueueFamilyIndex(const VkPhysicalDevice& physicalDevice,
                                                 std::optional<uint32_t>& graphicsFamilyIndex, std::optional<uint32_t>& presentFamilyIndex) const
{
    uint32_t propertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &propertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(propertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &propertyCount, queueFamilyProperties.data());

    int familyIndex = 0;
    for (auto& queueFamilyProperty : queueFamilyProperties)
    {
        if (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphicsFamilyIndex = familyIndex;
        }
        
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, this->surface, &presentSupport);
        
        if (presentSupport)
        {
            presentFamilyIndex = familyIndex;
        }
        
        familyIndex++;
    }
}

bool RendererPBR::IsDeviceSuitable(const VkPhysicalDevice& physicalDevice)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    
    std::optional<uint32_t> graphicsFamilyIndex;
    std::optional<uint32_t> presentFamilyIndex;
    GetOperationQueueFamilyIndex(physicalDevice, graphicsFamilyIndex, presentFamilyIndex);
    
    bool supportsSwapChain = CheckExtensionSupport(physicalDevice, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    
    if (supportsSwapChain)
    {
        SwapChainSupportDetails details = QuerySwapChainSupportDetails(physicalDevice);
        supportsSwapChain = !details.formats.empty() && !details.presentModes.empty();
    }
    
    bool supportsDynamicRendering = CheckExtensionSupport(physicalDevice, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    
    return (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || 
        deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) && 
            (graphicsFamilyIndex.has_value() && presentFamilyIndex.has_value()
                && supportsSwapChain && supportsDynamicRendering);
}

bool RendererPBR::CheckExtensionSupport(const VkPhysicalDevice& physicalDevice, const char* extensionName)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensionProperties.data());
    
    for (auto& extensionProperty : extensionProperties)
    {
        if(std::strcmp(extensionProperty.extensionName, extensionName) == 0)
        {
            return true;
        }
    }
    return false;
}


SwapChainSupportDetails RendererPBR::QuerySwapChainSupportDetails(const VkPhysicalDevice& physicalDevice) const
{
    SwapChainSupportDetails swapChainSupportDetails{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &swapChainSupportDetails.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        swapChainSupportDetails.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, swapChainSupportDetails.formats.data());                
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        swapChainSupportDetails.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, swapChainSupportDetails.presentModes.data());
    }
    return swapChainSupportDetails;
}

VkSurfaceFormatKHR RendererPBR::ChooseSwapChainFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (auto& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
    return formats[0];
}

VkPresentModeKHR RendererPBR::ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
    for (auto& presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D RendererPBR::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

float mapValues(float value, float a, float b, float x, float y)
{
    if (a == b) return x;
    float t = (value - a) / (b - a);
    return glm::mix(x, y, t);
}