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
#include <span>
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

#define RuntimeCheckError(code) do { if (!(code)) { fprintf(stderr, "[%s:%u] Runtime check failed: %s.\n", __FILE__, __LINE__, #code); throw std::runtime_error("Runtime check failed: " #code); } } while(0)

bool VKCheckError(VkResult result, bool throwException);
#define VKCall(code) (VKCheckError(code, true))
#define VKSucceed(code) (VKCheckError(code, false))


// VkInstance helper function.

bool VKCreateInstance(std::span<const char*> instExts, std::span<const char*> enableLayers, VkAllocationCallbacks* allocator, VkInstance* vkInstResultPtr);


// Debug utilities.

void VKSetDebugObjectName(VkDevice vkDevice, VkObjectType vkObjType, uint64_t vkObjHandle, const char* name);

bool RegisterDebugReportCallback(VkInstance vkInst, PFN_vkDebugReportCallbackEXT vkDebugReportCallbackFunc);


// VkPhysicalDevice helper functions.

bool VKPickSinglePhysicalDeviceAndQueueFamily(const VkInstance& vkInst, const VkSurfaceKHR& surf, VkPhysicalDevice* vkPhysicalDevice, uint32_t* vkQueueFamilyIdx);

void VKPrintPhysicalDeviceName(const VkPhysicalDevice& vkPhysicalDevice);

void VKPrintPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice& vkPhysicalDevice);

void VKPrintPhysicalDeviceFeatures(const VkPhysicalDevice& vkPhysicalDevice);


// VkSurface helper functions.

bool VKPickSurfaceFormat(VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR* vkSurfFmtResult);

void VKPrintSurfaceCapabilities(const char* surfName, VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf);


// Create a logical device.

struct VKDeviceQueueCreateRequest
{
	uint32_t QueueFamilyIdx;
	uint32_t QueueCount;
	std::vector<float> QueuePriorities;
};
//bool VKCreateDevice(
//	VkDevice& vkDevice, VkPhysicalDevice vkPhysicalDevice,
//	size_t queueCreateRequestCount, const VKDeviceQueueCreateRequest* queueCreateRequests,
//	size_t enableLayerCount, const char** enableLayerNames,
//	size_t enableExtensionCount, const char** enableExtensionNames,
//	const void* featureLinkedList = nullptr, VkAllocationCallbacks* vkAllocator = nullptr);

bool VKCreateDevice(
	VkPhysicalDevice vkPhysicalDevice,
	std::span<const VKDeviceQueueCreateRequest> queueCreateRequests,
	std::span<const char*> enableLayerNames,
	std::span<const char*> enableExtensionNames,
	const VkAllocationCallbacks* vkAllocator,
	VkDevice* vkDeviceResult);

bool VKCreateSwapchainRenderPass(
	VkDevice device,
	VkFormat surfaceFormat,
	VkAllocationCallbacks* allocator,
	VkRenderPass* renderPass);

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


// Destroy helper functions.
void VKDestroySemaphoreVector(VkDevice device, std::vector<VkSemaphore>& semaphores, VkAllocationCallbacks* allocator);
void VKDestroyFenceVector(VkDevice device, std::vector<VkFence>& fences, VkAllocationCallbacks* allocator);
void VKDestroyImageViewVector(VkDevice device, std::vector<VkImageView>& imageViews, VkAllocationCallbacks* allocator);
void VKDestroyFramebufferVector(VkDevice device, std::vector<VkFramebuffer>& framebuffers, VkAllocationCallbacks* allocator);


#endif