#ifndef _VK_CONTEXT_H_
#define _VK_CONTEXT_H_

#include <map>
#include <functional>
#include "vk_function.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

class VKContext
{
public:
	virtual ~VKContext();

	// Create vulkan instance.
	bool CreateVkInstance(size_t reqVkInstExtNum, const char** reqVkInstExts);
	VkInstance GetVkInstance() const { return m_vkInstance; }

	// Initialize vulkan.
	bool InitializeVulkan(VkSurfaceKHR winSurf, int drawableWidth, int drawableHeight);

	VkPhysicalDevice GetVkPhysicalDevice() const { return m_vkPhysicalDevice; }
	VkDevice GetVkDevice() const { return m_vkDevice; }

	uint32_t GetQueueFamilyIdx() const { return m_vkQueueFamilyIdx; }
	VkQueue GetGraphicsQueue() const { return m_vkQueue; }

	const VkRect2D& GetSwapchainRect() const { return m_swapchainContext.GetSwapchainRect(); }
	const VkRenderPass& GetSwapchainRenderPass() const { return m_swapchainContext.GetSwapchainRenderPass(); }
	const VkFramebuffer& GetSwapchainFramebuffer() const { return m_swapchainContext.GetSwapchainFramebuffer(); }
	uint32_t GetSwapchainImageNum() const { return m_swapchainContext.GetSwapchainImageNum(); }

	uint32_t GetInflightFrameNum() const { return m_inflightContext.GetInflightFrameNum(); }
	uint32_t GetInflightFrameIdx() const { return m_inflightContext.GetInflightFrameIdx(); }

	bool WaitForInflightFrameFence(uint64_t timeout = UINT64_MAX);
	bool AcquireNextSwapchainImage(uint64_t timeout = UINT64_MAX);

	void SubmitToQueue(size_t cmdBufNum, const VkCommandBuffer *cmdBufs);
	void Present();

	void NextFrame();
	
private:
	VkInstance m_vkInstance = VK_NULL_HANDLE;

	uint32_t m_drawableWidth = (uint32_t)-1;
	uint32_t m_drawableHeight = (uint32_t)-1;
	VkSurfaceKHR m_winSurf = VK_NULL_HANDLE;

	VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_vkDevice = VK_NULL_HANDLE;
	uint32_t m_vkQueueFamilyIdx = (uint32_t)-1;
	VkQueue m_vkQueue = VK_NULL_HANDLE;

	VKSwapchainContext m_swapchainContext;
	VKInflightContext m_inflightContext;
};

#endif