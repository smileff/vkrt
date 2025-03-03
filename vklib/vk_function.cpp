#include "vk_function.h"

bool VKCheckError(VkResult result)
{
	if (result == VK_SUCCESS) {
		return true;
	} else {
		fprintf(stderr, "[%s:%u] Last error: %s.\n", __FILE__, __LINE__, string_VkResult(result));
		return false;
	}
}

// SetDebugUtils --------------------------------------------------------

PFN_vkSetDebugUtilsObjectNameEXT VKSetDebugUtilsObjectName = nullptr;

void VKSetDebugObjectName(VkDevice vkDevice, VkObjectType vkObjType, uint64_t vkObjHandle, const char* name)
{
	if (VKSetDebugUtilsObjectName) {
		// Ensure you have the VK_EXT_debug_utils extension enabled
		VkDebugUtilsObjectNameInfoEXT nameInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = vkObjType,
			.objectHandle = vkObjHandle,
			.pObjectName = name,
		};

		// Set the debug name
		VKSetDebugUtilsObjectName(vkDevice, &nameInfo);
	}
}

// PFN_vkSetDebugUtilsObjectTagEXT VKSetDebugUtilsObjectTag = nullptr;

//void VKSetDebugObjectTag(VkDevice vkDevice, VkObjectType vkObjType, uint64_t vkObjHandle, const char* tag)
//{
//	if (VKSetDebugUtilsObjectName) {
//		// Ensure you have the VK_EXT_debug_utils extension enabled
//		VkDebugUtilsObjectTagInfoEXT nameInfo = {
//			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
//			.objectType = vkObjType,
//			.objectHandle = vkObjHandle,
//			.pObjectName = name,
//		};
//
//		// Set the debug name
//		VKSetDebugUtilsObjectName(vkDevice, &nameInfo);
//	}
//}

// CreateInstance

bool VKCreateInstance(VkInstance& vkInst, size_t instExtNum, const char** instExts, size_t enableLayerNum, const char** enableLayers, VkAllocationCallbacks* allocator)
{
	// Specify version version by ApplicationInfo.
	VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = VK_API_VERSION_1_2,
	};

	std::vector<const char*> instExts2(instExts, instExts + instExtNum);
	instExts2.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	// Create instance.
	VkInstanceCreateInfo instCInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = (uint32_t)enableLayerNum,
		.ppEnabledLayerNames = enableLayers,
		.enabledExtensionCount = (uint32_t)instExts2.size(),
		.ppEnabledExtensionNames = instExts2.data(),
	};

	if (!VKSucceed(vkCreateInstance(&instCInfo, allocator, &vkInst))) {
		return false;
	}

	// Get the SetDebugUtilsObjectName function.
	if (!VKSetDebugUtilsObjectName) {
		VKSetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(vkInst, "vkSetDebugUtilsObjectNameEXT");
		if (!VKSetDebugUtilsObjectName) {
			std::cerr << "VK_EXT_debug_utils is not available.\n";
			return false;
		}
	}

	return true;
}

// PickPhysicalDevice 

bool VKPickPhysicalDeviceAndOneQueueFamily(VkInstance inst, VkSurfaceKHR surf, VkPhysicalDevice* toPickPhysDevice, uint32_t* toPickQueueFamilyIdx)
{
	// Enumerate physical devices.
	uint32_t vkPhysicalDeviceCnt;
	VKCall(vkEnumeratePhysicalDevices(inst, &vkPhysicalDeviceCnt, nullptr));
	std::vector<VkPhysicalDevice> vkPhysicalDevices(vkPhysicalDeviceCnt);
	VKCall(vkEnumeratePhysicalDevices(inst, &vkPhysicalDeviceCnt, vkPhysicalDevices.data()));

	// Loop each physical device to select the suitable one; prefer discrete GPU.
	*toPickPhysDevice = VK_NULL_HANDLE;
	*toPickQueueFamilyIdx = (uint32_t)-1;
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
			VkBool32 supportSurf = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIdx, surf, &supportSurf);

			const VkQueueFamilyProperties& props = queueFamilyProps[queueFamilyIdx];
			bool supportFunction = (props.queueFlags & reqFlags) == reqFlags;

			if (supportSurf && supportFunction) {
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
		*toPickPhysDevice = physicalDevice;
		*toPickQueueFamilyIdx = queueFamilyIdx;

		// Prefer discrete device.
		if (vkPhysicalDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			break;
		}
	}

	return true;
}

// CreateDevice

