#include "vklib.h"

// VKContext 

//VKContext::VKContext(const std::shared_ptr<VKContext>& parent, const char* ctxName)
//	: m_parentContext(parent)
//	, m_contextName(ctxName)
//{}
//
//const char* VKContext::GetContextName() const
//{
//	return m_contextName;
//}
//
//VKContext* VKContext::GetParentContext() const
//{
//	return m_parentContext.get();
//}

// 

VKDeviceContext::VKDeviceContext(VkInstance instance, VkPhysicalDevice physicalDevice, const char* ctxName)
	: m_vkContextName(ctxName)
	, m_vkInstance(instance)
	, m_vkPhysicalDevice(physicalDevice)
{
}

VKDeviceContext::~VKDeviceContext()
{
	if (m_vkEmptyPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(m_vkDevice, m_vkEmptyPipelineLayout, nullptr);
	}

	if (m_VmaAllocator != VK_NULL_HANDLE) {
		vmaDestroyAllocator(m_VmaAllocator);
	}

	if (m_vkDevice != VK_NULL_HANDLE) {
		vkDestroyDevice(m_vkDevice, nullptr);
	}
}

bool VKDeviceContext::InitDeviceContext(size_t queueCreateQuestCnt, const VKDeviceQueueCreateRequest* queueCreateQuests, size_t layerCnt, const char** layers, size_t extensionCnt, const char** extensions)
{
	// Create the vulkan device.
	//if (!VKCreateDevice(m_vkDevice, m_vkPhysicalDevice, queueCreateQuestCnt, queueCreateQuests, layerCnt, layers, extensionCnt, extensions)) {
	//	return false;
	//}

	//if (!VKCreateDevice(
	//		m_vkPhysicalDevice, 
	//	std::span<VKDeviceQueueCreateRequest>(queueCreateQuests, queueCreateQuestCnt), 
	//	std::span<const char*>(layers, layerCnt),
	//	std::span<const char*>(extensions, extensionCnt), &m_device) {
	//	return false;
	//}

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

	// Initialize VMA Allocator.
	VmaAllocatorCreateInfo allocatorInfo = {
		.flags = 0,
		.physicalDevice = m_vkPhysicalDevice,
		.device = m_vkDevice,
		.instance = m_vkInstance,
		.vulkanApiVersion = VK_API_VERSION_1_3,
	};
	if (!VKSucceed(vmaCreateAllocator(&allocatorInfo, &m_VmaAllocator))) {
		return false;
	}

	// Create global objects.
	// 
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

VKSwapchainContext::VKSwapchainContext(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue)
	: m_device(device), m_queueFamilyIndex(queueFamilyIndex), m_queue(queue)
{}

VKSwapchainContext::~VKSwapchainContext()
{
	std::for_each(m_swapchainFramebuffers.begin(), m_swapchainFramebuffers.end(),
		[this](VkFramebuffer& fb)
		{
			if (fb != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(m_device, fb, m_vkAllocator);
				fb = VK_NULL_HANDLE;
			}
		});

	std::for_each(m_swapchainImageViews.begin(), m_swapchainImageViews.end(),
		[this](VkImageView imageView)
		{
			if (imageView) {
				vkDestroyImageView(m_device, imageView, m_vkAllocator);
				imageView = VK_NULL_HANDLE;
			}
		});

	if (m_swapchainRenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(m_device, m_swapchainRenderPass, m_vkAllocator);
		m_swapchainRenderPass = VK_NULL_HANDLE;
	}

	if (m_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_device, m_swapchain, m_vkAllocator);
		m_swapchain = VK_NULL_HANDLE;
	}
}

