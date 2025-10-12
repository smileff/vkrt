#include "vk_context.h"

VKDeviceContext::VKDeviceContext(const char* ctxName)
	: m_vkContextName(ctxName)
{
}

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
	if (!VKCreateDevice(m_vkDevice, m_vkPhysicalDevice, queueCreateQuestCnt, queueCreateQuests, layerCnt, layers, extensionCnt, extensions)) {
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
{
}

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