bool VKCreateDevice(
	VkDevice& vkDevice,
	VkPhysicalDevice vkPhysicalDevice,
	const std::vector<VKDeviceQueueCreateRequest>& vkQueueCreateRequests,
	uint32_t enableLayerCount, const char** enableLayerNames,
	uint32_t enableExtensionCount, const char** enableExtensionNames,
	const void* featureLinkedList, VkAllocationCallbacks* vkAllocator)
{
	size_t queueFamilyCnt = vkQueueCreateRequests.size();
	std::vector<VkDeviceQueueCreateInfo> deviceQueueCInfos(queueFamilyCnt);
	for (size_t i = 0; i < queueFamilyCnt; ++i) {
		const VKDeviceQueueCreateRequest& request = vkQueueCreateRequests[i];
		deviceQueueCInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = request.QueueFamilyIdx,
			.queueCount = request.QueueCount,
			.pQueuePriorities = request.QueuePriorities.data(),
		};
	}

	VkDeviceCreateInfo deviceCInfo = {
		.sType{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO },
		.pNext = featureLinkedList,
		.queueCreateInfoCount = (uint32_t)deviceQueueCInfos.size(),
		.pQueueCreateInfos = deviceQueueCInfos.data(),
		.enabledLayerCount = enableLayerCount,
		.ppEnabledLayerNames = enableLayerNames,
		.enabledExtensionCount = enableExtensionCount,
		.ppEnabledExtensionNames = enableExtensionNames,
	};

	vkDevice = VK_NULL_HANDLE;
	if (!VKSucceed(vkCreateDevice(vkPhysicalDevice, &deviceCInfo, vkAllocator, &vkDevice))) {
		return false;
	}

	return true;
}

void VKPrintSurfaceCapabilities(const char* surfName, VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf)
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

void VKPrintPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice vkPhysicalDevice)
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

bool VKRegisterDebugReportCallback(VkInstance vkInst)
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
	if (!VKSucceed(vkCreateDebugReportCallbackEXT(vkInst, &debugReportCallbackCInfo, nullptr, &vkDebugReportCallback))) {
		return false;
	}

	return true;
}

bool VKPickSurfaceFormat(VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR& surfFmt)
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
		surfFmt = preferSurfFmt;
		return true;
	}

	// Then try to find R8G8B8A8.
	VkSurfaceFormatKHR preferSurfFmt2 = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	if (std::find_if(supportedSurfFmts.begin(), supportedSurfFmts.end(), [toFindFmt = preferSurfFmt2](const VkSurfaceFormatKHR& fmt)->bool
		{
			return fmt.format == toFindFmt.format && fmt.colorSpace == toFindFmt.colorSpace;
		}) != supportedSurfFmts.end())
	{
		surfFmt = preferSurfFmt;
		return true;
	}

	// Can't find the prefered format.
	return false;
}

// VKDeviceContext

VKDeviceContext::VKDeviceContext(const char* ctxName)
	: m_vkContextName(ctxName)
{}

VKDeviceContext::~VKDeviceContext()
{
	if (m_vkEmptyPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(m_vkDevice, m_vkEmptyPipelineLayout, nullptr);
	}

	if (m_vkDevice != VK_NULL_HANDLE) {
		vkDestroyDevice(m_vkDevice, nullptr);
	}
}

bool VKDeviceContext::CreateDevice(VkInstance vkInst, VkPhysicalDevice physicalDevice, size_t queueCreateQuestCnt, const VKDeviceQueueCreateRequest* queueCreateQuests, size_t layerCnt, const char** layers, size_t extensionCnt, const char** extensions)
{
	m_vkInstance = vkInst;
	m_vkPhysicalDevice = physicalDevice;
	
	// Create device.
	std::vector<VKDeviceQueueCreateRequest> queueCreateRequestVec(queueCreateQuests, queueCreateQuests + queueCreateQuestCnt);
	if (!VKCreateDevice(m_vkDevice, m_vkPhysicalDevice, queueCreateRequestVec, (uint32_t)layerCnt, layers, (uint32_t)extensionCnt, extensions)) {
		return false;
	}

	// Get queues.
	m_vkQueueFamilyIdxs.reserve(queueCreateQuestCnt);
	m_vkQueueFamilyQueueStartIdxs.reserve(queueCreateQuestCnt + 1);
	m_vkQueues.reserve(queueCreateQuestCnt);
	for (size_t i = 0; i < queueCreateQuestCnt; ++i) {
		const VKDeviceQueueCreateRequest& request = queueCreateQuests[i];

		m_vkQueueFamilyIdxs.push_back(request.QueueFamilyIdx);

		size_t queueStartIdx = m_vkQueues.size();
		m_vkQueueFamilyQueueStartIdxs.push_back(queueStartIdx);

		for (uint32_t j = 0; j < request.QueueCount; ++j) {
			VkQueue queue = VK_NULL_HANDLE;
			vkGetDeviceQueue(m_vkDevice, request.QueueFamilyIdx, j, &queue);
			m_vkQueues.push_back(queue);
		}
	}
	m_vkQueueFamilyQueueStartIdxs.push_back(m_vkQueues.size());

	return true;
}