bool VKSwapchainContext::InitSwapchain(VkSurfaceKHR surf, VkSurfaceFormatKHR surfFormat, VkExtent2D surfExt, uint32_t swapchainImageMinCount)
{
	m_surfaceFormat = surfFormat;
	m_swapchainRect = VkRect2D{ {0,0}, surfExt };

	// Create the swapchain.
	VkSwapchainCreateInfoKHR vkSwapchainCInfo = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surf,
			.minImageCount = (uint32_t)m_swapchainImageNum,
			.imageFormat = m_surfaceFormat.format,
			.imageColorSpace = m_surfaceFormat.colorSpace,
			.imageExtent = surfExt,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 1,
			.pQueueFamilyIndices = &m_queueFamilyIndex,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_FIFO_KHR,
			.clipped = VK_TRUE,
	};
	if (!VKSucceed(vkCreateSwapchainKHR(m_device, &vkSwapchainCInfo, m_vkAllocator, &m_swapchain))) {
		return false;
	}

	// Obtain images from the swapchain.
	if (!VKSucceed(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageNum, nullptr))) {
		return false;
	}
	m_swapchainImages.resize(m_swapchainImageNum);
	if (!VKSucceed(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageNum, m_swapchainImages.data()))) {
		return false;
	}

	// And create image view for each image in swapchain.
	assert(m_swapchainImageViews.empty());
	m_swapchainImageViews.resize(m_swapchainImageNum);
	for (int i = 0; i < (int)m_swapchainImageNum; ++i) {
		VkImageViewCreateInfo imageViewCInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_surfaceFormat.format,
			.components = VkComponentMapping {},
			.subresourceRange = VkImageSubresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		if (!VKSucceed(vkCreateImageView(m_device, &imageViewCInfo, m_vkAllocator, &m_swapchainImageViews[i]))) {
			return false;
		}
	}

	// Create framebuffer for each swapchain image.
	m_swapchainFramebuffers.resize(m_swapchainImageNum);
	for (uint32_t i = 0; i < m_swapchainImageNum; ++i) {
		VkFramebufferCreateInfo framebufferCInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = m_swapchainRenderPass,
			.attachmentCount = 1,
			.pAttachments = &m_swapchainImageViews[i],
			.width = m_swapchainRect.extent.width,
			.height = m_swapchainRect.extent.height,
			.layers = 1,
		};
		vkCreateFramebuffer(m_device, &framebufferCInfo, m_vkAllocator, &m_swapchainFramebuffers[i]);
	}

	return true;
}

bool VKSwapchainContext::AcquireNextImage(VkSemaphore imageAvailableSemaphore, uint64_t timeout)
{
	VkResult res = vkAcquireNextImageKHR(m_device, m_swapchain, timeout, imageAvailableSemaphore, VK_NULL_HANDLE, &m_currSwapchainImageIndex);

	switch (res)
	{
	case VK_NOT_READY:
		return true;
	case VK_SUCCESS:
		return true;
	case VK_SUBOPTIMAL_KHR:
		return true;
	case VK_TIMEOUT:
		return false;
	case VK_ERROR_OUT_OF_DATE_KHR:
		return true;
	default:
		return false;
	}

	if (res == VK_SUCCESS) {
		return true;
	} else {
		assert(res == VK_TIMEOUT);
		return false;
	}
}

bool VKSwapchainContext::Present(const std::span<VkSemaphore>& waitSemaphores)
{
	VkResult presentRes = VK_SUCCESS;
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = (uint32_t)waitSemaphores.size(),
		.pWaitSemaphores = waitSemaphores.data(),
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain,
		.pImageIndices = &m_currSwapchainImageIndex,
		.pResults = &presentRes,
	};
	if (!VKSucceed(vkQueuePresentKHR(m_queue, &presentInfo)) || !VKSucceed(presentRes)) {
		return false;
	}

	return true;
}

// VKInflightContext

VKInflightContext::VKInflightContext(const VKDeviceContext& vkDeviceContext, int inflightFrameNum, const VkAllocationCallbacks* vkAllocator)
	: m_vkCtx(vkDeviceContext)
	, m_vkAllocator(vkAllocator)
	, m_inflightFrameNum(inflightFrameNum)
{
}

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


// VKInflightContext

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

// VKSingleQueueDeviceContext

VKSingleQueueDeviceContext::VKSingleQueueDeviceContext(VkInstance vkInst, VkPhysicalDevice physDevice, uint32_t queueFamilyIdx)
	: m_instance(vkInst)
	, m_physicalDevice(physDevice)
	, m_queueFamilyIdx(queueFamilyIdx)
{
	// Initialized with a drawable window.	
	assert(m_instance != VK_NULL_HANDLE);		
}

