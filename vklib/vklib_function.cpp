#include "vklib_function.h"

bool VKCheckError(VkResult result, bool throwException)
{
	if (result == VK_SUCCESS) {
		return true;
	} else {
		fprintf(stderr, "[%s:%u] Last error: %s.\n", __FILE__, __LINE__, string_VkResult(result));
		if (throwException) {
			throw std::runtime_error(string_VkResult(result));
		}
		return false;
	}
}

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

bool RegisterDebugReportCallback(VkInstance vkInst, PFN_vkDebugReportCallbackEXT vkDebugReportCallbackFunc, VkAllocationCallbacks* allocator)
{
	// Get the vkCreateDebugReportCallbackEXT function.
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(vkInst, "vkCreateDebugReportCallbackEXT");

	// Register debug report callback.
	VkDebugReportCallbackEXT vkDebugReportCallback = VK_NULL_HANDLE;
	VkDebugReportCallbackCreateInfoEXT debugReportCallbackCInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
		.pfnCallback = vkDebugReportCallbackFunc,
	};
	if (!VKSucceed(vkCreateDebugReportCallbackEXT(vkInst, &debugReportCallbackCInfo, allocator, &vkDebugReportCallback))) {
		return false;
	}

	return true;
}

bool VKCreateInstance(std::span<const char*> instExts, std::span<const char*> enableLayers, VkAllocationCallbacks* allocator, VkInstance* vkInstResultPtr)
{
	// Specify version version by ApplicationInfo.
	VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = VK_API_VERSION,
	};

	std::vector<const char*> instExts2(instExts.begin(), instExts.end());
	instExts2.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	// Create instance.
	VkInstanceCreateInfo instCInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = (uint32_t)enableLayers.size(),
		.ppEnabledLayerNames = enableLayers.data(),
		.enabledExtensionCount = (uint32_t)instExts2.size(),
		.ppEnabledExtensionNames = instExts2.data(),
	};

	if (!VKSucceed(vkCreateInstance(&instCInfo, allocator, vkInstResultPtr))) {
		return false;
	}

	return true;
}

bool VKPickSinglePhysicalDeviceAndQueueFamily(const VkInstance& vkInst, const VkSurfaceKHR& vkSurf, VkPhysicalDevice* vkPhysicalDeviceResult, uint32_t* vkQueueFamilyIdxResult)
{
	// Enumerate physical devices.
	uint32_t vkPhysicalDeviceCnt;
	VKCall(vkEnumeratePhysicalDevices(vkInst, &vkPhysicalDeviceCnt, nullptr));
	std::vector<VkPhysicalDevice> vkPhysicalDevices(vkPhysicalDeviceCnt);
	VKCall(vkEnumeratePhysicalDevices(vkInst, &vkPhysicalDeviceCnt, vkPhysicalDevices.data()));

	// Loop each physical device to select the suitable one; prefer discrete GPU.
	*vkPhysicalDeviceResult = VK_NULL_HANDLE;
	*vkQueueFamilyIdxResult = (uint32_t)-1;
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
			// Check if support the surface.
			if (vkSurf != VK_NULL_HANDLE) {
				VkBool32 supportSurf = VK_FALSE;
				VKCall(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIdx, vkSurf, &supportSurf));
				if (!supportSurf) {
					++queueFamilyIdx;
					continue;
				}
			}

			// Check if satisfy the required queue flags.
			const VkQueueFamilyProperties& props = queueFamilyProps[queueFamilyIdx];
			bool supportFunction = (props.queueFlags & reqFlags) == reqFlags;
			if (!supportFunction) {
				++queueFamilyIdx;
				continue;
			} 

			// Satisify all requires, found.
			break;
		}

		if (queueFamilyIdx == queueFamilyPropCnt) {
			// No queue family of the current physical deivce meet the requirements, try next.
			continue;
		}

		//Found.
		*vkPhysicalDeviceResult = physicalDevice;
		*vkQueueFamilyIdxResult = queueFamilyIdx;

		// Prefer discrete device.
		if (vkPhysicalDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			break;
		}
	}

	return true;
}

