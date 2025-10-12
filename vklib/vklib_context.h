#ifndef _VK_DEVICE_CONTEXT_H_
#define _VK_DEVICE_CONTEXT_H_

#include "vklib_function.h"
#include "vma_usage.h"

// VKDeviceContext
// Purpose of this class it to contain the global vulkan objects, like vkInstance, vkPhysicalDevice, the queueFamily index, and device.
// The problem is, for each application, the way to pick the physical device and queue family is different. Many applications only pick one device queue while others may use multiple physical device.s
// It's unwise to design a library for these for this moment.
// Satisify current needs is enough.

// VKContext, encapsule:
// * context name.
// * context object tree-hierarchy.
class VKContext
{
public:
	VKContext(const std::shared_ptr<VKContext>& parent, const char* ctxName);

	const char* GetContextName() const;

	VKContext* GetParentContext() const;
private:
	std::shared_ptr<VKContext> m_parentContext;
	const char* m_contextName = nullptr;
};

// Encapsule the vulkan instance and physical device.
class VKInstanceContext
{
public:
	VKInstanceContext(const char* ctxName);


private:
	VkInstance m_vkInstance = VK_NULL_HANDLE;
	VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
};

// Concerns:
// * Should support multiple queue?
class VKDeviceContext
{
public:
	VKDeviceContext(VkInstance instance, VkPhysicalDevice physicalDevice, const char* ctxName);
	~VKDeviceContext();

	std::string GetContextName() const { return m_vkContextName ? m_vkContextName : ""; }

	// Input: queues to create, enabled layers, required extensions.
	bool InitDeviceContext(
		size_t queueCreateQuestCnt, const VKDeviceQueueCreateRequest* queueCreateQuests,
		size_t layerCnt, const char** layers, 
		size_t extensionCnt, const char** extensions);

	// Instance, PhysicalDevice, Device.
	const VkInstance& GetInstance() const { return m_vkInstance; }	
	const VkPhysicalDevice& GetPhysicalDevice() const { return m_vkPhysicalDevice; }
	const VkDevice& GetDevice() const { return m_vkDevice; }
	const VmaAllocator& GetVmaAllocator() const { return m_VmaAllocator; }

	// Get queue family idx.
	size_t GetQueueFamilyCount() const;
	uint32_t GetQueueFamilyIdx(uint32_t i) const;
	// Get queue.
	size_t GetQueueFamilyQueueCount(uint32_t i) const;
	VkQueue GetQueueFamilyQueue(uint32_t i, uint32_t queueIdx) const;

	// Global vk objects.
	VkPipelineLayout GetGlobalEmptyPipelineLayout() const { return m_vkEmptyPipelineLayout; }

	const VkPipelineVertexInputStateCreateInfo& GetPipelineVertexInputStateCreateInfo_NoVertexInput() const {
		return m_vkPipelineVertexInputStateCInfo_NoVertexInput;
	}

private:
	const char* m_vkContextName = nullptr;

	const VkInstance m_vkInstance = VK_NULL_HANDLE;
	const VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_vkDevice = VK_NULL_HANDLE;
	VmaAllocator m_VmaAllocator = VK_NULL_HANDLE;

	// Queue family idxs and queues.
	std::vector<uint32_t> m_vkQueueFamilyIdxs;
	std::vector<size_t> m_vkQueueFamilyQueueStartIdxs;
	std::vector<VkQueue> m_vkQueues;

	VkPipelineVertexInputStateCreateInfo m_vkPipelineVertexInputStateCInfo_NoVertexInput = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr,
	};

	// Device global objects.
	VkPipelineLayout m_vkEmptyPipelineLayout = VK_NULL_HANDLE;
};


// Swapchain vulkan objects.

class VKSwapchainContext
{
public:

	VKSwapchainContext(const class VKDeviceContext& vkDeviceContext, int swapchainImageNum = 3, const VkAllocationCallbacks* vkAllocator = nullptr);
	virtual ~VKSwapchainContext();

	bool InitSwapchain(uint32_t queueFamilyIdx, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR vkSurfaceFormat, uint32_t rtWidth, uint32_t rtHeight);

	const VkSwapchainKHR& GetSwapchain() const { return m_vkSwapchain; }
	const VkRenderPass& GetSwapchainRenderPass() const { return m_swapchainRenderPass; }
	const VkRect2D& GetSwapchainRect() const { return m_swapchainRect; }

	// Render a frame.
	uint32_t GetSwapchainImageNum() const { return m_swapchainImageNum; }
	const uint32_t& GetSwapchainImageIdx() const { return m_swapchainImageIdx; }

	bool AcquireNextSwapchainImage(VkSemaphore imageAvailableSemaphore, uint64_t timeout = UINT64_MAX);

	const VkFramebuffer& GetSwapchainFramebuffer() const { return m_swapchainFramebuffers[m_swapchainImageIdx]; }

private:
	const class VKDeviceContext& m_vkCtx;
	const VkAllocationCallbacks* m_vkAllocator = nullptr;
	const int m_swapchainImageNum;
	uint32_t m_swapchainImageIdx = (uint32_t)-1;
	VkSurfaceFormatKHR m_vkSwapchainSurfaceFormat = {};
	VkSwapchainKHR m_vkSwapchain = VK_NULL_HANDLE;
	VkRenderPass m_swapchainRenderPass = VK_NULL_HANDLE;
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	VkRect2D m_swapchainRect = {};
};


// Semaphores and fences for inflight rendering.

class VKInflightContext
{
public:
	VKInflightContext(const class VKDeviceContext& vkDeviceContext, int inflightFrameNum = 2, const VkAllocationCallbacks* vkAllocator = nullptr);
	virtual ~VKInflightContext();

	bool InitInflight();

	uint32_t GetInflightFrameNum() const { return m_inflightFrameNum; }
	uint32_t GetInflightFrameIdx() const { return m_inflightFrameIdx; }

	bool WaitForInflightFrameFence(uint64_t timeout = UINT64_MAX);

	const VkSemaphore& GetImageAvailableSemaphore() const { return m_vkImageAvailableSemaphores[m_inflightFrameIdx]; }
	const VkSemaphore& GetRenderFinishedSemaphore() const { return m_vkRenderFinishedSemaphores[m_inflightFrameIdx]; }
	const VkFence& GetInflightFrameFence() const { return m_vkInflightFrameFences[m_inflightFrameIdx]; }

	void NextFrame();

private:
	const class VKDeviceContext& m_vkCtx;
	const VkAllocationCallbacks* m_vkAllocator = nullptr;
	const uint32_t m_inflightFrameNum = 2;
	uint32_t m_inflightFrameIdx = 0;
	std::vector<VkSemaphore> m_vkImageAvailableSemaphores;
	std::vector<VkSemaphore> m_vkRenderFinishedSemaphores;
	std::vector<VkFence> m_vkInflightFrameFences;
};


#endif