bool VKDeviceContext::CreateGlobalObjects()
{
	// Create global empty pipeline layout.
	VkPipelineLayoutCreateInfo emptyPipelineLayoutCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	};
	if (!VKSucceed(vkCreatePipelineLayout(m_vkDevice, &emptyPipelineLayoutCInfo, nullptr, &m_vkEmptyPipelineLayout))) {
		return false;
	}

	return true;
}

size_t VKDeviceContext::GetQueueFamilyCount() const
{
	return (uint32_t)m_vkQueueFamilyIdxs.size();
}

uint32_t VKDeviceContext::GetQueueFamilyIdx(uint32_t i) const
{
	assert(i < m_vkQueueFamilyIdxs.size());
	return m_vkQueueFamilyIdxs[i];
}

size_t VKDeviceContext::GetQueueFamilyQueueCount(uint32_t i) const
{
	assert(i < m_vkQueueFamilyIdxs.size());
	size_t startIdx = m_vkQueueFamilyQueueStartIdxs[i];
	size_t endIdx = m_vkQueueFamilyQueueStartIdxs[i + 1];
	return endIdx - startIdx;
}

VkQueue VKDeviceContext::GetQueueFamilyQueue(uint32_t i, uint32_t queueIdx) const
{
	assert(i < m_vkQueueFamilyIdxs.size());
	size_t startIdx = m_vkQueueFamilyQueueStartIdxs[i];
	size_t endIdx = m_vkQueueFamilyQueueStartIdxs[i + 1];
	assert(queueIdx < endIdx - startIdx);
	return m_vkQueues[startIdx + queueIdx];
}

// VKSwapchainContext

VKSwapchainContext::VKSwapchainContext(const VKDeviceContext& vkDeviceContext, int swapchainImageNum, const VkAllocationCallbacks* vkAllocator)
	: m_vkCtx(vkDeviceContext)
	, m_vkAllocator(vkAllocator)
	, m_swapchainImageNum(swapchainImageNum)
{}

VKSwapchainContext::~VKSwapchainContext()
{
	std::for_each(m_swapchainFramebuffers.begin(), m_swapchainFramebuffers.end(),
		[this](VkFramebuffer& fb)
		{
			if (fb != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(m_vkCtx.GetDevice(), fb, m_vkAllocator);
				fb = VK_NULL_HANDLE;
			}
		});

	std::for_each(m_swapchainImageViews.begin(), m_swapchainImageViews.end(),
		[this](VkImageView imageView)
		{
			if (imageView) {
				vkDestroyImageView(m_vkCtx.GetDevice(), imageView, m_vkAllocator);
				imageView = VK_NULL_HANDLE;
			}
		});

	if (m_swapchainRenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(m_vkCtx.GetDevice(), m_swapchainRenderPass, m_vkAllocator);
		m_swapchainRenderPass = VK_NULL_HANDLE;
	}

	if (m_vkSwapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_vkCtx.GetDevice(), m_vkSwapchain, m_vkAllocator);
		m_vkSwapchain = VK_NULL_HANDLE;
	}
}