bool VKCreateDevice(
	VkDevice& vkDevice,
	VkPhysicalDevice vkPhysicalDevice,
	size_t queueCreateRequestCount, const VKDeviceQueueCreateRequest* queueCreateRequests,
	size_t enableLayerCount, const char** enableLayerNames,
	size_t enableExtensionCount, const char** enableExtensionNames,
	const void* featureLinkedList, const VkAllocationCallbacks* vkAllocator)
{
	size_t queueFamilyCnt = queueCreateRequestCount;
	std::vector<VkDeviceQueueCreateInfo> deviceQueueCInfos(queueFamilyCnt);
	for (size_t i = 0; i < queueFamilyCnt; ++i) {
		const VKDeviceQueueCreateRequest& request = queueCreateRequests[i];
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
		.enabledLayerCount = (uint32_t)enableLayerCount,
		.ppEnabledLayerNames = enableLayerNames,
		.enabledExtensionCount = (uint32_t)enableExtensionCount,
		.ppEnabledExtensionNames = enableExtensionNames,
	};

	vkDevice = VK_NULL_HANDLE;
	if (!VKSucceed(vkCreateDevice(vkPhysicalDevice, &deviceCInfo, vkAllocator, &vkDevice))) {
		return false;
	}

	return true;
}

bool VKCreateDevice(
	VkPhysicalDevice vkPhysicalDevice, 
	std::span<const VKDeviceQueueCreateRequest> queueCreateRequests, 
	std::span<const char*> enableLayerNames, 
	std::span<const char*> enableExtensionNames, 
	VkAllocationCallbacks* vkAllocator,
	VkDevice* vkDevicePtr)
{
	// Make the VkDeviceQueueCreateInfo array and VkDeviceCreateInfo.
	size_t queueFamilyCnt = queueCreateRequests.size();
	std::vector<VkDeviceQueueCreateInfo> deviceQueueCInfos(queueFamilyCnt);
	for (size_t i = 0; i < queueFamilyCnt; ++i) {
		const VKDeviceQueueCreateRequest& request = queueCreateRequests[i];
		deviceQueueCInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = request.QueueFamilyIdx,
			.queueCount = request.QueueCount,
			.pQueuePriorities = request.QueuePriorities.data(),
		};
	}

	VkDeviceCreateInfo deviceCInfo = {
		.sType{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO },
		.pNext = nullptr,
		.queueCreateInfoCount = (uint32_t)deviceQueueCInfos.size(),
		.pQueueCreateInfos = deviceQueueCInfos.data(),
		.enabledLayerCount = (uint32_t)enableLayerNames.size(),
		.ppEnabledLayerNames = enableLayerNames.data(),
		.enabledExtensionCount = (uint32_t)enableExtensionNames.size(),
		.ppEnabledExtensionNames = enableExtensionNames.data(),
	};

	// Create the device.
	if (!VKSucceed(vkCreateDevice(vkPhysicalDevice, &deviceCInfo, vkAllocator, vkDevicePtr))) {
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

void VKPrintPhysicalDeviceName(const VkPhysicalDevice& vkPhysicalDevice)
{
	VkPhysicalDeviceProperties vkPhysicalDeviceProps;
	vkGetPhysicalDeviceProperties(vkPhysicalDevice, &vkPhysicalDeviceProps);
	std::cout << vkPhysicalDeviceProps.deviceName << std::endl;
}

void VKPrintPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice& vkPhysicalDevice)
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

void VKPrintPhysicalDeviceFeatures(const VkPhysicalDevice& vkPhysicalDevice)
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

bool VKPickSurfaceFormat(VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR* vkSurfFmtResult)
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
		*vkSurfFmtResult = preferSurfFmt;
		return true;
	}

	// Then try to find R8G8B8A8.
	VkSurfaceFormatKHR preferSurfFmt2 = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	if (std::find_if(supportedSurfFmts.begin(), supportedSurfFmts.end(), [toFindFmt = preferSurfFmt2](const VkSurfaceFormatKHR& fmt)->bool
		{
			return fmt.format == toFindFmt.format && fmt.colorSpace == toFindFmt.colorSpace;
		}) != supportedSurfFmts.end())
	{
		*vkSurfFmtResult = preferSurfFmt;
		return true;
	}

	// Can't find the prefered format.
	return false;
}


