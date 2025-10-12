#ifndef _VK_FUNCTION_H_
#define _VK_FUNCTION_H_

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <exception>
#include <array>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <numeric>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <thread>
// #include <functional>
#include <algorithm>

#include "vulkan/vulkan.h"
#include "vulkan/vk_enum_string_helper.h"

#define VK_API_VERSION VK_API_VERSION_1_3

// Helper class, store a lambda function and execute it when go out of scope.

template<class Func>
class OnScopeExit
{
public:
	OnScopeExit(Func&& func) : m_func(func) {}
	~OnScopeExit() { m_func(); }
private:
	Func m_func;
};


// Check vulkan errors.

bool VKCheckError(VkResult result, bool throwException);
#define VKCall(code) (VKCheckError(code, true))
#define VKSucceed(code) (VKCheckError(code, false))


// Create vulkan instance.

bool VKCreateInstance(VkInstance& vkInst, size_t instExtNum, const char** instExts, size_t enableLayerNum, const char** enableLayers, VkAllocationCallbacks* allocator = nullptr);

void VKSetDebugObjectName(VkDevice vkDevice, VkObjectType vkObjType, uint64_t vkObjHandle, const char* name);


// Pick physcical device.

bool VKPickPhysicalDeviceAndOneQueueFamily(VkInstance vkInst, VkSurfaceKHR surf, VkPhysicalDevice* vkPhysicalDevice, uint32_t* vkQueueFamilyIdx);

void VKPrintSurfaceCapabilities(const char* surfName, VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf);

void VKPrintPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice vkPhysicalDevice);

void VKPrintPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures& vkPhysicalDeviceFeatures);

bool VKPickSurfaceFormat(VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR& surfFmt);


// Create a logical device.

struct VKDeviceQueueCreateRequest
{
	uint32_t QueueFamilyIdx;
	uint32_t QueueCount;
	std::vector<float> QueuePriorities;
};
bool VKCreateDevice(
	VkDevice& vkDevice, VkPhysicalDevice vkPhysicalDevice,
	size_t queueCreateRequestCount, const VKDeviceQueueCreateRequest* queueCreateRequests,
	size_t enableLayerCount, const char** enableLayerNames,
	size_t enableExtensionCount, const char** enableExtensionNames,
	const void* featureLinkedList = nullptr, VkAllocationCallbacks* vkAllocator = nullptr);

// Shader & pipeline.

extern const VkPipelineVertexInputStateCreateInfo g_VertexInputStateCreateInfo_NoVertexInput;
extern const VkPipelineInputAssemblyStateCreateInfo g_InputAssemblyStateCreateInfo_TriangleList;
extern const VkPipelineViewportStateCreateInfo g_ViewportStateCreateInfo_DynamicViewportScissor;
extern const VkPipelineRasterizationStateCreateInfo g_RasterizationStateCreateInfo_Default;
extern const VkPipelineMultisampleStateCreateInfo g_MultisamplingStateCreateInfo_DisableMultisample;
extern const VkPipelineColorBlendStateCreateInfo g_ColorBlendStateCreateInfo_RGB_DisableBlend;
extern const VkPipelineDynamicStateCreateInfo g_DynamicStateCreateInfo_DynamicViewportScissor;

bool VKCompileShader(const std::string& shaderFilepath, const std::string& options, const std::string& outputFilepath);

bool VKCreateShaderModuleFromSPV(VkDevice vkDevice, const std::string& filepath, VkShaderModule& shaderModule);

#endif