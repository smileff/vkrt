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

VkResult g_vkResult = VK_RESULT_MAX_ENUM;

#define VKCall(code)\
{\
	VkResult res = (code);\
	if (res != VK_SUCCESS) {\
		fprintf(stderr, "[%s:%u] \"" #code "\" returns: %s.\n", __FILE__, __LINE__, string_VkResult(res));\
		assert(0);\
	}\
}

#define VKFail(code) (g_vkResult = (code), g_vkResult != VK_SUCCESS)

// Some context structures?

struct ApplicationContext
{
	int WinWidth, WinHeight;
	SDL_Window* Window;
};

struct VulkanContext
{
	VkInstance Instance;
	VkPhysicalDevice PhysicalDevice;
	VkDevice Device;
	uint32_t QueueFamilyIdx;
	VkQueue Queue;
	VkSurfaceKHR Surface;
	VkSwapchainKHR Swapchain;
	//VkRenderPass RenderPass;
	std::vector<VkImage> SwapchainImages;
	std::vector<VkImageView> SwapchainImageViews;
	std::vector<VkFramebuffer> Framebuffers;

	std::vector<VkCommandBuffer> CommandBuffers;
	std::vector<VkSemaphore> ImageAvailableSemaphores;
	std::vector<VkSemaphore> RenderFinishedSemaphores;

	std::vector<VkFence> InFlightFences;
	std::vector<VkFence> ImagesInFlight;

	uint32_t CurrentFrame;
};

void VKPrintLastError()
{
	fprintf(stderr, "[%s:%u] Last error: %s.\n", __FILE__, __LINE__, string_VkResult(g_vkResult));
}

bool VKCreateInstance(uint32_t reqVkInstExtNum, const char** reqVkInstExts, VkInstance *vkInst)
{
	// Specify version version by ApplicationInfo.
	VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = VK_API_VERSION_1_2,
	};

	// Enable layers
	std::vector<const char*> enableLayers = {
		"VK_LAYER_KHRONOS_validation",	// Enable validation layer.
	};

	// Required instance extensions.
	std::vector<const char*> reqInstExts(reqVkInstExts, reqVkInstExts + reqVkInstExtNum);
	// Append the debug report callback extension.
	reqInstExts.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	// Create instance.
	VkInstanceCreateInfo instCInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = (uint32_t)enableLayers.size(),
		.ppEnabledLayerNames = enableLayers.data(),
		.enabledExtensionCount = (uint32_t)reqInstExts.size(),
		.ppEnabledExtensionNames = reqInstExts.data(),
	};
	if (VKFail(vkCreateInstance(&instCInfo, g_vkAllocator, vkInst))) {
		VKPrintLastError();
		return false;
	}

	return true;
}

bool VKPickPhysicalDevice(VkInstance vkInst, VkPhysicalDevice *vkPhysicalDevice, uint32_t *vkQueueFamilyIdx)
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

bool VKCreateDevice(VkPhysicalDevice vkPhysicalDevice, uint32_t vkQueueFamilyIdx, uint32_t vkQueueCnt, VkDevice *vkDevice)
{
	std::vector<float> queuePriorities(vkQueueCnt, 1.0f);
	std::vector<const char*> vkDeviceExts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };

	// Create logical device and device queue.
	VkDeviceQueueCreateInfo deviceQueueCInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = vkQueueFamilyIdx,
		.queueCount = vkQueueCnt,
		.pQueuePriorities = queuePriorities.data(),
	};

	// Enable dynamic rendering extension.
	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
		.dynamicRendering = VK_TRUE,
	};

	VkDeviceCreateInfo deviceCInfo = {
		.sType{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO },
		.pNext{ &dynamicRenderingFeatures },
		.queueCreateInfoCount{ 1 },
		.pQueueCreateInfos{ &deviceQueueCInfo },
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = (uint32_t)vkDeviceExts.size(),
		.ppEnabledExtensionNames = vkDeviceExts.data(),
	};
	*vkDevice = VK_NULL_HANDLE;
	if (VKFail(vkCreateDevice(vkPhysicalDevice, &deviceCInfo, g_vkAllocator, vkDevice))) {
		VKPrintLastError();
		return false;
	}

	return true;
}

//void GLFWHandleKeyFunction(GLFWwindow* win, int key, int scanCode, int action, int mods)
//{
//	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
//		glfwSetWindowShouldClose(win, GL_TRUE);
//	}
//}