bool VKSwapchainContext::InitSwapchain(uint32_t queueFamilyIdx, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR vkSurfaceFormat, uint32_t rtWidth, uint32_t rtHeight)
{
	m_vkSwapchainSurfaceFormat = vkSurfaceFormat;

	VkSwapchainCreateInfoKHR vkSwapchainCInfo = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = vkSurf,
			.minImageCount = (uint32_t)m_swapchainImageNum,
			.imageFormat = m_vkSwapchainSurfaceFormat.format,
			.imageColorSpace = m_vkSwapchainSurfaceFormat.colorSpace,
			.imageExtent = VkExtent2D{ (uint32_t)rtWidth, (uint32_t)rtHeight },
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 1,
			.pQueueFamilyIndices = &queueFamilyIdx,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_FIFO_KHR,
			.clipped = VK_TRUE,
	};
	if (!VKSucceed(vkCreateSwapchainKHR(m_vkCtx.GetDevice(), &vkSwapchainCInfo, m_vkAllocator, &m_vkSwapchain))) {
		return false;
	}

	m_swapchainRect = VkRect2D{ {0, 0}, {rtWidth, rtHeight} };

	// And create image view for each image in swapchain.
	uint32_t swapchainImageNum;
	VKCall(vkGetSwapchainImagesKHR(m_vkCtx.GetDevice(), m_vkSwapchain, &swapchainImageNum, nullptr));
	std::vector<VkImage> swapchainImages(swapchainImageNum);
	VKCall(vkGetSwapchainImagesKHR(m_vkCtx.GetDevice(), m_vkSwapchain, &swapchainImageNum, swapchainImages.data()));

	assert(swapchainImageNum == m_swapchainImageNum);

	assert(m_swapchainImageViews.empty());
	m_swapchainImageViews.resize(swapchainImageNum);
	for (int i = 0; i < (int)swapchainImageNum; ++i) {
		VkImageViewCreateInfo imageViewCInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_vkSwapchainSurfaceFormat.format,
			.components = VkComponentMapping {},
			.subresourceRange = VkImageSubresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		VKCall(vkCreateImageView(m_vkCtx.GetDevice(), &imageViewCInfo, m_vkAllocator, &m_swapchainImageViews[i]));
	}

	// Create render pass for the swapchain.
	{
		// Need attachment descriptions, subpass descriptions and subpass denpendencies.
		VkAttachmentDescription attachmentDesc = {
			.format = m_vkSwapchainSurfaceFormat.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};


		VkAttachmentReference colorRef = {
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpassDesc = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorRef,
		};

		VkSubpassDependency subpassDep = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		};

		VkRenderPassCreateInfo renderPassCInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &attachmentDesc,
			.subpassCount = 1,
			.pSubpasses = &subpassDesc,
			.dependencyCount = 1,
			.pDependencies = &subpassDep,
		};

		if (!VKSucceed(vkCreateRenderPass(m_vkCtx.GetDevice(), &renderPassCInfo, m_vkAllocator, &m_swapchainRenderPass))) {
			return false;
		}
	}

	// Create framebuffer for each swapchain image.
	m_swapchainFramebuffers.resize(m_swapchainImageNum);
	for (int i = 0; i < m_swapchainImageNum; ++i) {
		VkFramebufferCreateInfo framebufferCInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = m_swapchainRenderPass,
			.attachmentCount = 1,
			.pAttachments = &m_swapchainImageViews[i],
			.width = rtWidth,
			.height = rtHeight,
			.layers = 1,
		};
		vkCreateFramebuffer(m_vkCtx.GetDevice(), &framebufferCInfo, m_vkAllocator, &m_swapchainFramebuffers[i]);
	}

	return true;
}

bool VKSwapchainContext::AcquireNextSwapchainImage(VkSemaphore imageAvailableSemaphore, uint64_t timeout /*= UINT64_MAX*/)
{
	VkResult res = vkAcquireNextImageKHR(m_vkCtx.GetDevice(), m_vkSwapchain, timeout, imageAvailableSemaphore, VK_NULL_HANDLE, &m_swapchainImageIdx);

	if (res == VK_SUCCESS) {
		return true;
	} else {
		assert(res == VK_TIMEOUT);
		return false;
	}
}

VKInflightContext::VKInflightContext(const VKDeviceContext& vkDeviceContext, int inflightFrameNum, const VkAllocationCallbacks* vkAllocator)
	: m_vkCtx(vkDeviceContext)
	, m_vkAllocator(vkAllocator)
	, m_inflightFrameNum(inflightFrameNum)
{}

VKInflightContext::~VKInflightContext()
{
	std::for_each(m_vkInflightFrameFences.begin(), m_vkInflightFrameFences.end(),
		[this](VkFence& fence)
		{
			vkDestroyFence(m_vkCtx.GetDevice(), fence, m_vkAllocator);
			fence = VK_NULL_HANDLE;
		});
	std::for_each(m_vkRenderFinishedSemaphores.begin(), m_vkRenderFinishedSemaphores.end(),
		[this](VkSemaphore& sema)
		{
			vkDestroySemaphore(m_vkCtx.GetDevice(), sema, m_vkAllocator);
			sema = VK_NULL_HANDLE;
		});
	std::for_each(m_vkImageAvailableSemaphores.begin(), m_vkImageAvailableSemaphores.end(),
		[this](VkSemaphore& sema)
		{
			vkDestroySemaphore(m_vkCtx.GetDevice(), sema, m_vkAllocator);
			sema = VK_NULL_HANDLE;
		});
}

