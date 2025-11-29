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
//class VKContext
//{
//public:
//	VKContext(const VKContext& parent, const char* ctxName);
//
//	const char* GetContextName() const;
//
//	VKContext* GetParentContext() const;
//private:
//	const VKContext m_parentContext;
//	const char* m_contextName = nullptr;
//};


// Simple device context, contain only one queue, which can do all graphics, compute and transfer works.
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

	// 
	const VkDevice& GetDevice() const { return m_vkDevice; }
	// Get queue family idx.
	size_t GetQueueFamilyCount() const;
	uint32_t GetQueueFamilyIdx(uint32_t i) const;
	// Get queue.
	size_t GetQueueFamilyQueueCount(uint32_t i) const;
	VkQueue GetQueueFamilyQueue(uint32_t i, uint32_t queueIdx) const;

	const VmaAllocator& GetVmaAllocator() const { return m_VmaAllocator; }

	// Global vk objects.
	VkPipelineLayout GetGlobalEmptyPipelineLayout() const { return m_vkEmptyPipelineLayout; }

	const VkPipelineVertexInputStateCreateInfo& GetPipelineVertexInputStateCreateInfo_NoVertexInput() const {
		return m_vkPipelineVertexInputStateCInfo_NoVertexInput;
	}

private:
	const char* m_vkContextName = nullptr;

	const VkInstance m_vkInstance = VK_NULL_HANDLE;
	const VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;

	// Device and queues.
	VkDevice m_vkDevice = VK_NULL_HANDLE;
	std::vector<uint32_t> m_vkQueueFamilyIdxs;
	std::vector<size_t> m_vkQueueFamilyQueueStartIdxs;
	std::vector<VkQueue> m_vkQueues;

	VmaAllocator m_VmaAllocator = VK_NULL_HANDLE;

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
// NOT well organized, don't use this.
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

class VKCommandPoolContext
{
public:
	VKCommandPoolContext(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue, VkCommandPool commandPool, VkAllocationCallbacks* allocator)
		: m_device(device)
		, m_queueFamilyIndex(queueFamilyIndex)
		, m_queue(queue)
		, m_commandPool(commandPool)
		, m_allocator(allocator)
	{
	}

	~VKCommandPoolContext()
	{
		vkDestroyCommandPool(m_device, m_commandPool, m_allocator);
	}

	bool AllocateCommandBuffers(size_t cmdBufNum, const std::span<VkCommandBuffer>& cmdBufs) 
	{
		if (cmdBufs.size() < cmdBufNum) {
			LogError("VKCommandPoolContext::CreateCommandBuffer: cmdBufs size is smaller than cmdBufNum.");
			return false;
		}

		if (cmdBufNum > 0) {
			VkCommandBufferAllocateInfo cmdBufAllocInfo = {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = m_commandPool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = (uint32_t)cmdBufNum,
			};
			if (!VKSucceed(vkAllocateCommandBuffers(m_device, &cmdBufAllocInfo, cmdBufs.data()))) {
				return false;
			}
		}

		return true;
	}


private:
	VkDevice m_device{ VK_NULL_HANDLE };
	uint32_t m_queueFamilyIndex{ ~0u };
	VkQueue m_queue{ VK_NULL_HANDLE };
	VkCommandPool m_commandPool{ VK_NULL_HANDLE };
	VkAllocationCallbacks* m_allocator{ nullptr };
};
using VKCommandPoolContextUniquePtr = std::unique_ptr<VKCommandPoolContext>;
using VKCommandPoolContextSharedPtr = std::shared_ptr<VKCommandPoolContext>;

// VKSingleQueueDeviceContext
// * Only use one queue, which can graphics, compute and transfer
//	? The problem is that this depends on the physical device and the chosen queue index. So it's better to also include the picking of the physical device here ?

class VKSingleQueueDeviceContext
{
public:

	VKSingleQueueDeviceContext(VkInstance inst, VkPhysicalDevice physDevice, uint32_t queueFamilyIdx);

	// Input: enabled layers, required extensions.
	bool InitializeDevice(
		std::span<const char*> layers, 
		std::span<const char*> extensions);

	// Initialize context for realtime rendering, e.g. swapchain, semaphores, fences.
	bool InitializeSwapchainContext(VkSurfaceKHR surf, const VkSurfaceFormatKHR& surfFmt, const VkExtent2D& swapchainExtent, uint32_t swapchainMinImageCount = 3);

	void DestroySwapchain();

	bool InitializeInflightContext(uint32_t inflightFrameNum = 2);

	bool CreateGraphicsQueueCommandPool(VKCommandPoolContextUniquePtr* cmdPoolCtx);
	// VKCommandPoolContextSharedPtr CreateComputeQueueCommandPool();

	// SDL_Window* GetWindow() const { return m_sdlWindow; }

	VkInstance GetInstance() const { return m_instance; }
	VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
	uint32_t GetQueueFamilyIdx() const { return m_queueFamilyIdx; }
	VkDevice GetDevice() const { return m_device; }
	VkQueue GetQueue() const { return m_queue; }

	VkSurfaceKHR GetSurface() const { return m_surface; }
	VkSurfaceFormatKHR GetSurfaceFormat() const { return m_surfaceFormat; }
	VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
	size_t GetSwapchainImageNum() const { return m_swapchainImages.size(); }
	std::span<const VkImageView> GetSwapchainImageViews() const { return m_swapchainImageViews; }
	VkRenderPass GetSwapchainRenderPass() const { return m_swapchainRenderPass; }


	VkCommandPool GetCommandPool() const { return m_commandPool; }

	size_t GetInflightFrameNum() const { return m_inflightFrameNum; }		

	// If return false, can't begin the current frame.
	bool BeginFrame();	
	size_t GetInflightFrameIndex() const { return m_inflightFrameIdx; }
	VkImage GetSwapchainImage() const { return m_swapchainImages[m_currSwapchainImageIndex]; }
	VkFramebuffer GetSwapchainFramebuffer() const { return m_swapchainFramebuffers[m_currSwapchainImageIndex]; }
	VkRect2D GetSwapchainRect() const { return VkRect2D{ {0,0}, m_swapchainExtent }; }

	// bool GraphicsQueueSubmitAndPresent

	void EndFrame(const std::span<VkCommandBuffer>& cmdBufs, bool present);

	bool DeviceWaitIdle();

	~VKSingleQueueDeviceContext();

private:
	VkAllocationCallbacks* m_allocator{ nullptr };

	// Device context.
	VkInstance m_instance{ VK_NULL_HANDLE };
	VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
	uint32_t m_queueFamilyIdx{ ~0u };
	VkDevice m_device{ VK_NULL_HANDLE };
	VkQueue m_queue{ VK_NULL_HANDLE };
	
	// Swapchain context.	
	VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
	VkSurfaceFormatKHR m_surfaceFormat{};
	VkExtent2D m_swapchainExtent{ ~0u, ~0u };
	VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };
	VkRenderPass m_swapchainRenderPass{ VK_NULL_HANDLE };
	std::vector<VkImage> m_swapchainImages;
	uint32_t m_currSwapchainImageIndex{ ~0u };
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;

	// Inflight context.
	size_t m_inflightFrameNum{ 2 };
	size_t m_inflightFrameIdx{ 0 };
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_renderFinishedFences;

	VkCommandPool m_commandPool{ VK_NULL_HANDLE };		// Should this be put here?

	// Vma memory management.
	//VmaAllocator m_VmaAllocator{ VK_NULL_HANDLE };
};

#endif