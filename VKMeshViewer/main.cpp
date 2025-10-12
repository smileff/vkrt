#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <exception>
#include <vector>
#include <deque>
#include <set>
#include <numeric>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <thread>

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include "vulkan/vulkan.h"
#include "vulkan/vk_enum_string_helper.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

static const VkAllocationCallbacks* g_vkAllocator = nullptr;

#define VKCall(code)\
{\
	VkResult res = (code);\
	if (res != VK_SUCCESS) {\
		fprintf(stderr, "[%s:%u] \"" #code "\" returns: %s.\n", __FILE__, __LINE__, string_VkResult(res));\
		assert(0);\
	}\
}

bool PickPhysicalDevice(VkInstance vkInst, VKSurface vkSurf, VkPhysicalDevice *vkPhysicalDevice, uint32_t *vkQueueFamilyIdx)
{
	// Enumerate physical devices.
	uint32_t vkPhysicalDeviceCnt;
	VKCall(vkEnumeratePhysicalDevices(vkInst, &vkPhysicalDeviceCnt, nullptr));
	std::vector<VkPhysicalDevice> vkPhysicalDevices(vkPhysicalDeviceCnt);
	VKCall(vkEnumeratePhysicalDevices(vkInst, &vkPhysicalDeviceCnt, vkPhysicalDevices.data()));

	// Loop each physical device to select the suitable one; prefer discrete GPU.
	*vkPhysicalDevice = VK_NULL_HANDLE;
	*vkQueueFamilyIdx = (uint32_t)-1;
	for (auto physicalDevice : vkPhysicalDevices)
	{
		// Get physical device property.
		VkPhysicalDeviceProperties vkPhysicalDeviceProps;
		vkGetPhysicalDeviceProperties(physicalDevice, &vkPhysicalDeviceProps);

		// Get physical device queue family properties.
		uint32_t queueFamilyPropCnt;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCnt, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCnt);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCnt, queueFamilyProps.data());

		// Loop each queue family property to find a queue family supporting all graphics, compute and transfer.
		VkFlags reqFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
		uint32_t queueFamilyIdx = 0;
		while (queueFamilyIdx < queueFamilyPropCnt)
		{
			const VkQueueFamilyProperties& props = queueFamilyProps[queueFamilyIdx];
			if ((props.queueFlags & reqFlags) == reqFlags) {
				break;
			} else {
				++queueFamilyIdx;
			}
		}

		if (queueFamilyIdx == queueFamilyPropCnt) {
			// No queue family meet the requirements.
			continue;
		}

		//Found.
		*vkPhysicalDevice = physicalDevice;
		*vkQueueFamilyIdx = queueFamilyIdx;

		// Prefer discrete device.
		// if (vkPhysicalDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
		if (vkPhysicalDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			break;
		}
	}

	return true;
}

class SDLVKContext
{
public:
	virtual ~SDLVKContext();

	bool Initialize(const char* winTitle, int winWidth, int winHeight);
private:
	// Call the following functions in order.
	bool CreateVkInstance(size_t reqExtNum, const char* reqExts, VkInstance* vkInst);
	bool PickPhysicalDeviceOneQueue();

public:


	virtual virtual PickPhysicalDeviceAndQueueFamily();

	SDL_Window* m_mainWin = nullptr;
	VkInstance m_vkInst = VK_NULL_HANDLE;
	VkSurfaceKHR m_vkSurf = VK_NULL_HANDLE;
	VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
};

int main(int argc, char** argv)
{


	// Create SDL window.
	const int WindowWidth = 1280;
	const int WindowHeight = 720;








	// Print the physical device features
	//PrintPhysicalDeviceFeatures(vkPhysicalDevice);

	std::cout << "Pick the " << vkQueueFamilyIdx << "th queue family." << std::endl;
	std::cout << std::endl;

	// 

	// Create vulkan logical device.
	const uint32_t vkQueueCnt = 1;
	VkDevice vkDevice = VK_NULL_HANDLE;
	std::vector<float> queuePriorities(vkQueueCnt, 1.0f);
	std::vector<const char*> vkDeviceExts = {};

	// Create logical device and device queue.
	VkDeviceQueueCreateInfo deviceQueueCInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = vkQueueFamilyIdx,
		.queueCount = vkQueueCnt,
		.pQueuePriorities = queuePriorities.data(),
	};

	// Test, try to create a lot of devices.
	std::vector<uint32_t> deviceNum = { 1, 10, 100, 1000, 10000, };
	for (size_t i = 0; i < deviceNum.size(); ++i)
	{
		uint32_t toCreateDeviceNum = deviceNum[i];
		
	}
	// Create device
	VkDeviceCreateInfo deviceCInfo = {
		.sType{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO },
		.queueCreateInfoCount{ 1 },
		.pQueueCreateInfos{ &deviceQueueCInfo },
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = (uint32_t)vkDeviceExts.size(),
		.ppEnabledExtensionNames = vkDeviceExts.data(),
	};
	VKCall(vkCreateDevice(vkPhysicalDevice, &deviceCInfo, g_vkAllocator, &vkDevice));

	// Get device queue.
	VkQueue vkQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(vkDevice, vkQueueFamilyIdx, 0, &vkQueue);

	//std::vector<VkDevice> devices(10);
	//for (int i = 0; i < (int)devices.size(); ++i) {
	//	std::cout << "Creating the " << i << "th device." << std::endl;
	//	if (!VKCreateDevice(vkPhysicalDevice, vkQueueFamilyIdx, 1, &devices[i])) {
	//		return -1;
	//	}
	//}

	//// Get multiple device queues.
	//std::vector<VkQueue> queues(1000);
	//for (int i = 0; i < (int)queues.size(); ++i) {
	//	std::cout << "Getting the " << i << "th queue." << std::endl;
	//	vkGetDeviceQueue(devices[i], vkQueueFamilyIdx, 0, &queues[i]);
	//}

	vkDestroyDevice(vkDevice, g_vkAllocator);
	vkDestroyInstance(vkInst, g_vkAllocator);
	
	return 0;
}