bool VKInflightContext::InitInflight()
{
	// Create synchoronization objects.
	m_vkImageAvailableSemaphores.resize(m_inflightFrameNum);
	m_vkRenderFinishedSemaphores.resize(m_inflightFrameNum);
	m_vkInflightFrameFences.resize(m_inflightFrameNum);
	for (uint32_t inflightIdx = 0; inflightIdx < m_inflightFrameNum; ++inflightIdx)
	{
		// Create semaphore.
		VkSemaphoreCreateInfo semaphoreCInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		if (!VKSucceed(vkCreateSemaphore(m_vkCtx.GetDevice(), &semaphoreCInfo, m_vkAllocator, &m_vkImageAvailableSemaphores[inflightIdx]))) {
			return false;
		}
		if (!VKSucceed(vkCreateSemaphore(m_vkCtx.GetDevice(), &semaphoreCInfo, m_vkAllocator, &m_vkRenderFinishedSemaphores[inflightIdx]))) {
			return false;
		}

		// Create fence.
		VkFenceCreateInfo fenceCInfo = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,		// Initially signaled.
		};
		if (!VKSucceed(vkCreateFence(m_vkCtx.GetDevice(), &fenceCInfo, m_vkAllocator, &m_vkInflightFrameFences[inflightIdx]))) {
			return false;
		}
	}

	return true;
}

bool VKInflightContext::WaitForInflightFrameFence(uint64_t timeout /*= UINT64_MAX*/)
{
	if (vkWaitForFences(m_vkCtx.GetDevice(), 1, &m_vkInflightFrameFences[m_inflightFrameIdx], VK_TRUE, timeout) != VK_TIMEOUT) 
	{
		VKCall(vkResetFences(m_vkCtx.GetDevice(), 1, &m_vkInflightFrameFences[m_inflightFrameIdx]));
		return true;
	} else {
		return false;
	}
}

void VKInflightContext::NextFrame()
{
	// Move to next frame.
	m_inflightFrameIdx = (m_inflightFrameIdx + 1) % m_inflightFrameNum;
}


// Shader compile
const std::string GLSLCompilerCmd = "glslc";

// Compile shader
bool VKCompileShader(const std::string& shaderFilepath, const std::string& options, const std::string& outputFilepath)
{
	std::string command = GLSLCompilerCmd + " " + options + " " + shaderFilepath + " -o " + outputFilepath + ".spv";

	int result = system(command.c_str());
	if (result != 0) {
		std::cerr << "Failed to compile shader: " << shaderFilepath << std::endl;
		return false;
	}
	return true;
}

// VKShaderModule

//VKShaderModule::~VKShaderModule()
//{
//	if (m_shaderModule != VK_NULL_HANDLE) {
//		vkDestroyShaderModule(GetDevice(), m_shaderModule, GetAllocator());
//		m_shaderModule = VK_NULL_HANDLE;
//	}
//}
//
//bool VKShaderModule::CreateShaderModule(const std::string& filepath)
//{
//	// Load the compiled binary data.
//	std::string spvFilename = filepath + ".spv";
//	std::ifstream file(spvFilename, std::ios::ate | std::ios::binary);
//	if (!file) {
//		std::cerr << "Failed to open compiled file: " << spvFilename << std::endl;
//		return false;
//	}
//	size_t fileSize = (size_t)file.tellg();
//	std::vector<char> shaderCode(fileSize);
//	file.seekg(0);
//	file.read(shaderCode.data(), fileSize);
//	file.close();
//	if (!file) {
//		std::cerr << "Failed to read compiled file: " << spvFilename << std::endl;
//		return false;
//	}
//
//	// Create shader module.
//	VkShaderModuleCreateInfo shaderCInfo{};
//	shaderCInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
//	shaderCInfo.codeSize = shaderCode.size();
//	shaderCInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
//	if (vkCreateShaderModule(GetDevice(), &shaderCInfo, GetAllocator(), &m_shaderModule) != VK_SUCCESS) {
//		std::cerr << "failed to create shader module for: " << filepath << std::endl;
//		return false;
//	}
//
//	return true;
//}