bool VKSingleQueueDeviceContext::InitializeDevice(std::span<const char*> layers, std::span<const char*> extensions)
{
	// Create the device.
	std::vector<VKDeviceQueueCreateRequest> vkDeviceQueueCreateRequest{ {m_queueFamilyIdx, 1, {1.0f}} };
	//std::vector<const char*> enabledLayerNames{};
	//std::vector<const char*> enabledExtensionNames{};
	if (!VKCreateDevice(m_physicalDevice, vkDeviceQueueCreateRequest, layers, extensions, nullptr, &m_device)) {
		return false;
	}

	// Get the DeviceQueue.
	vkGetDeviceQueue(m_device, m_queueFamilyIdx, 0, &m_queue);
		
	// Initialize VMA Allocator.


	return true;
}

void VKSingleQueueDeviceContext::DestroySwapchain()
{
	// Must first destroy framebuffers,
	VKDestroyFramebufferVector(m_device, m_swapchainFramebuffers, m_allocator);

	//	then image views,
	VKDestroyImageViewVector(m_device, m_swapchainImageViews, m_allocator);

	//	and last the swapchain itself.
	if (m_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_device, m_swapchain, m_allocator);
		m_swapchain = VK_NULL_HANDLE;
	}
}

bool VKSingleQueueDeviceContext::InitializeSwapchainContext(VkSurfaceKHR surf, const VkSurfaceFormatKHR& surfFmt, const VkExtent2D& swapchainExtent, uint32_t swapchainMinImageCount)
{
	assert(surf != VK_NULL_HANDLE);
	m_surface = surf;
	m_surfaceFormat = surfFmt;
	m_swapchainExtent = swapchainExtent;

	assert(m_swapchain == VK_NULL_HANDLE);
	assert(m_swapchainImageViews.empty());	

	// Create swapchain.
	VkSwapchainCreateInfoKHR vkSwapchainCInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_surface,
		.minImageCount = swapchainMinImageCount,
		.imageFormat = m_surfaceFormat.format,
		.imageColorSpace = m_surfaceFormat.colorSpace,
		.imageExtent = m_swapchainExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &m_queueFamilyIdx,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.clipped = VK_FALSE,	// Intended false, to measure actual performance.
		.oldSwapchain = VK_NULL_HANDLE,
	};
	if (!VKSucceed(vkCreateSwapchainKHR(m_device, &vkSwapchainCInfo, nullptr, &m_swapchain))) {
		LogError("Failed to create VkSwapchain.");
		return false;
	}

	// Create image view for each image in swapchain.
	uint32_t swapchainImageNum{ ~0u };
	if (!VKSucceed(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageNum, nullptr))) {
		LogError("Failed to get swapchain image count.");
		return false;
	}
	m_swapchainImages.resize(swapchainImageNum, VK_NULL_HANDLE);
	if (!VKSucceed(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageNum, m_swapchainImages.data()))) {
		LogError("Failed to get swapchain images.");
		return false;
	}
	
	// Create an VKImageView for each swapchain image.
	m_swapchainImageViews.resize(swapchainImageNum);
	for (uint32_t swapchainImageIdx = 0; swapchainImageIdx < swapchainImageNum; ++swapchainImageIdx)
	{
		VkImageViewCreateInfo imageViewCInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_swapchainImages[swapchainImageIdx],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_surfaceFormat.format,
			.components = VkComponentMapping{},
			.subresourceRange = VkImageSubresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		if (!VKSucceed(vkCreateImageView(m_device, &imageViewCInfo, nullptr, &m_swapchainImageViews[swapchainImageIdx]))) {
			return false;
		}
	}

	// Create the VkRenderPass draw into the swapchain images.
	{
		RuntimeCheckError(m_swapchainRenderPass == VK_NULL_HANDLE);

		VkAttachmentDescription colorAttachment{
			.flags{0},
			.format{m_surfaceFormat.format},
			.samples{VK_SAMPLE_COUNT_1_BIT},
			.loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR},
			.storeOp{VK_ATTACHMENT_STORE_OP_STORE},
			.stencilLoadOp{VK_ATTACHMENT_LOAD_OP_DONT_CARE},
			.stencilStoreOp{VK_ATTACHMENT_STORE_OP_DONT_CARE},
			.initialLayout{VK_IMAGE_LAYOUT_UNDEFINED},
			.finalLayout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
		};
		VkAttachmentReference colorAttachmentRef = {
			.attachment{0},
			.layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Only one sub pass.
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// Create the back buffer pass.
		VkRenderPassCreateInfo renderPassCInfo = {};
		renderPassCInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCInfo.attachmentCount = 1;
		renderPassCInfo.pAttachments = &colorAttachment;
		renderPassCInfo.subpassCount = 1;
		renderPassCInfo.pSubpasses = &subpass;
		renderPassCInfo.dependencyCount = 1;
		renderPassCInfo.pDependencies = &dependency;
		if (!VKSucceed(vkCreateRenderPass(m_device, &renderPassCInfo, nullptr, &m_swapchainRenderPass))) {
			return false;
		}
	}

	// Create the swapchain framebuffers.	
	size_t swapchainFramebufferNum = m_swapchainImageViews.size();
	m_swapchainFramebuffers.resize(swapchainFramebufferNum);
	for (uint32_t i = 0; i < swapchainFramebufferNum; ++i) {
		VkFramebufferCreateInfo framebufferCInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = m_swapchainRenderPass,
			.attachmentCount = 1,
			.pAttachments = &m_swapchainImageViews[i],
			.width = m_swapchainExtent.width,
			.height = m_swapchainExtent.height,
			.layers = 1,
		};
		if (!VKSucceed(vkCreateFramebuffer(m_device, &framebufferCInfo, nullptr, &m_swapchainFramebuffers[i]))) {
			return false;
		}
	}

	return true;
}

