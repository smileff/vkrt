#ifndef _VK_FUNCTION_H_
#define _VK_FUNCTION_H_

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <exception>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <numeric>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <thread>

#include "vulkan/vulkan.h"
#include "vulkan/vk_enum_string_helper.h"

extern VkAllocationCallbacks* g_vkAllocator;

extern VkResult g_vkLastResult;
void VKPrintLastError();

#define VKCall(code)\
{\
	VkResult res = (code);\
	if (res != VK_SUCCESS) {\
		fprintf(stderr, "[%s:%u] \"" #code "\" returns: %s.\n", __FILE__, __LINE__, string_VkResult(res));\
		assert(0);\
	}\
}

#define VKFailed(code) (g_vkLastResult = (code), g_vkLastResult != VK_SUCCESS)


// Create VkInstance according to input parameters.
bool VKCreateInstance(VkInstance& vkInst, size_t instExtNum, const char** instExts, size_t enableLayerNum, const char** enableLayers);

// Pick a physical device supporting the input surface, and with single queue family.
bool VKPickPhysicalDeviceAndOneQueueFamily(VkInstance vkInst, VkSurfaceKHR surf, VkPhysicalDevice* vkPhysicalDevice, uint32_t* vkQueueFamilyIdx);

// Create a logical device with the input device queue create requests.
struct VKDeviceQueueCreateRequest
{
	uint32_t QueueFamilyIdx;
	uint32_t QueueCount;
	std::vector<float> QueuePriorities;
};
bool VKCreateDevice(
	VkDevice& vkDevice,
	VkPhysicalDevice vkPhysicalDevice, 
	const std::vector<VKDeviceQueueCreateRequest>& vkQueueCreateRequests, 
	uint32_t enableLayerNum, const char** enableLayers,
	uint32_t enableExtensionNum, const char** enableExtensions,
	const void* featureLinkedList = nullptr);

void VKPrintSurfaceCapabilities(const char* surfName, VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf);

void VKPrintPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice vkPhysicalDevice);

void VKPrintPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures& vkPhysicalDeviceFeatures);

VkBool32 VKDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);

bool VKRegisterDebugReportCallback(VkInstance vkInst);

bool VKPickSurfaceFormat(VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR& surfFmt);

// Swapchain vulkan objects.
class VKSwapchainContext
{
public:

	VKSwapchainContext(int swapchainImageNum = 3);
	virtual ~VKSwapchainContext();

	bool InitSwapchain(VkDevice vkDevice, uint32_t queueFamilyIdx, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR vkSurfaceFormat, uint32_t rtWidth, uint32_t rtHeight);

	const VkSwapchainKHR& GetSwapchain() const { return m_vkSwapchain; }
	const VkRenderPass& GetSwapchainRenderPass() const { return m_swapchainRenderPass; }
	const VkRect2D& GetSwapchainRect() const { return m_swapchainRect; }

	// Render a frame.
	uint32_t GetSwapchainImageNum() const { return m_swapchainImageNum; }
	const uint32_t& GetSwapchainImageIdx() const { return m_swapchainImageIdx; }

	bool AcquireNextSwapchainImage(VkSemaphore imageAvailableSemaphore, uint64_t timeout = UINT64_MAX);

	const VkFramebuffer& GetSwapchainFramebuffer() const { return m_swapchainFramebuffers[m_swapchainImageIdx]; }

private:
	VkDevice m_vkDevice = VK_NULL_HANDLE;

	const int m_swapchainImageNum;
	uint32_t m_swapchainImageIdx = (uint32_t)-1;
	VkSurfaceFormatKHR m_vkSwapchainSurfaceFormat = {};
	VkSwapchainKHR m_vkSwapchain = VK_NULL_HANDLE;
	VkRenderPass m_swapchainRenderPass = VK_NULL_HANDLE;
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	VkRect2D m_swapchainRect = {};
};

// Semaphores and fences for inflight rendering.
class VKInflightContext
{
public:
	VKInflightContext(int inflightFrameNum = 2);
	virtual ~VKInflightContext();

	bool InitInflight(VkDevice vkDevice);

	uint32_t GetInflightFrameNum() const { return m_inflightFrameNum; }
	uint32_t GetInflightFrameIdx() const { return m_inflightFrameIdx; }

	bool WaitForInflightFrameFence(uint64_t timeout = UINT64_MAX);

	const VkSemaphore& GetImageAvailableSemaphore() const { return m_vkImageAvailableSemaphores[m_inflightFrameIdx]; }
	const VkSemaphore& GetRenderFinishedSemaphore() const { return m_vkRenderFinishedSemaphores[m_inflightFrameIdx]; }
	const VkFence& GetInflightFrameFence() const { return m_vkInflightFrameFences[m_inflightFrameIdx]; }

	void NextFrame();

private:
	VkDevice m_vkDevice = VK_NULL_HANDLE;

	const uint32_t m_inflightFrameNum = 2;
	uint32_t m_inflightFrameIdx = 0;
	std::vector<VkSemaphore> m_vkImageAvailableSemaphores;
	std::vector<VkSemaphore> m_vkRenderFinishedSemaphores;
	std::vector<VkFence> m_vkInflightFrameFences;
};

#endif