bool VKCreateShaderModuleFromSPV(VkDevice vkDevice, const std::string& filepath, VkShaderModule& shaderModule)
{
	if (shaderModule != VK_NULL_HANDLE) {
		std::cerr << "Input shader module is not null." << std::endl;
		return false;
	}

	// Load the compiled binary data.
	std::string spvFilename = filepath;
	std::ifstream file(spvFilename, std::ios::ate | std::ios::binary);
	if (!file) {
		std::cerr << "Failed to open compiled file: " << spvFilename << std::endl;
		return false;
	}
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> shaderCode(fileSize);
	file.seekg(0);
	file.read(shaderCode.data(), fileSize);
	file.close();
	if (!file) {
		std::cerr << "Failed to read compiled file: " << spvFilename << std::endl;
		return false;
	}

	// Create shader module.
	VkShaderModuleCreateInfo shaderCInfo{};
	shaderCInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderCInfo.codeSize = shaderCode.size();
	shaderCInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

	if (vkCreateShaderModule(vkDevice, &shaderCInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		std::cerr << "failed to create shader module for: " << filepath << std::endl;
		return false;
	}

	return true;
}

// VKPipelineLayout

VKPipelineLayout::~VKPipelineLayout()
{
	if (m_vkPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(GetDevice(), m_vkPipelineLayout, GetAllocator());
		m_vkPipelineLayout = VK_NULL_HANDLE;
	}
}

bool VKPipelineLayout::CreatePipelineLayout(const VkPipelineLayoutCreateInfo& pplLayoutCInfo)
{
	assert(m_vkPipelineLayout == VK_NULL_HANDLE);

	if (!VKSucceed(vkCreatePipelineLayout(GetDevice(), &pplLayoutCInfo, GetAllocator(), &m_vkPipelineLayout))) {
		return false;
	}

	return true;
}

// VKGraphicsPipeline

VkPipelineColorBlendAttachmentState g_RGBColorAttachment = {
	.blendEnable = VK_FALSE,
	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
};

VkDynamicState g_DynamicStates_Viewport_Scissor[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
const uint32_t g_DynamicStateCount_Viewport_Scissor = sizeof(g_DynamicStates_Viewport_Scissor) / sizeof(VkDynamicState);

VKGraphicsPipeline::~VKGraphicsPipeline()
{
	if (m_vkPipeline) {
		vkDestroyPipeline(GetDevice(), m_vkPipeline, GetAllocator());
		m_vkPipeline = VK_NULL_HANDLE;
	}
}

bool VKGraphicsPipeline::ConfigureVertexStage(VkPipelineShaderStageCreateInfo& vertStageCInfo)
{
	assert((m_vkShaderStageFlagBits & VK_SHADER_STAGE_VERTEX_BIT) == 0); 
	return true;
}

bool VKGraphicsPipeline::ConfigFragmentStage(VkPipelineShaderStageCreateInfo& vertStageCInfo)
{
	assert((m_vkShaderStageFlagBits & VK_SHADER_STAGE_FRAGMENT_BIT) == 0); 
	return true;
}

void VKGraphicsPipeline::ConfigureVertexInputState(VkPipelineVertexInputStateCreateInfo& vertInputStateCInfo)
{
	// Defaultly no vertex inputs.
	vertInputStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr,
	};
}

void VKGraphicsPipeline::ConfigureInputAssemblyState(VkPipelineInputAssemblyStateCreateInfo& inputAssemblyStateCInfo)
{
	// Defaultly triangle list.
	inputAssemblyStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};
}

void VKGraphicsPipeline::ConfigureViewportState(VkPipelineViewportStateCreateInfo& viewportStateCInfo)
{
	// Defaultly use single dynamic viewport and scissor.
	viewportStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
}

void VKGraphicsPipeline::ConfigureRasterizationState(VkPipelineRasterizationStateCreateInfo& rasterizationStateCInfo)
{
	rasterizationStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f,
	};
}

void VKGraphicsPipeline::ConfigureMultisampleState(VkPipelineMultisampleStateCreateInfo& multisampleStateCInfo)
{
	// Default multisampling not enabled.
	multisampleStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};
}

void VKGraphicsPipeline::ConfigureColorBlendState(VkPipelineColorBlendStateCreateInfo& colorBlendStateCInfo)
{
	// Defaultly write to RGB, alpha blend not enabled.
	colorBlendStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY, // Optional
		.attachmentCount = 1,
		.pAttachments = &g_RGBColorAttachment,
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }, // Optional
	};
}

void VKGraphicsPipeline::ConfigureDynamicState(VkPipelineDynamicStateCreateInfo& dynamicStateCInfo)
{
	dynamicStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = g_DynamicStateCount_Viewport_Scissor,
		.pDynamicStates = g_DynamicStates_Viewport_Scissor,
	};
}

VkPipelineLayout VKGraphicsPipeline::GetPipelineLayout()
{
	// Defaultly return the global empty pipeline layout.
	return GetDeviceContext().GetGlobalEmptyPipelineLayout();
}