bool VKSingleQueueDeviceContext::InitializeInflightContext(uint32_t inflightFrameNum)
{
	// Check that all synchoronization objects should not be initialized.
	RuntimeCheckError(m_imageAvailableSemaphores.empty());
	RuntimeCheckError(m_renderFinishedSemaphores.empty());
	RuntimeCheckError(m_renderFinishedFences.empty());

	m_inflightFrameNum = inflightFrameNum;

	// Create inflight semaphores and fences.
	m_imageAvailableSemaphores.resize(m_inflightFrameNum);
	m_renderFinishedSemaphores.resize(m_inflightFrameNum);
	m_renderFinishedFences.resize(m_inflightFrameNum);
	for (uint32_t i = 0; i < m_inflightFrameNum; ++i)
	{
		VkSemaphoreCreateInfo semaphoreCInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};

		if (!VKSucceed(vkCreateSemaphore(m_device, &semaphoreCInfo, m_allocator, &m_imageAvailableSemaphores[i]))) {
			return false;
		}

		if (!VKSucceed(vkCreateSemaphore(m_device, &semaphoreCInfo, m_allocator, &m_renderFinishedSemaphores[i]))) {
			return false;
		}

		VkFenceCreateInfo fenceCInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		if (!VKSucceed(vkCreateFence(m_device, &fenceCInfo, m_allocator, &m_renderFinishedFences[i]))) {
			return false;
		}
	}

	return true;
}

bool VKSingleQueueDeviceContext::CreateGraphicsQueueCommandPool(VkCommandPoolCreateFlags flags, VKCommandPoolContextUniquePtr* cmdPoolCtx)
{
	assert(cmdPoolCtx != nullptr);

	VkCommandPoolCreateInfo cmdPoolCInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = flags,
		.queueFamilyIndex = m_queueFamilyIdx,
	};

	VkCommandPool commandPool = VK_NULL_HANDLE;
	if (!VKSucceed(vkCreateCommandPool(m_device, &cmdPoolCInfo, m_allocator, &commandPool))) {
		return false;
	}
	
	*cmdPoolCtx = std::make_unique<VKCommandPoolContext>(m_device, m_queueFamilyIdx, m_queue, commandPool, m_allocator);

	return true;
}