const std::string GLSLCompilerCmd = "glslc";

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


// Input state
const VkPipelineVertexInputStateCreateInfo g_VertexInputStateCreateInfo_NoVertexInput = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	.vertexBindingDescriptionCount = 0,
	.pVertexBindingDescriptions = nullptr,
	.vertexAttributeDescriptionCount = 0,
	.pVertexAttributeDescriptions = nullptr,
};

// Input assemble
const VkPipelineInputAssemblyStateCreateInfo g_InputAssemblyStateCreateInfo_TriangleList = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
};

// Viewport
const VkPipelineViewportStateCreateInfo g_ViewportStateCreateInfo_DynamicViewportScissor = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	.viewportCount = 1,
	.scissorCount = 1,
};

// Rasterization state.
const VkPipelineRasterizationStateCreateInfo g_RasterizationStateCreateInfo_Default = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	.depthClampEnable = VK_FALSE,
	.rasterizerDiscardEnable = VK_FALSE,
	.polygonMode = VK_POLYGON_MODE_FILL,
	.cullMode = VK_CULL_MODE_NONE,
	.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
	.depthBiasEnable = VK_FALSE,
	.depthBiasConstantFactor = 0.0f,
	.depthBiasClamp = 0.0f,
	.depthBiasSlopeFactor = 0.0f,
	.lineWidth = 1.0f,
};

const VkPipelineMultisampleStateCreateInfo g_MultisamplingStateCreateInfo_DisableMultisample = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	.sampleShadingEnable = VK_FALSE,
	.minSampleShading = 1.0f,
	.pSampleMask = nullptr,
	.alphaToCoverageEnable = VK_FALSE,
	.alphaToOneEnable = VK_FALSE,
};

const VkPipelineColorBlendAttachmentState g_RGBColorAttachment = {
	.blendEnable = VK_FALSE,
	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
};
const VkPipelineColorBlendStateCreateInfo g_ColorBlendStateCreateInfo_RGB_DisableBlend = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	.logicOpEnable = VK_FALSE,
	.logicOp = VK_LOGIC_OP_COPY, // Optional
	.attachmentCount = 1,
	.pAttachments = &g_RGBColorAttachment,
	.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }, // Optional
};


// Dynamic states.
const VkDynamicState g_DynamicStates_Viewport_Scissor[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
const VkPipelineDynamicStateCreateInfo g_DynamicStateCreateInfo_DynamicViewportScissor = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	.dynamicStateCount = (uint32_t)std::size(g_DynamicStates_Viewport_Scissor),
	.pDynamicStates = g_DynamicStates_Viewport_Scissor,
};




// Destroy helper functions.

// Destroy vulkan semaphore vector.
void VKDestroySemaphoreVector(VkDevice device, std::vector<VkSemaphore>& semaphores, VkAllocationCallbacks* allocator)
{
	std::for_each(semaphores.begin(), semaphores.end(),
		[device, allocator](VkSemaphore sema)
		{
			if (sema != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, sema, allocator);
			}
		}
	);
	semaphores.clear();
}

// Destroy vulkan fence vector.
void VKDestroyFenceVector(VkDevice device, std::vector<VkFence>& fences, VkAllocationCallbacks* allocator)
{
	std::for_each(fences.begin(), fences.end(),
		[device, allocator](VkFence fence)
		{
			if (fence != VK_NULL_HANDLE) {
				vkDestroyFence(device, fence, allocator);
			}
		}
	);
	fences.clear();
}

void VKDestroyImageViewVector(VkDevice device, std::vector<VkImageView>& imageViews, VkAllocationCallbacks* allocator)
{
	std::for_each(imageViews.begin(), imageViews.end(),
		[=](VkImageView imageView)
		{
			if (imageView != VK_NULL_HANDLE) {
				vkDestroyImageView(device, imageView, allocator);
			}
		}
	);
	imageViews.clear();
}