void PrintSurfaceCapabilities(const char* surfName, VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf)
{
	std::cout << "Surface: " << surfName << ", capabilities:" << std::endl;
	//std::cout << "--------------------------------" << std::endl;
	VkSurfaceCapabilitiesKHR surfCaps;
	VKCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice, vkSurf, &surfCaps));
	std::cout << "MinImageCount: " << surfCaps.minImageCount << std::endl;
	std::cout << "MaxImageCount: " << surfCaps.maxImageCount << std::endl;
	std::cout << "CurrentExtent: " << surfCaps.currentExtent.width << " * " << surfCaps.currentExtent.height << std::endl;
	std::cout << "MinImageExtent: " << surfCaps.minImageExtent.width << " * " << surfCaps.minImageExtent.height << std::endl;
	std::cout << "MaxImageExtent: " << surfCaps.maxImageExtent.width << " * " << surfCaps.maxImageExtent.height << std::endl;
	std::cout << "MaxImageArrayLayers: " << surfCaps.maxImageArrayLayers << std::endl;

	// Print transforms info.
	std::cout << "SupportedTransforms: " << std::endl;
	for (uint32_t i = 0; i < 32; ++i) {
		VkSurfaceTransformFlagBitsKHR flag = (VkSurfaceTransformFlagBitsKHR)(1 << i);
		if (flag & surfCaps.supportedTransforms) {
			std::cout << "\t" << string_VkSurfaceTransformFlagBitsKHR(flag) << std::endl;
		}
	}
	std::cout << "CurrentTransforms: ";
	if (surfCaps.currentTransform == 0) {
		std::cout << "none" << std::endl;
	} else {
		for (uint32_t i = 0; i < 32; ++i) {
			VkSurfaceTransformFlagBitsKHR flag = (VkSurfaceTransformFlagBitsKHR)(1 << i);
			if (flag & surfCaps.currentTransform) {
				std::cout << string_VkSurfaceTransformFlagBitsKHR(flag);
			}
		}
		std::cout << std::endl;
	}

	// Print alpha composite info.
	std::cout << "Supported composite alpha:" << std::endl;
	for (uint32_t i = 0; i < 32; ++i) {
		VkCompositeAlphaFlagBitsKHR flag = (VkCompositeAlphaFlagBitsKHR)(1 << i);
		if (flag & surfCaps.supportedCompositeAlpha) {
			std::cout << "\t" << string_VkCompositeAlphaFlagBitsKHR(flag) << std::endl;
		}
	}

	// Print supprted usage info.
	std::cout << "Supported usage:" << std::endl;
	for (uint32_t i = 0; i < 32; ++i) {
		VkImageUsageFlagBits flag = (VkImageUsageFlagBits)(1 << i);
		if (flag & surfCaps.supportedUsageFlags) {
			std::string flagStr = string_VkImageUsageFlagBits(flag);
			std::cout << "\t" << flagStr << std::endl;
		}
	}
}

void PrintPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice vkPhysicalDevice)
{
	uint32_t queueFamilyPropCnt;
	vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyPropCnt, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCnt);
	vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyPropCnt, queueFamilyProps.data());

	std::cout << "Queue family properties:" << std::endl;
	//std::cout << "--------------------------------" << std::endl;
	for (uint32_t i = 0; i < queueFamilyPropCnt; ++i) {
		const VkQueueFamilyProperties& props = queueFamilyProps[i];
		std::cout << "Queue family " << i << ":" << std::endl;
		std::cout << "\tqueueCount: " << props.queueCount << std::endl;
		std::cout << std::hex << std::uppercase;
		std::cout << "\tqueueFlags: " << props.queueFlags << std::endl;
		for (uint32_t bitIdx = 0; bitIdx < 32; ++bitIdx) {
			if (props.queueFlags & (1 << bitIdx)) {
				std::cout << "\t\t" << string_VkQueueFlagBits((VkQueueFlagBits)(1 << bitIdx)) << std::endl;
			}
		}
		std::cout << "\ttimestampValidBits: " << props.timestampValidBits << std::endl;
		std::cout << std::dec << std::nouppercase;

		std::cout << "\tminImageTransferGranularity: " << props.minImageTransferGranularity.width << " * " << props.minImageTransferGranularity.height << " * " << props.minImageTransferGranularity.depth << std::endl;
	}
}

void VKPrintPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures& vkPhysicalDeviceFeatures)
{
	std::cout << "Physical device features:" << std::endl;
	//std::cout << "--------------------------------" << std::endl;
	std::cout << "\trobustBufferAccess: " << (vkPhysicalDeviceFeatures.robustBufferAccess ? "Yes" : "NO") << std::endl;
	std::cout << "\tfullDrawIndexUint32: " << (vkPhysicalDeviceFeatures.fullDrawIndexUint32 ? "Yes" : "NO") << std::endl;
	std::cout << "\timageCubeArray: " << (vkPhysicalDeviceFeatures.imageCubeArray ? "Yes" : "NO") << std::endl;
	std::cout << "\tindependentBlend: " << (vkPhysicalDeviceFeatures.independentBlend ? "Yes" : "NO") << std::endl;
	std::cout << "\tgeometryShader: " << (vkPhysicalDeviceFeatures.geometryShader ? "Yes" : "NO") << std::endl;
	std::cout << "\ttessellationShader: " << (vkPhysicalDeviceFeatures.tessellationShader ? "Yes" : "NO") << std::endl;
	std::cout << "\tsampleRateShading: " << (vkPhysicalDeviceFeatures.sampleRateShading ? "Yes" : "NO") << std::endl;
	std::cout << "\tdualSrcBlend: " << (vkPhysicalDeviceFeatures.dualSrcBlend ? "Yes" : "NO") << std::endl;
	std::cout << "\tlogicOp: " << (vkPhysicalDeviceFeatures.logicOp ? "Yes" : "NO") << std::endl;
	std::cout << "\tmultiDrawIndirect: " << (vkPhysicalDeviceFeatures.multiDrawIndirect ? "Yes" : "NO") << std::endl;
	std::cout << "\tdrawIndirectFirstInstance: " << (vkPhysicalDeviceFeatures.drawIndirectFirstInstance ? "Yes" : "NO") << std::endl;
	std::cout << "\tdepthClamp: " << (vkPhysicalDeviceFeatures.depthClamp ? "Yes" : "NO") << std::endl;
	std::cout << "\tdepthBiasClamp: " << (vkPhysicalDeviceFeatures.depthBiasClamp ? "Yes" : "NO") << std::endl;
	std::cout << "\tfillModeNonSolid: " << (vkPhysicalDeviceFeatures.fillModeNonSolid ? "Yes" : "NO") << std::endl;
	std::cout << "\tdepthBounds: " << (vkPhysicalDeviceFeatures.depthBounds ? "Yes" : "NO") << std::endl;
	std::cout << "\twideLines: " << (vkPhysicalDeviceFeatures.wideLines ? "Yes" : "NO") << std::endl;
	std::cout << "\tlargePoints: " << (vkPhysicalDeviceFeatures.largePoints ? "Yes" : "NO") << std::endl;
	std::cout << "\talphaToOne: " << (vkPhysicalDeviceFeatures.alphaToOne ? "Yes" : "NO") << std::endl;
	std::cout << "\tmultiViewport: " << (vkPhysicalDeviceFeatures.multiViewport ? "Yes" : "NO") << std::endl;
	std::cout << "\tsamplerAnisotropy: " << (vkPhysicalDeviceFeatures.samplerAnisotropy ? "Yes" : "NO") << std::endl;
	std::cout << "\ttextureCompressionETC2: " << (vkPhysicalDeviceFeatures.textureCompressionETC2 ? "Yes" : "NO") << std::endl;
	std::cout << "\ttextureCompressionASTC_LDR: " << (vkPhysicalDeviceFeatures.textureCompressionASTC_LDR ? "Yes" : "NO") << std::endl;
	std::cout << "\ttextureCompressionBC: " << (vkPhysicalDeviceFeatures.textureCompressionBC ? "Yes" : "NO") << std::endl;
	std::cout << "\tocclusionQueryPrecise: " << (vkPhysicalDeviceFeatures.occlusionQueryPrecise ? "Yes" : "NO") << std::endl;
	std::cout << "\tpipelineStatisticsQuery: " << (vkPhysicalDeviceFeatures.pipelineStatisticsQuery ? "Yes" : "NO") << std::endl;
	std::cout << "\tvertexPipelineStoresAndAtomics: " << (vkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics ? "Yes" : "NO") << std::endl;
	std::cout << "\tfragmentStoresAndAtomics: " << (vkPhysicalDeviceFeatures.fragmentStoresAndAtomics ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderTessellationAndGeometryPointSize: " << (vkPhysicalDeviceFeatures.shaderTessellationAndGeometryPointSize ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderImageGatherExtended: " << (vkPhysicalDeviceFeatures.shaderImageGatherExtended ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderStorageImageExtendedFormats: " << (vkPhysicalDeviceFeatures.shaderStorageImageExtendedFormats ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderStorageImageMultisample: " << (vkPhysicalDeviceFeatures.shaderStorageImageMultisample ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderStorageImageReadWithoutFormat: " << (vkPhysicalDeviceFeatures.shaderStorageImageReadWithoutFormat ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderStorageImageWriteWithoutFormat: " << (vkPhysicalDeviceFeatures.shaderStorageImageWriteWithoutFormat ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderUniformBufferArrayDynamicIndexing: " << (vkPhysicalDeviceFeatures.shaderUniformBufferArrayDynamicIndexing ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderSampledImageArrayDynamicIndexing: " << (vkPhysicalDeviceFeatures.shaderSampledImageArrayDynamicIndexing ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderStorageBufferArrayDynamicIndexing: " << (vkPhysicalDeviceFeatures.shaderStorageBufferArrayDynamicIndexing ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderStorageImageArrayDynamicIndexing: " << (vkPhysicalDeviceFeatures.shaderStorageImageArrayDynamicIndexing ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderClipDistance: " << (vkPhysicalDeviceFeatures.shaderClipDistance ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderCullDistance: " << (vkPhysicalDeviceFeatures.shaderCullDistance ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderFloat64: " << (vkPhysicalDeviceFeatures.shaderFloat64 ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderInt64: " << (vkPhysicalDeviceFeatures.shaderInt64 ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderInt16: " << (vkPhysicalDeviceFeatures.shaderInt16 ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderResourceResidency: " << (vkPhysicalDeviceFeatures.shaderResourceResidency ? "Yes" : "NO") << std::endl;
	std::cout << "\tshaderResourceMinLod: " << (vkPhysicalDeviceFeatures.shaderResourceMinLod ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseBinding: " << (vkPhysicalDeviceFeatures.sparseBinding ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseResidencyBuffer: " << (vkPhysicalDeviceFeatures.sparseResidencyBuffer ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseResidencyImage2D: " << (vkPhysicalDeviceFeatures.sparseResidencyImage2D ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseResidencyImage3D: " << (vkPhysicalDeviceFeatures.sparseResidencyImage3D ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseResidency2Samples: " << (vkPhysicalDeviceFeatures.sparseResidency2Samples ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseResidency4Samples: " << (vkPhysicalDeviceFeatures.sparseResidency4Samples ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseResidency8Samples: " << (vkPhysicalDeviceFeatures.sparseResidency8Samples ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseResidency16Samples: " << (vkPhysicalDeviceFeatures.sparseResidency16Samples ? "Yes" : "NO") << std::endl;
	std::cout << "\tsparseResidencyAliased: " << (vkPhysicalDeviceFeatures.sparseResidencyAliased ? "Yes" : "NO") << std::endl;
	std::cout << "\tvariableMultisampleRate: " << (vkPhysicalDeviceFeatures.variableMultisampleRate ? "Yes" : "NO") << std::endl;
	std::cout << "\tinheritedQueries: " << (vkPhysicalDeviceFeatures.inheritedQueries ? "Yes" : "NO") << std::endl;
}


VkBool32 VKDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData) 
{
	std::cout << "[" << layerPrefix << "] " << msg << std::endl;
	fprintf(stderr, "[%s] %s\n", layerPrefix, msg);
	return VK_FALSE;
}

bool RegisterDebugReportCallback(VkInstance vkInst)
{
	// Get the vkCreateDebugReportCallbackEXT function.
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(vkInst, "vkCreateDebugReportCallbackEXT");

	// Register debug report callback.
	VkDebugReportCallbackEXT vkDebugReportCallback = VK_NULL_HANDLE;
	VkDebugReportCallbackCreateInfoEXT debugReportCallbackCInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
		.pfnCallback = VKDebugReportCallback,
	};
	if (VKFail(vkCreateDebugReportCallbackEXT(vkInst, &debugReportCallbackCInfo, g_vkAllocator, &vkDebugReportCallback))) {
		VKPrintLastError();
		return false;
	}

	return true;
}

bool PickSurfaceFormat(VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR *surfFmt)
{
	// Get all supported surface formats.
	uint32_t supportedSurfFmtCnt = 0;
	VKCall(vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurf, &supportedSurfFmtCnt, nullptr));
	std::vector<VkSurfaceFormatKHR> supportedSurfFmts(supportedSurfFmtCnt);
	VKCall(vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurf, &supportedSurfFmtCnt, supportedSurfFmts.data()));

	// First try to find B8G8R8A8.
	VkSurfaceFormatKHR preferSurfFmt = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	if (std::find_if(supportedSurfFmts.begin(), supportedSurfFmts.end(), [toFindFmt = preferSurfFmt](const VkSurfaceFormatKHR& fmt)->bool
		{
			return fmt.format == toFindFmt.format && fmt.colorSpace == toFindFmt.colorSpace;
		}) != supportedSurfFmts.end())
	{
		*surfFmt = preferSurfFmt;
		return true;
	}

	// Then try to find R8G8B8A8.
	VkSurfaceFormatKHR preferSurfFmt2 = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	if (std::find_if(supportedSurfFmts.begin(), supportedSurfFmts.end(), [toFindFmt = preferSurfFmt2](const VkSurfaceFormatKHR& fmt)->bool
		{
			return fmt.format == toFindFmt.format && fmt.colorSpace == toFindFmt.colorSpace;
		}) != supportedSurfFmts.end())
	{
		*surfFmt = preferSurfFmt;
		return true;
	}

	// Can't find the prefered format.
	return false;
}

int main(int argc, char** argv)
{
	// Initialize SDL
	SDL_Init(SDL_INIT_EVERYTHING);

	// Create SDL window.
	const int winWidth = 1280;
	const int winHeight = 720;
	uint32_t sdlWindowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	SDL_Window* win = SDL_CreateWindow("SDL Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winWidth, winHeight, sdlWindowFlags);
	if (!win) {
		std::cerr << "Failed to create SDL window." << std::endl;
		SDL_Quit();
		return -1;
	}

	// Get the drawable size of the window.
	int rtWidth, rtHeight;
	SDL_Vulkan_GetDrawableSize(win, &rtWidth, &rtHeight);

	// Get instance extensions required by SDL.
	uint32_t reqVkInstExtNum = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(win, &reqVkInstExtNum, nullptr)) {
		std::cerr << "Failed to get instance extensions required by SDL." << std::endl;		
	}
	std::vector<const char*> reqVkInstExts(reqVkInstExtNum);
	if (reqVkInstExtNum > 0) {
		if (!SDL_Vulkan_GetInstanceExtensions(win, &reqVkInstExtNum, &reqVkInstExts[0])) {
			std::cerr << "Failed to get instance extensions required by SDL." << std::endl;
		}
	}

	// Create vulkan instance.
	VkInstance vkInst{ VK_NULL_HANDLE };
	if (!VKCreateInstance((uint32_t)reqVkInstExts.size(), reqVkInstExts.data(), &vkInst)) {
		return -1;
	}

	//// Create multiple instances.
	//std::vector<VkInstance> instances(1000);
	//for (int i = 0; i < (int)instances.size(); ++i) {
	//	std::cout << "Creating the " << i << "th instance." << std::endl;
	//	if (!VKCreateInstance((uint32_t)reqVkInstExts.size(), reqVkInstExts.data(), &instances[i])) {
	//		return -1;
	//	}
	//}

	// Register debug report callback.
	// RegisterDebugReportCallback(vkInst);

	// Pick vulkan physical device.
	VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
	uint32_t vkQueueFamilyIdx = (uint32_t)-1;
	if (!VKPickPhysicalDevice(vkInst, &vkPhysicalDevice, &vkQueueFamilyIdx)) {
		return -1;
	}

	// Print the name of the selected physical device.
	VkPhysicalDeviceProperties vkPhysicalDeviceProps;
	vkGetPhysicalDeviceProperties(vkPhysicalDevice, &vkPhysicalDeviceProps);
	std::cout << vkPhysicalDeviceProps.deviceName << std::endl;

	// Print physical device properties.
	PrintPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice);

	// Print the physical device features
	VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures;
	vkGetPhysicalDeviceFeatures(vkPhysicalDevice, &vkPhysicalDeviceFeatures);
	VKPrintPhysicalDeviceFeatures(vkPhysicalDeviceFeatures);
	std::cout << std::endl;

	// Create vulkan logical device.
	VkDevice vkDevice = VK_NULL_HANDLE;
	if (!VKCreateDevice(vkPhysicalDevice, vkQueueFamilyIdx, 20, &vkDevice)) {
		return -1;
	}

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

	// Create a command pool.
	VkCommandPoolCreateInfo vkCmdPoolCInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vkQueueFamilyIdx,
	};
	VkCommandPool vkCmdPool = VK_NULL_HANDLE;
	if (VKFail(vkCreateCommandPool(vkDevice, &vkCmdPoolCInfo, nullptr, &vkCmdPool))) {
		VKPrintLastError();
		return -1;
	}

	// Get the vulkan surface of SDL window.
	VkSurfaceKHR vkSurf = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(win, vkInst, &vkSurf)) {
		std::cerr << "Failed to create SDL vulkan surface." << std::endl;
		return -1;
	}

	// Check if the physical device support presenting to the surface.
	VkBool32 support = VK_FALSE;
	VKCall(vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice, vkQueueFamilyIdx, vkSurf, &support));
	if (support != VK_TRUE) {
		fprintf(stdout, "Don't support surface.");
		return -1;
	}

	// Get all supported surface formats.
	VkSurfaceFormatKHR vkSurfFmt;
	if (PickSurfaceFormat(vkPhysicalDevice, vkSurf, &vkSurfFmt)) {
		std::cout << "Selected surface format: " << string_VkFormat(vkSurfFmt.format) << ", color space: " << string_VkColorSpaceKHR(vkSurfFmt.colorSpace) << std::endl;
	} else {
		std::cerr << "Failed to select surface format." << std::endl;
		return -1;
	}

	// Print the capabilities of this surface.	
	PrintSurfaceCapabilities(SDL_GetWindowTitle(win), vkPhysicalDevice, vkSurf);
	std::cout << std::endl;

	// Create vulkan swap chain
	VkSwapchainCreateInfoKHR swapchainCInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = vkSurf,
		.minImageCount = 3,
		.imageFormat = vkSurfFmt.format,
		.imageColorSpace = vkSurfFmt.colorSpace,
		.imageExtent = VkExtent2D{ (uint32_t)rtWidth, (uint32_t)rtHeight },
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	VkSwapchainKHR vkSwapchain = VK_NULL_HANDLE;
	if (VKFail(vkCreateSwapchainKHR(vkDevice, &swapchainCInfo, nullptr, &vkSwapchain))) {
		VKPrintLastError();
		return -1;
	}

	// Create a renderpass.
	VkRenderPass vkRenderPass = VK_NULL_HANDLE;
	VkAttachmentDescription attachment = {};
	attachment.format = vkSurfFmt.format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment = {};
	color_attachment.attachment = 0;
	color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo vkRenderPassCInfo = {};
	vkRenderPassCInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	vkRenderPassCInfo.attachmentCount = 1;
	vkRenderPassCInfo.pAttachments = &attachment;
	vkRenderPassCInfo.subpassCount = 1;
	vkRenderPassCInfo.pSubpasses = &subpass;
	vkRenderPassCInfo.dependencyCount = 1;
	vkRenderPassCInfo.pDependencies = &dependency;
	VKCall(vkCreateRenderPass(vkDevice, &vkRenderPassCInfo, g_vkAllocator, &vkRenderPass));

	// Get swap chain images.
	uint32_t swapchainImgCnt;
	VKCall(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapchainImgCnt, nullptr));
	std::vector<VkImage> vkSwapchainImages(swapchainImgCnt);
	if (VKFail(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapchainImgCnt, vkSwapchainImages.data()))) {
		VKPrintLastError();
		return -1;
	}

	// Create image view for each swapchain image.
	std::vector<VkImageView> vkSwapchainImageViews(swapchainImgCnt);
	for (uint32_t imgIdx = 0; imgIdx < swapchainImgCnt; ++imgIdx) {
		VkImageViewCreateInfo imgViewCInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = vkSwapchainImages[imgIdx],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = vkSurfFmt.format,
			.subresourceRange = {
				.aspectMask{ VK_IMAGE_ASPECT_COLOR_BIT },
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		VKCall(vkCreateImageView(vkDevice, &imgViewCInfo, g_vkAllocator, &vkSwapchainImageViews[imgIdx]));
	}

	// Create framebuffers.
	std::vector<VkFramebuffer> vkFramebuffers(swapchainImgCnt);
	for (uint32_t imgIdx = 0; imgIdx < swapchainImgCnt; ++imgIdx) {
		VkFramebufferCreateInfo framebufferCInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = vkRenderPass,
			.attachmentCount = 1,
			.pAttachments = &vkSwapchainImageViews[imgIdx],
			.width = (uint32_t)rtWidth,
			.height = (uint32_t)rtHeight,
			.layers = 1,
		};
		VKCall(vkCreateFramebuffer(vkDevice, &framebufferCInfo, g_vkAllocator, &vkFramebuffers[imgIdx]));
	}

	// Allocate a command buffer for each swapchain image.
	std::vector<VkCommandBuffer> imageCmdBufs(swapchainImgCnt);
	if (swapchainImgCnt > 0) {
		VkCommandBufferAllocateInfo cmdBufAllocInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = vkCmdPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = swapchainImgCnt,
		};
		VKCall(vkAllocateCommandBuffers(vkDevice, &cmdBufAllocInfo, imageCmdBufs.data()));
	}

	const int inflightFrameCnt = 3;

	// Create a semaphore and fence for synchronization.
	std::vector<VkSemaphore> imageAvailableSemaphores(inflightFrameCnt);
	std::vector<VkSemaphore> renderFinishedSemaphores(inflightFrameCnt);
	std::vector<VkFence> renderFinishedFences(inflightFrameCnt);
	for (int inflightIdx = 0; inflightIdx < inflightFrameCnt; ++inflightIdx) {
		VkSemaphoreCreateInfo imageAvailableSemaphoreCInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		VKCall(vkCreateSemaphore(vkDevice, &imageAvailableSemaphoreCInfo, g_vkAllocator, &imageAvailableSemaphores[inflightIdx]));
		VKCall(vkCreateSemaphore(vkDevice, &imageAvailableSemaphoreCInfo, g_vkAllocator, &renderFinishedSemaphores[inflightIdx]));

		VkFenceCreateInfo renderFinishedFenceCInfo = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,  // Initially signaled.
		};
		VKCall(vkCreateFence(vkDevice, &renderFinishedFenceCInfo, g_vkAllocator, &renderFinishedFences[inflightIdx]));
	}

	// Initialize imgUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	// io.ConfigFlags != ImGuiConfigFlags_DockingEnable;

	ImGui_ImplSDL2_InitForVulkan(win);
	ImGui_ImplVulkan_InitInfo init_info = {
		.Instance = vkInst,
		.PhysicalDevice = vkPhysicalDevice,
		.Device = vkDevice,
		.QueueFamily = vkQueueFamilyIdx,
		.Queue = vkQueue,
		.DescriptorPool = VK_NULL_HANDLE,
		.RenderPass = vkRenderPass,
		.MinImageCount = inflightFrameCnt,
		.ImageCount = inflightFrameCnt,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		.PipelineCache = VK_NULL_HANDLE,
		.Subpass = 0,
		.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE * 2,
		//.UseDynamicRendering = true,
		//.PipelineRenderingCreateInfo = {
		//	.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		//	.pNext = nullptr,
		//	.viewMask = 0x0,
		//	.colorAttachmentCount = 1,
		//	.pColorAttachmentFormats = &vkSurfFmt.format,
		//	.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
		//	.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
		//},
		.Allocator = g_vkAllocator,
	};
	ImGui_ImplVulkan_Init(&init_info);

	// Skip loading font.
	io.Fonts->AddFontDefault();

	// Enter SDL event loop.
	std::deque<double> frameTimeHistory;  // In nanosecond
	const size_t frameTimeMaxCnt = 100;
	double frameTimeSum = 0.0;
	std::chrono::time_point prevFrameStartTimePoint = std::chrono::high_resolution_clock::now();
	const int MaxFrameFPS = 120;
	const double MinFrameTime = 1.0e9 / MaxFrameFPS;

	uint32_t frameIdx = 0;
	int inflightIdx = 0;
	bool quit = false;
	while (!quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {

			ImGui_ImplSDL2_ProcessEvent(&event);

			// Check quit.
			quit |= (event.type == SDL_QUIT);  // Quit if ESC is pressed.
			quit |= (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE);  // Quit if ESC is pressed.
			if (quit) {
				break;
			}
		}

		// ImGui handles events.
		

		// ImGui start new frame.
		ImGui_ImplSDL2_NewFrame();
		ImGui_ImplVulkan_NewFrame();
		ImGui::NewFrame();
		ImGui::ShowDemoWindow();
		ImGui::ShowMetricsWindow();
		ImGui::ShowDebugLogWindow();
		ImGui::ShowIDStackToolWindow();
		ImGui::ShowAboutWindow();

		// Maintain frame time history.
		std::chrono::time_point currFrameStartTimePoint = std::chrono::high_resolution_clock::now();
		std::chrono::duration frameDuration = currFrameStartTimePoint - prevFrameStartTimePoint;
		double frameTime = (double)frameDuration.count();
		prevFrameStartTimePoint = currFrameStartTimePoint;
		if (frameTimeHistory.size() > frameTimeMaxCnt) {
			double oldestFrameTime = frameTimeHistory.front();
			frameTimeHistory.pop_front();
			frameTimeSum = std::max(frameTimeSum - oldestFrameTime, 0.0);
		}
		frameTimeHistory.push_back(frameTime);
		frameTimeSum += frameTime;
		double avgFrameTime = static_cast<double>(frameTimeSum / frameTimeHistory.size());
		double avgFrameFPS = 1.0e9 / avgFrameTime;

		// Do rendering.
		{
			if (vkWaitForFences(vkDevice, 1, &renderFinishedFences[inflightIdx], VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
				continue;
			}

			// Try to get the image to render to.
			uint32_t imgIdx;
			VkResult acqRes = vkAcquireNextImageKHR(vkDevice, vkSwapchain, 0, imageAvailableSemaphores[inflightIdx], VK_NULL_HANDLE, &imgIdx);
			if (acqRes != VK_SUCCESS) {
				continue;
			}
			VkImage vkSwapchainImage = vkSwapchainImages[imgIdx];

			//  Now we can begin to render.
			std::stringstream ss;
			ss << "[FPS: " << std::setprecision(2) << std::fixed << std::setw(5) << avgFrameFPS << "] FrameIdx: " << frameIdx << ", InflightIdx: " << inflightIdx << ", SwapchainImgIdx: " << imgIdx << std::endl;
			SDL_SetWindowTitle(win, ss.str().c_str());
			
			// reset the renderFinishedFence.
			VKCall(vkResetFences(vkDevice, 1, &renderFinishedFences[inflightIdx]));

			// Reset command buffer.
			VkCommandBuffer vkCmdBuf = imageCmdBufs[inflightIdx];
			VKCall(vkResetCommandBuffer(vkCmdBuf, 0));

			// Begin the command buffer.
			VkCommandBufferBeginInfo cmdBufBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};
			VKCall(vkBeginCommandBuffer(vkCmdBuf, &cmdBufBeginInfo));

			//// Transfer the image to the color attachment layout.
			//VkImageMemoryBarrier imgBarrier = {
			//	.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			//	.srcAccessMask = 0,
			//	.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			//	.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			//	.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			//	.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			//	.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			//	.image = vkSwapchainImages[inflightIdx],
			//	.subresourceRange = {
			//		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			//		.baseMipLevel = 0,
			//		.levelCount = 1,
			//		.baseArrayLayer = 0,
			//		.layerCount = 1,
			//	},
			//};
			//vkCmdPipelineBarrier(
			//	vkCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
			//	0, nullptr, 0, nullptr, 1, &imgBarrier);

			//// Fill the image with red color
			//VkClearColorValue clearColor = {
			//	.float32 = { 1.0f, 0.0f, 0.0f, 1.0f },
			//};
			//VkImageSubresourceRange fillRange = {
			//	.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			//	.baseMipLevel = 0,
			//	.levelCount = 1,
			//	.baseArrayLayer = 0,
			//	.layerCount = 1,
			//};
			//vkCmdClearColorImage(vkCmdBuf, vkSwapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &fillRange);

			// Transfer the swapchain image to the present layout.
			VkImageMemoryBarrier presentImgBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = vkSwapchainImage,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};
			vkCmdPipelineBarrier(
				vkCmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
				0, nullptr, 0, nullptr, 1, &presentImgBarrier);


			VkClearValue clearValue = { 0.1f, 0.0f, 0.0f, 1.0f };
			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = vkRenderPass;
			renderPassBeginInfo.framebuffer = vkFramebuffers[imgIdx];
			renderPassBeginInfo.renderArea.extent.width = (uint32_t)rtWidth;
			renderPassBeginInfo.renderArea.extent.height = (uint32_t)rtHeight;
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = &clearValue;
			vkCmdBeginRenderPass(vkCmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// ImGui render.
			ImGui::Render();
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdBuf);

			vkCmdEndRenderPass(vkCmdBuf);
			
			// End the command buffer.
			VKCall(vkEndCommandBuffer(vkCmdBuf));

			// Submit the command buffer.
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			VkSubmitInfo submitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &imageAvailableSemaphores[inflightIdx],
				.pWaitDstStageMask = waitStages,
				.commandBufferCount = 1,
				.pCommandBuffers = &vkCmdBuf,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &renderFinishedSemaphores[inflightIdx],
			};
			VKCall(vkQueueSubmit(vkQueue, 1, &submitInfo, renderFinishedFences[inflightIdx]));

			// Queue present.
			VkResult presentRes = VK_SUCCESS;
			VkPresentInfoKHR presentInfo = {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &renderFinishedSemaphores[inflightIdx],
				.swapchainCount = 1,
				.pSwapchains = &vkSwapchain,
				.pImageIndices = &imgIdx,
				.pResults = &presentRes,
			};
			VKCall(vkQueuePresentKHR(vkQueue, &presentInfo));

			inflightIdx = (inflightIdx + 1) % inflightFrameCnt;
			++frameIdx;
		}

		// Limit fps to max 200.
		std::chrono::time_point currFrameEndTimePoint = std::chrono::high_resolution_clock::now();
		std::chrono::duration frameRenderDuration = currFrameEndTimePoint - currFrameStartTimePoint;
		double frameRenderTime = (double)frameRenderDuration.count();
		if (frameRenderTime < MinFrameTime) {
			double toSleepTime = MinFrameTime - frameRenderTime - 2000.0;	// Intentionally minus 2us
			std::this_thread::sleep_for(std::chrono::milliseconds((long long)(toSleepTime * 1.0e-6)));
		}
	}

	// Wait for all vulkan operations to complete.
	VKCall(vkDeviceWaitIdle(vkDevice));

	// ImGuid shutdown.
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	// Free framebuffer and renderpass.
	for (VkFramebuffer fb : vkFramebuffers) {
		vkDestroyFramebuffer(vkDevice, fb, g_vkAllocator);
	}
	vkDestroyRenderPass(vkDevice, vkRenderPass, g_vkAllocator);

	// Free vulkan objects.
	for (VkSemaphore sema : imageAvailableSemaphores) {
		vkDestroySemaphore(vkDevice, sema, g_vkAllocator);
	}
	for (VkSemaphore sema : renderFinishedSemaphores) {
		vkDestroySemaphore(vkDevice, sema, g_vkAllocator);
	}
	for (VkFence fence : renderFinishedFences) {
		vkDestroyFence(vkDevice, fence, g_vkAllocator);
	}
	vkDestroyCommandPool(vkDevice, vkCmdPool, g_vkAllocator);
	for (VkImageView imgView : vkSwapchainImageViews) {
		vkDestroyImageView(vkDevice, imgView, g_vkAllocator);
	}
	vkDestroySwapchainKHR(vkDevice, vkSwapchain, g_vkAllocator);
	vkDestroyDevice(vkDevice, g_vkAllocator);
	vkDestroySurfaceKHR(vkInst, vkSurf, g_vkAllocator);
	vkDestroyInstance(vkInst, g_vkAllocator);

	// Cleanup SDL.
	SDL_DestroyWindow(win);
	SDL_Quit();
	
	return 0;
}