bool VKSingleQueueDeviceContext::AdvanceFrame(InflightState* inflightFrameInfo)
{
	size_t nextInflightFrameIdx = (m_inflightFrameIdx + 1) % m_inflightFrameNum;
	uint64_t timeoutNano = 10;
	VkResult res{};

	// Wait for the previous inflight render finish fences. Can't timeout, as we input a infinite timeout parameter.
	res = vkWaitForFences(m_device, 1, &m_renderFinishedFences[nextInflightFrameIdx], VK_TRUE, timeoutNano);
	if (res != VK_SUCCESS)
	{
		assert(res == VK_TIMEOUT);
		return false;
	}

	// Acqure the swapchiain image.
	VkResult acqSwapchainImageRes = vkAcquireNextImageKHR(m_device, m_swapchain, timeoutNano, m_imageAvailableSemaphores[nextInflightFrameIdx], VK_NULL_HANDLE, &m_currSwapchainImageIndex);
	if (acqSwapchainImageRes != VK_SUCCESS) {
		assert(acqSwapchainImageRes == VK_NOT_READY);		
		return false;
	}	

	// So it's ready to begin the frame.
	VKCall(vkResetFences(m_device, 1, &m_renderFinishedFences[nextInflightFrameIdx]));

	m_inflightFrameIdx = nextInflightFrameIdx;

	*inflightFrameInfo = {
		.SwapchainImageIndex{ m_currSwapchainImageIndex },
		.SwapchainImage{ m_swapchainImages[m_currSwapchainImageIndex] },
		.SwapchainFramebuffer{ m_swapchainFramebuffers[m_currSwapchainImageIndex] },
		.InflightFrameIndex{ m_inflightFrameIdx },
		.InflightImageAvailableSemaphore{ m_imageAvailableSemaphores[m_inflightFrameIdx] },
		.InflightRenderFinishedSemaphore{ m_renderFinishedSemaphores[m_inflightFrameIdx] },
		.InflightRenderFinishedFence{ m_renderFinishedFences[m_inflightFrameIdx] },
	};

	return true;
}

void VKSingleQueueDeviceContext::EndFrame(const std::span<VkCommandBuffer>& cmdBufs)
{
	// Submit the command buffer.
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_imageAvailableSemaphores[m_inflightFrameIdx],
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = (uint32_t)cmdBufs.size(),
		.pCommandBuffers = cmdBufs.data(),
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_renderFinishedSemaphores[m_inflightFrameIdx],
	};
	VKCall(vkQueueSubmit(m_queue, 1, &submitInfo, m_renderFinishedFences[m_inflightFrameIdx]));
}

bool VKSingleQueueDeviceContext::Present(const std::span<VkSemaphore>& waitSemaphores)
{
	VkResult presentRes = VK_SUCCESS;
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = (uint32_t)waitSemaphores.size()	,
		.pWaitSemaphores = waitSemaphores.data(),
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain,
		.pImageIndices = &m_currSwapchainImageIndex,
		.pResults = &presentRes,
	};
	if (!VKSucceed(vkQueuePresentKHR(m_queue, &presentInfo)) || !VKSucceed(presentRes)) {
		return false;
	}

	return true;
}

bool VKSingleQueueDeviceContext::DeviceWaitIdle()
{
	if (!VKSucceed(vkDeviceWaitIdle(m_device))) {
		return false;
	}
	return true;
}

VKSingleQueueDeviceContext::~VKSingleQueueDeviceContext()
{
	// Destroy inflight objects.
	VKDestroySemaphoreVector(m_device, m_imageAvailableSemaphores, m_allocator);
	VKDestroySemaphoreVector(m_device, m_renderFinishedSemaphores, m_allocator);
	VKDestroyFenceVector(m_device, m_renderFinishedFences, m_allocator);

	DestroySwapchain();

	if (m_swapchainRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(m_device, m_swapchainRenderPass, m_allocator);
	}

	if (m_device != VK_NULL_HANDLE) 
	{
		vkDestroyDevice(m_device, m_allocator);
	}
}

// RealtimeRenderingContext --------------------------------------------------------------------------------------------------------------

VKRealtimeRenderingContext::VKRealtimeRenderingContext(VkInstance instance, VkPhysicalDevice& physicalDevice, VkDevice device, uint32_t queueFamilyIndex, VkQueue queue) : m_instance(instance)
, m_physicalDevice(physicalDevice)
, m_device(device)
, m_queueFamilyIdx(queueFamilyIndex)
, m_queue(queue)
{
}