//void VKGraphicsPipeline::ConfigurePipelineLayoutCreateInfo(VkPipelineLayoutCreateInfo& pipelineLayoutCInfo) const
//{
//	// Default has no vertex input.
//	pipelineLayoutCInfo = {
//		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
//		.setLayoutCount = 0,
//		.pSetLayouts = nullptr,
//		.pushConstantRangeCount = 0,
//		.pPushConstantRanges = nullptr,
//	};
//}
//
//const VkPipelineLayout& VKGraphicsPipeline::GetPipelineLayout()
//{
//	// Create pipeline layout.
//	VkPipelineLayoutCreateInfo pipelineLayoutCInfo;
//	ConfigurePipelineLayoutCreateInfo(pipelineLayoutCInfo);
//
//	VKPipelineLayout pipelineLayout(GetDeviceContext());
//	if (!pipelineLayout.CreatePipelineLayout(pipelineLayoutCInfo)) {
//		return VK_NULL_HANDLE;
//	}
//
//
//
//	return ;
//}

// VKGraphicsPipeline

bool VKGraphicsPipeline::CreateGraphicsPipeline(VkRenderPass renderPass, uint32_t subpass)
{
	// First configure shader stages.

	int stageCnt = std::popcount(m_vkShaderStageFlagBits);
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages(stageCnt);
	int stageIdx = 0;

	// Vertex stage.
	if (m_vkShaderStageFlagBits & VK_SHADER_STAGE_VERTEX_BIT) {
		VkPipelineShaderStageCreateInfo& vertShaderStageCInfo = shaderStages[stageIdx];
		if (!ConfigureVertexStage(vertShaderStageCInfo)) {
			return false;
		}
		assert(vertShaderStageCInfo.stage == VK_SHADER_STAGE_VERTEX_BIT);
		++stageIdx;
	}

	// Fragement stage.
	if (m_vkShaderStageFlagBits & VK_SHADER_STAGE_FRAGMENT_BIT) {
		VkPipelineShaderStageCreateInfo& vertShaderStageCInfo = shaderStages[stageIdx];
		if (!ConfigFragmentStage(vertShaderStageCInfo)) {
			return false;
		}
		assert(vertShaderStageCInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT);
		++stageIdx;
	}
	assert(stageIdx == stageCnt);
	
	// Input state
	VkPipelineVertexInputStateCreateInfo vertexInputStateCInfo;
	ConfigureVertexInputState(vertexInputStateCInfo);

	// Input assemble
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCInfo;
	ConfigureInputAssemblyState(inputAssemblyStateCInfo);

	// Viewport
	VkPipelineViewportStateCreateInfo viewportStateCInfo;
	ConfigureViewportState(viewportStateCInfo);

	// Rasterization state.
	VkPipelineRasterizationStateCreateInfo rasterizationStateCInfo;
	ConfigureRasterizationState(rasterizationStateCInfo);

	// Multisampling state.
	VkPipelineMultisampleStateCreateInfo multisamplingStateCInfo;
	ConfigureMultisampleState(multisamplingStateCInfo);

	// Color blend state.
	VkPipelineColorBlendStateCreateInfo colorBlendStateCInfo;
	ConfigureColorBlendState(colorBlendStateCInfo);

	// Dynamic states.
	VkPipelineDynamicStateCreateInfo dynamicStateCInfo;
	ConfigureDynamicState(dynamicStateCInfo);

	// Create pipeline layout.
	//VkPipelineLayoutCreateInfo pipelineLayoutCInfo;
	//ConfigurePipelineLayoutCreateInfo(pipelineLayoutCInfo);
	//VKPipelineLayout pipelineLayout(GetDeviceContext());
	//if (!pipelineLayout.CreatePipelineLayout(pipelineLayoutCInfo)) {
	//	return false;
	//}

	VkPipelineLayout pipelineLayout = GetPipelineLayout();
	if (pipelineLayout == VK_NULL_HANDLE) {
		return false;
	}

	// Create graphics pipeline.
	VkGraphicsPipelineCreateInfo pipelineCInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)shaderStages.size(),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputStateCInfo,
		.pInputAssemblyState = &inputAssemblyStateCInfo,
		.pViewportState = &viewportStateCInfo,
		.pRasterizationState = &rasterizationStateCInfo,
		.pMultisampleState = &multisamplingStateCInfo,
		.pColorBlendState = &colorBlendStateCInfo,
		.pDynamicState = &dynamicStateCInfo,
		.layout = pipelineLayout,
		.renderPass = renderPass,
		.subpass = subpass,
	};
	if (!VKSucceed(vkCreateGraphicsPipelines(GetDevice(), VK_NULL_HANDLE, 1, &pipelineCInfo, GetAllocator(), &m_vkPipeline))) {
		return false;
	}
	VKSetDebugObjectName(GetDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_vkPipeline, GetName());

	return true;
}

