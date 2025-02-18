#include "vk_function.h"

VkResult g_vkLastResult = VK_RESULT_MAX_ENUM;

void VKPrintLastError()
{
	fprintf(stderr, "[%s:%u] Last error: %s.\n", __FILE__, __LINE__, string_VkResult(g_vkLastResult));
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

	if (VKFailed(vkCreateInstance(&instCInfo, allocator, &vkInst))) {
		VKPrintLastError();
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
	if (VKFailed(vkCreateDevice(vkPhysicalDevice, &deviceCInfo, vkAllocator, &vkDevice))) {
		VKPrintLastError();
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
	if (VKFailed(vkCreateDebugReportCallbackEXT(vkInst, &debugReportCallbackCInfo, nullptr, &vkDebugReportCallback))) {
		VKPrintLastError();
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

VKSwapchainContext::VKSwapchainContext(const VKDeviceContext& vkDeviceContext, const char* name, int swapchainImageNum, VkAllocationCallbacks* vkAllocator)
	: VKObject(vkDeviceContext, name, vkAllocator)
	, m_swapchainImageNum(swapchainImageNum)
{}

VKSwapchainContext::~VKSwapchainContext()
{
	std::for_each(m_swapchainFramebuffers.begin(), m_swapchainFramebuffers.end(),
		[this](VkFramebuffer& fb)
		{
			if (fb != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(GetDevice(), fb, GetAllocator());
				fb = VK_NULL_HANDLE;
			}
		});

	std::for_each(m_swapchainImageViews.begin(), m_swapchainImageViews.end(),
		[this](VkImageView imageView)
		{
			if (imageView) {
				vkDestroyImageView(GetDevice(), imageView, GetAllocator());
				imageView = VK_NULL_HANDLE;
			}
		});

	if (m_swapchainRenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(GetDevice(), m_swapchainRenderPass, GetAllocator());
		m_swapchainRenderPass = VK_NULL_HANDLE;
	}

	if (m_vkSwapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(GetDevice(), m_vkSwapchain, GetAllocator());
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
	if (VKFailed(vkCreateSwapchainKHR(GetDevice(), &vkSwapchainCInfo, GetAllocator(), &m_vkSwapchain))) {
		VKPrintLastError();
		return false;
	}

	m_swapchainRect = VkRect2D{ {0, 0}, {rtWidth, rtHeight} };

	// And create image view for each image in swapchain.
	uint32_t swapchainImageNum;
	VKCall(vkGetSwapchainImagesKHR(GetDevice(), m_vkSwapchain, &swapchainImageNum, nullptr));
	std::vector<VkImage> swapchainImages(swapchainImageNum);
	VKCall(vkGetSwapchainImagesKHR(GetDevice(), m_vkSwapchain, &swapchainImageNum, swapchainImages.data()));

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
		VKCall(vkCreateImageView(GetDevice(), &imageViewCInfo, GetAllocator(), &m_swapchainImageViews[i]));
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

		if (VKFailed(vkCreateRenderPass(GetDevice(), &renderPassCInfo, GetAllocator(), &m_swapchainRenderPass))) {
			VKPrintLastError();
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
		vkCreateFramebuffer(GetDevice(), &framebufferCInfo, GetAllocator(), &m_swapchainFramebuffers[i]);
	}

	return true;
}

bool VKSwapchainContext::AcquireNextSwapchainImage(VkSemaphore imageAvailableSemaphore, uint64_t timeout /*= UINT64_MAX*/)
{
	VkResult res = vkAcquireNextImageKHR(GetDevice(), m_vkSwapchain, timeout, imageAvailableSemaphore, VK_NULL_HANDLE, &m_swapchainImageIdx);

	if (res == VK_SUCCESS) {
		return true;
	} else {
		assert(res == VK_TIMEOUT);
		return false;
	}
}

VKInflightContext::VKInflightContext(const VKDeviceContext& vkDeviceContext, const char* name, int inflightFrameNum, VkAllocationCallbacks* vkAllocator)
	: VKObject(vkDeviceContext, name, vkAllocator)
	, m_inflightFrameNum(inflightFrameNum)
{}

VKInflightContext::~VKInflightContext()
{
	std::for_each(m_vkInflightFrameFences.begin(), m_vkInflightFrameFences.end(),
		[this](VkFence& fence)
		{
			vkDestroyFence(GetDevice(), fence, GetAllocator());
			fence = VK_NULL_HANDLE;
		});
	std::for_each(m_vkRenderFinishedSemaphores.begin(), m_vkRenderFinishedSemaphores.end(),
		[this](VkSemaphore& sema)
		{
			vkDestroySemaphore(GetDevice(), sema, GetAllocator());
			sema = VK_NULL_HANDLE;
		});
	std::for_each(m_vkImageAvailableSemaphores.begin(), m_vkImageAvailableSemaphores.end(),
		[this](VkSemaphore& sema)
		{
			vkDestroySemaphore(GetDevice(), sema, GetAllocator());
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
		if (VKFailed(vkCreateSemaphore(GetDevice(), &semaphoreCInfo, GetAllocator(), &m_vkImageAvailableSemaphores[inflightIdx]))) {
			VKPrintLastError();
			return false;
		}
		if (VKFailed(vkCreateSemaphore(GetDevice(), &semaphoreCInfo, GetAllocator(), &m_vkRenderFinishedSemaphores[inflightIdx]))) {
			VKPrintLastError();
			return false;
		}

		// Create fence.
		VkFenceCreateInfo fenceCInfo = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,		// Initially signaled.
		};
		if (VKFailed(vkCreateFence(GetDevice(), &fenceCInfo, GetAllocator(), &m_vkInflightFrameFences[inflightIdx]))) {
			VKPrintLastError();
			return false;
		}
	}

	return true;
}

bool VKInflightContext::WaitForInflightFrameFence(uint64_t timeout /*= UINT64_MAX*/)
{
	if (vkWaitForFences(GetDevice(), 1, &m_vkInflightFrameFences[m_inflightFrameIdx], VK_TRUE, timeout) != VK_TIMEOUT) {
		VKCall(vkResetFences(GetDevice(), 1, &m_vkInflightFrameFences[m_inflightFrameIdx]));
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

VKShaderModule::~VKShaderModule()
{
	if (m_shaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(GetDevice(), m_shaderModule, GetAllocator());
		m_shaderModule = VK_NULL_HANDLE;
	}
}

bool VKShaderModule::CreateShaderModule(const std::string& filepath)
{
	// Load the compiled binary data.
	std::string spvFilename = filepath + ".spv";
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
	if (vkCreateShaderModule(GetDevice(), &shaderCInfo, GetAllocator(), &m_shaderModule) != VK_SUCCESS) {
		std::cerr << "failed to create shader module for: " << filepath << std::endl;
		return false;
	}

	return true;
}

VKGraphicsPipeline::~VKGraphicsPipeline()
{
	if (m_vkPipeline) {
		vkDestroyPipeline(GetDevice(), m_vkPipeline, GetAllocator());
		m_vkPipeline = VK_NULL_HANDLE;
	}
}

void VKGraphicsPipeline::ConfigurePipelineLayoutCreateInfo(VkPipelineLayoutCreateInfo& pipelineLayoutCInfo) const
{
	pipelineLayoutCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pSetLayouts = nullptr,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};
}

// VKGraphicsPipeline

bool VKGraphicsPipeline::CreatePipeline(VkRenderPass renderPass, const std::string& vertShaderPath, const std::string& fragShaderPath)
{
	// Compile shader
	VKShaderModule vertShaderModule(GetDeviceContext());
	if (!vertShaderModule.CreateShaderModule(vertShaderPath)) {
		return false;
	}
	VKShaderModule fragShaderModule(GetDeviceContext());
	if (!fragShaderModule.CreateShaderModule(fragShaderPath)) {
		return false;
	}

	// Create pipeline layout.
	VkPipelineLayoutCreateInfo pipelineLayoutCInfo;
	ConfigurePipelineLayoutCreateInfo(pipelineLayoutCInfo);
	VkPipelineLayout pipelineLayout;
	if (vkCreatePipelineLayout(GetDevice(), &pipelineLayoutCInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		std::wcerr << L"Failed to create pipeline layout!";
		return false;
	}
	auto deleter = [vkDevice = GetDevice(), vkAllocator = GetAllocator()](VkPipelineLayout* ptr)
		{
			vkDestroyPipelineLayout(vkDevice, *ptr, vkAllocator);
		};
	std::unique_ptr<VkPipelineLayout, decltype(deleter)> pipelineLayoutPtr(&pipelineLayout, deleter);

	// Pipeline stages.
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	ConfigurePipelineShaderStageCreateInfos(shaderStages);

	// Create pipeline.
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	shaderStages.push_back(vertShaderStageInfo);
	shaderStages.push_back(fragShaderStageInfo);
	//VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Input state
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	// Input assemble
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	//VkViewport viewport = { 0, 0, rect.extent.width, rect.extent.height, 0.0f, 1.0f };
	//VkRect2D scissor = rect;
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	//viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	//viewportState.pScissors = &scissor;

	// Rasterization state.
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	// Multisampling state.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	// Dynamic states.
	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = (uint32_t)dynamicStates.size(),
		.pDynamicStates = dynamicStates.data(),
	};

	// 管线创建信息
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = pipelineLayout;  // 使用创建的 PipelineLayout
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	// 创建图形管线
	if (vkCreateGraphicsPipelines(GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_vkPipeline) != VK_SUCCESS) {
		std::wcerr << L"failed to create graphics pipeline!";
		return false;
	}

	VKSetDebugObjectName(GetDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_vkPipeline, GetName());

	return true;
}