bool VKRealtimeRenderingContext::InitializeSwapchainContext(VkSurfaceKHR surf, const VkSurfaceFormatKHR& surfFmt, const VkExtent2D& swapchainExtent, uint32_t swapchainMinImageCount /*= 3*/)
{
	assert(surf != VK_NULL_HANDLE);
	m_surface = surf;
	m_surfaceFormat = surfFmt;
	m_swapchainExtent = swapchainExtent;

	assert(m_swapchain == VK_NULL_HANDLE);
	assert(m_swapchainImageViews.empty());

	// Create swapchain.
	VkSwapchainCreateInfoKHR vkSwapchainCInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_surface,
		.minImageCount = swapchainMinImageCount,
		.imageFormat = m_surfaceFormat.format,
		.imageColorSpace = m_surfaceFormat.colorSpace,
		.imageExtent = m_swapchainExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &m_queueFamilyIdx,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.clipped = VK_FALSE,	// Intended false, to measure actual performance.
		.oldSwapchain = VK_NULL_HANDLE,
	};
	if (!VKSucceed(vkCreateSwapchainKHR(m_device, &vkSwapchainCInfo, nullptr, &m_swapchain))) {
		LogError("Failed to create VkSwapchain.");
		return false;
	}

	// Create image view for each image in swapchain.
	uint32_t swapchainImageNum{ ~0u };
	if (!VKSucceed(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageNum, nullptr))) {
		LogError("Failed to get swapchain image count.");
		return false;
	}
	m_swapchainImages.resize(swapchainImageNum, VK_NULL_HANDLE);
	if (!VKSucceed(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageNum, m_swapchainImages.data()))) {
		LogError("Failed to get swapchain images.");
		return false;
	}

	// Create an VKImageView for each swapchain image.
	m_swapchainImageViews.resize(swapchainImageNum);
	for (uint32_t swapchainImageIdx = 0; swapchainImageIdx < swapchainImageNum; ++swapchainImageIdx)
	{
		VkImageViewCreateInfo imageViewCInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_swapchainImages[swapchainImageIdx],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_surfaceFormat.format,
			.components = VkComponentMapping{},
			.subresourceRange = VkImageSubresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		if (!VKSucceed(vkCreateImageView(m_device, &imageViewCInfo, nullptr, &m_swapchainImageViews[swapchainImageIdx]))) {
			return false;
		}
	}

	// Create the VkRenderPass draw into the swapchain images.
	{
		RuntimeCheckError(m_swapchainRenderPass == VK_NULL_HANDLE);

		VkAttachmentDescription colorAttachment{
			.flags{0},
			.format{m_surfaceFormat.format},
			.samples{VK_SAMPLE_COUNT_1_BIT},
			.loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR},
			.storeOp{VK_ATTACHMENT_STORE_OP_STORE},
			.stencilLoadOp{VK_ATTACHMENT_LOAD_OP_DONT_CARE},
			.stencilStoreOp{VK_ATTACHMENT_STORE_OP_DONT_CARE},
			.initialLayout{VK_IMAGE_LAYOUT_UNDEFINED},
			.finalLayout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
		};
		VkAttachmentReference colorAttachmentRef = {
			.attachment{0},
			.layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Only one sub pass.
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// Create the back buffer pass.
		VkRenderPassCreateInfo renderPassCInfo = {};
		renderPassCInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCInfo.attachmentCount = 1;
		renderPassCInfo.pAttachments = &colorAttachment;
		renderPassCInfo.subpassCount = 1;
		renderPassCInfo.pSubpasses = &subpass;
		renderPassCInfo.dependencyCount = 1;
		renderPassCInfo.pDependencies = &dependency;
		if (!VKSucceed(vkCreateRenderPass(m_device, &renderPassCInfo, nullptr, &m_swapchainRenderPass))) {
			return false;
		}
	}

	// Create the swapchain framebuffers.	
	size_t swapchainFramebufferNum = m_swapchainImageViews.size();
	m_swapchainFramebuffers.resize(swapchainFramebufferNum);
	for (uint32_t i = 0; i < swapchainFramebufferNum; ++i) {
		VkFramebufferCreateInfo framebufferCInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = m_swapchainRenderPass,
			.attachmentCount = 1,
			.pAttachments = &m_swapchainImageViews[i],
			.width = m_swapchainExtent.width,
			.height = m_swapchainExtent.height,
			.layers = 1,
		};
		if (!VKSucceed(vkCreateFramebuffer(m_device, &framebufferCInfo, nullptr, &m_swapchainFramebuffers[i]))) {
			return false;
		}
	}

	return true;
}

