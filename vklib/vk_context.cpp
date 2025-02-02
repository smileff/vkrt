#include "vk_context.h"
#include <algorithm>

VKContext::~VKContext()
{
	VKCall(vkDeviceWaitIdle(m_vkDevice));

	m_inflightContext.~VKInflightContext();
	m_swapchainContext.~VKSwapchainContext();

	if (m_vkQueue != VK_NULL_HANDLE) {
		m_vkQueue = VK_NULL_HANDLE;
	}

	if (m_vkDevice != VK_NULL_HANDLE) {
		vkDestroyDevice(m_vkDevice, g_vkAllocator);
		m_vkDevice = VK_NULL_HANDLE;
	}

	if (m_vkPhysicalDevice != VK_NULL_HANDLE) {
		m_vkPhysicalDevice = VK_NULL_HANDLE;
	}

	if (m_winSurf != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(m_vkInstance, m_winSurf, g_vkAllocator);
		m_winSurf = VK_NULL_HANDLE;
	}

	if (m_vkInstance) {
		vkDestroyInstance(m_vkInstance, g_vkAllocator);
		m_vkInstance = VK_NULL_HANDLE;
	}
}

bool VKContext::CreateVkInstance(size_t reqVkInstExtNum, const char** reqVkInstExts)
{
	// Determine the required vulkan extensions.
	std::vector<const char*> vkInstExtensions(reqVkInstExts, reqVkInstExts + reqVkInstExtNum);
	vkInstExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	// Determine the to-enable layers
	std::vector<const char*> vkEnableLayers = {
		"VK_LAYER_KHRONOS_validation",	// Enable validation layer.
	};

	if (!VKCreateInstance(m_vkInstance, vkInstExtensions.size(), vkInstExtensions.data(), vkEnableLayers.size(), vkEnableLayers.data())) {
		return false;
	}

	return true;
}

bool VKContext::InitializeVulkan(VkSurfaceKHR winSurf, int drawableWidth, int drawableHeight)
{
	m_drawableWidth = drawableWidth;
	m_drawableHeight = drawableHeight;
	m_winSurf = winSurf;

	// Pick vulkan physical device.
	if (!VKPickPhysicalDeviceAndOneQueueFamily(m_vkInstance, m_winSurf, &m_vkPhysicalDevice, &m_vkQueueFamilyIdx)) {
		return false;
	}
	const int vkQueueCnt = 1;

	// Create logical device and device queue.
	{
		// Create logical device.
		std::vector<VKDeviceQueueCreateRequest> vkQueueCreateRequests = {
			{ m_vkQueueFamilyIdx, 1, {1.0f} },
		};
		const char* enableExtensionNames[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		if (!VKCreateDevice(m_vkDevice, m_vkPhysicalDevice, vkQueueCreateRequests, 0, nullptr, 1, enableExtensionNames)) {
			return false;
		}

		// Get device queue.
		vkGetDeviceQueue(m_vkDevice, m_vkQueueFamilyIdx, 0, &m_vkQueue);
	}

	// Determine the main surface format.
	VkSurfaceFormatKHR vkSwapchainSurfaceFormat;
	if (!VKPickSurfaceFormat(m_vkPhysicalDevice, m_winSurf, vkSwapchainSurfaceFormat)) {
		return false;
	}

	if (!m_swapchainContext.InitSwapchain(m_vkDevice, m_vkQueueFamilyIdx, m_winSurf, vkSwapchainSurfaceFormat, m_drawableWidth, m_drawableHeight)) {
		return false;
	}

	if (!m_inflightContext.InitInflight(m_vkDevice)) {
		return false;
	}


	// So all are ready.

	return true;
}

bool VKContext::WaitForInflightFrameFence(uint64_t timeout)
{
	return m_inflightContext.WaitForInflightFrameFence(timeout);
}

bool VKContext::AcquireNextSwapchainImage(uint64_t timeout)
{
	return m_swapchainContext.AcquireNextSwapchainImage(
		m_inflightContext.GetImageAvailableSemaphore(), timeout);
}

void VKContext::SubmitToQueue(size_t cmdBufNum, const VkCommandBuffer *cmdBufs)
{
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_inflightContext.GetImageAvailableSemaphore(),
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = (uint32_t)cmdBufNum,
		.pCommandBuffers = cmdBufs,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_inflightContext.GetRenderFinishedSemaphore(),
	};

	VKCall(vkQueueSubmit(m_vkQueue, 1, &submitInfo, m_inflightContext.GetInflightFrameFence()));
}

void VKContext::Present()
{
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_inflightContext.GetRenderFinishedSemaphore(),
		.swapchainCount = 1,
		.pSwapchains = &m_swapchainContext.GetSwapchain(),
		.pImageIndices = &m_swapchainContext.GetSwapchainImageIdx(),
	};
	vkQueuePresentKHR(m_vkQueue, &presentInfo);
}

void VKContext::NextFrame()
{
	// Move to next frame.
	m_inflightContext.NextFrame();
}
