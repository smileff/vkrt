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

std::string GetPhysicalDeviceTypeString(VkPhysicalDeviceType type)
{
	switch (type)
	{
	case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		return "Other";
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return "Integrated GPU";
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return "Discrete GPU";
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		return "Virtual GPU";
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		return "CPU";
	default:
		return "Unknown";
	}
}

void PrintPhysicalDeviceInfo(VkInstance vkInst)
{
	// Get all physical devices.
	uint32_t physicalDeviceNum;
	VKCall(vkEnumeratePhysicalDevices(vkInst, &physicalDeviceNum, nullptr));
	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceNum);
	VKCall(vkEnumeratePhysicalDevices(vkInst, &physicalDeviceNum, physicalDevices.data()));

	// Loop each physical device.
	for (uint32_t i = 0; i < physicalDeviceNum; i++)
	{
		VkPhysicalDevice physicalDevice = physicalDevices[i];

		// Get physical device properties.
		VkPhysicalDeviceProperties physicalDeviceProps;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProps);

		std::cout
			<< GetPhysicalDeviceTypeString(physicalDeviceProps.deviceType) << ": "
			<< physicalDeviceProps.deviceName << ", "
			<< "Vulkan API Ver.: " << VK_VERSION_MAJOR(physicalDeviceProps.apiVersion) << "." << VK_VERSION_MINOR(physicalDeviceProps.apiVersion) << "." << VK_VERSION_PATCH(physicalDeviceProps.apiVersion) << std::endl;

		// Get the queue familiy properties.
		uint32_t queueFamilyPropNum;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropNum, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropNum);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropNum, queueFamilyProps.data());

		//std::cout << "\tQueue family properties:" << std::endl;
		for (uint32_t i = 0; i < queueFamilyPropNum; ++i) {
			const VkQueueFamilyProperties& props = queueFamilyProps[i];
			std::cout << "* Queue family " << i << ":" << std::endl;
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

		std::cout << std::endl;
	}
}

bool PickPhysicalDevice(VkInstance vkInst, VkPhysicalDevice *vkPhysicalDevice, uint32_t *vkQueueFamilyIdx)
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
		std::cout << "* Queue family " << i << ":" << std::endl;
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

void PrintPhysicalDeviceFeatures(VkPhysicalDevice vkPhysicalDevice)
{
	VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures;
	vkGetPhysicalDeviceFeatures(vkPhysicalDevice, &vkPhysicalDeviceFeatures);

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

int main(int argc, char** argv)
{
	// Enable layers
	std::vector<const char*> enableLayers = {
		"VK_LAYER_KHRONOS_validation",	// Enable validation layer.
	};

	// Required instance extensions.
	std::vector<const char*> reqInstExts;
	reqInstExts.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	// Create instance.
	VkInstanceCreateInfo instCInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.enabledLayerCount = (uint32_t)enableLayers.size(),
		.ppEnabledLayerNames = enableLayers.data(),
		.enabledExtensionCount = (uint32_t)reqInstExts.size(),
		.ppEnabledExtensionNames = reqInstExts.data(),
	};
	VkInstance vkInst = VK_NULL_HANDLE;
	VKCall(vkCreateInstance(&instCInfo, g_vkAllocator, &vkInst));

	PrintPhysicalDeviceInfo(vkInst);
	std::cout << std::endl;

	// Pick vulkan physical device.
	VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
	uint32_t vkQueueFamilyIdx = (uint32_t)-1;
	if (!PickPhysicalDevice(vkInst, &vkPhysicalDevice, &vkQueueFamilyIdx)) {
		return -1;
	}

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