bool VKRealtimeRenderingContext::InitializeInflightContext(uint32_t inflightFrameNum /*= 2*/)
{
	// Check that all synchoronization objects should not be initialized.
	RuntimeCheckError(m_imageAvailableSemaphores.empty());
	RuntimeCheckError(m_renderFinishedSemaphores.empty());
	RuntimeCheckError(m_renderFinishedFences.empty());

	m_inflightFrameNum = inflightFrameNum;

	// Create inflight semaphores and fences.
	m_imageAvailableSemaphores.resize(m_inflightFrameNum);
	m_renderFinishedSemaphores.resize(m_inflightFrameNum);
	m_renderFinishedFences.resize(m_inflightFrameNum);
	for (uint32_t i = 0; i < m_inflightFrameNum; ++i)
	{
		VkSemaphoreCreateInfo semaphoreCInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};

		if (!VKSucceed(vkCreateSemaphore(m_device, &semaphoreCInfo, m_allocator, &m_imageAvailableSemaphores[i]))) {
			return false;
		}

		if (!VKSucceed(vkCreateSemaphore(m_device, &semaphoreCInfo, m_allocator, &m_renderFinishedSemaphores[i]))) {
			return false;
		}

		VkFenceCreateInfo fenceCInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		if (!VKSucceed(vkCreateFence(m_device, &fenceCInfo, m_allocator, &m_renderFinishedFences[i]))) {
			return false;
		}
	}

	return true;
}

bool VKRealtimeRenderingContext::AdvanceFrame(InflightState* inflightFrameInfo)
{
	size_t nextInflightFrameIdx = (m_inflightFrameIdx + 1) % m_inflightFrameNum;
	uint64_t timeoutNano = 10;
	VkResult res{};

	// Wait for the previous inflight render finish fences. Can't timeout, as we input a infinite timeout parameter.
	res = vkWaitForFences(m_device, 1, &m_renderFinishedFences[nextInflightFrameIdx], VK_TRUE, timeoutNano);
	if (res != VK_SUCCESS)
	{
		assert(res == VK_TIMEOUT);
		return false;
	}

	// Acqure the swapchiain image.
	VkResult acqSwapchainImageRes = vkAcquireNextImageKHR(m_device, m_swapchain, timeoutNano, m_imageAvailableSemaphores[nextInflightFrameIdx], VK_NULL_HANDLE, &m_currSwapchainImageIndex);
	if (acqSwapchainImageRes != VK_SUCCESS) {
		assert(acqSwapchainImageRes == VK_NOT_READY);
		return false;
	}

	// So it's ready to begin the frame.
	VKCall(vkResetFences(m_device, 1, &m_renderFinishedFences[nextInflightFrameIdx]));

	m_inflightFrameIdx = nextInflightFrameIdx;

	*inflightFrameInfo = {
		.SwapchainImageIndex{ m_currSwapchainImageIndex },
		.SwapchainImage{ m_swapchainImages[m_currSwapchainImageIndex] },
		.SwapchainFramebuffer{ m_swapchainFramebuffers[m_currSwapchainImageIndex] },
		.InflightFrameIndex{ m_inflightFrameIdx },
		.InflightImageAvailableSemaphore{ m_imageAvailableSemaphores[m_inflightFrameIdx] },
		.InflightRenderFinishedSemaphore{ m_renderFinishedSemaphores[m_inflightFrameIdx] },
		.InflightRenderFinishedFence{ m_renderFinishedFences[m_inflightFrameIdx] },
	};

	return true;
}

void VKRealtimeRenderingContext::EndFrame(const std::span<VkCommandBuffer>& cmdBufs)
{
	// Submit the command buffer.
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_imageAvailableSemaphores[m_inflightFrameIdx],
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = (uint32_t)cmdBufs.size(),
		.pCommandBuffers = cmdBufs.data(),
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_renderFinishedSemaphores[m_inflightFrameIdx],
	};
	VKCall(vkQueueSubmit(m_queue, 1, &submitInfo, m_renderFinishedFences[m_inflightFrameIdx]));
}

bool VKRealtimeRenderingContext::Present(const std::span<VkSemaphore>& waitSemaphores)
{
	VkResult presentRes = VK_SUCCESS;
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = (uint32_t)waitSemaphores.size()	,
		.pWaitSemaphores = waitSemaphores.data(),
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain,
		.pImageIndices = &m_currSwapchainImageIndex,
		.pResults = &presentRes,
	};
	if (!VKSucceed(vkQueuePresentKHR(m_queue, &presentInfo)) || !VKSucceed(presentRes)) {
		return false;
	}

	return true;
}