// TestGraphicsPipeline

//TestGraphicsPipeline::~TestGraphicsPipeline()
//{}
//
//bool TestGraphicsPipeline::ConfigureVertexStage(VkPipelineShaderStageCreateInfo& vertStageCInfo)
//{
//	std::string vertShaderPath = "./shaders/test.vert";
//	if (!m_vertShaderModule.CreateShaderModule(vertShaderPath)) {
//		return false;
//	}
//
//	vertStageCInfo = {
//		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
//		.stage = VK_SHADER_STAGE_VERTEX_BIT,
//		.module = m_vertShaderModule,
//		.pName = "main",
//	};
//
//	return true;
//}
//
//bool TestGraphicsPipeline::ConfigFragmentStage(VkPipelineShaderStageCreateInfo& fragStageCInfo)
//{
//	std::string fragShaderPath = "./shaders/test.frag";
//	if (!m_fragShaderModule.CreateShaderModule(fragShaderPath)) {
//		return false;
//	}
//
//	fragStageCInfo = {
//		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
//		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
//		.module = m_fragShaderModule,
//		.pName = "main",
//	};
//
//	return true;
//}

bool VKCreateGraphicsPipelineTest(VkDevice device, VkRenderPass pass, uint32_t subpass, VkPipeline& pipeline)
{
	if (pipeline != VK_NULL_HANDLE) {
		std::cerr << "Input pipeline is not null." << std::endl;
		return false;
	}

	// Vertex stage.
	std::string vertShaderPath = "./shaders/test.vert.spv";
	VkShaderModule vertShaderModule = VK_NULL_HANDLE;
	if (!VKCreateShaderModuleFromSPV(device, vertShaderPath, vertShaderModule)) {
		return false;
	}
	OnScopeExit deleteVertShaderModule([=] { vkDestroyShaderModule(device, vertShaderModule, nullptr); });

	// Fragment stage.
	std::string fragShaderPath = "./shaders/test.frag.spv";
	VkShaderModule fragShaderModule = VK_NULL_HANDLE;
	if (!VKCreateShaderModuleFromSPV(device, fragShaderPath, fragShaderModule)) {
		return false;
	}
	OnScopeExit deleteFragShaderModule([=] { vkDestroyShaderModule(device, fragShaderModule, nullptr); });

	// First configure shader stages.
	VkPipelineShaderStageCreateInfo shaderStages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertShaderModule,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragShaderModule,
			.pName = "main",
		},
	};

	// Input state
	VkPipelineVertexInputStateCreateInfo vertexInputStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr,
	};

	// Input assemble
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	// Viewport
	VkPipelineViewportStateCreateInfo viewportStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	// Rasterization state.
	VkPipelineRasterizationStateCreateInfo rasterizationStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f,
	};

	// Multisampling state.
	VkPipelineMultisampleStateCreateInfo multisamplingStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	// Color blend state.
	VkPipelineColorBlendStateCreateInfo colorBlendStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY, // Optional
		.attachmentCount = 1,
		.pAttachments = &g_RGBColorAttachment,
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }, // Optional
	};

	// Dynamic states.
	VkPipelineDynamicStateCreateInfo dynamicStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = g_DynamicStateCount_Viewport_Scissor,
		.pDynamicStates = g_DynamicStates_Viewport_Scissor,
	};

	// Create pipeline layout.
	VkPipelineLayoutCreateInfo pipelineLayoutCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pushConstantRangeCount = 0,
	};
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	if (!VKSucceed(vkCreatePipelineLayout(device, &pipelineLayoutCInfo, nullptr, &pipelineLayout))) {
		std::cerr << "Failed to create pipeline for TestGraphicsPipeline." << std::endl;
	}
	OnScopeExit deletePipelineLayout([=] { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); });

	// Create graphics pipeline.
	VkGraphicsPipelineCreateInfo pipelineCInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)std::size(shaderStages),
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputStateCInfo,
		.pInputAssemblyState = &inputAssemblyStateCInfo,
		.pViewportState = &viewportStateCInfo,
		.pRasterizationState = &rasterizationStateCInfo,
		.pMultisampleState = &multisamplingStateCInfo,
		.pColorBlendState = &colorBlendStateCInfo,
		.pDynamicState = &dynamicStateCInfo,
		.layout = pipelineLayout,
		.renderPass = pass,
		.subpass = subpass,
	};
	if (!VKSucceed(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCInfo, nullptr, &pipeline))) {
		return false;
	}

	return true;
}