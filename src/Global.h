#pragma once

#include "vulkan/vulkan.h"
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/random.hpp"

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define MAX_FRAMES_IN_FLIGHT 2

#define MODELS_PATH "assets/models/"

extern inline float mapValues(float value, float a, float b, float x, float y);