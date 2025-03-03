#ifndef _VK_FUNCTION_H_
#define _VK_FUNCTION_H_

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <exception>
#include <array>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <numeric>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <thread>
// #include <functional>
#include <algorithm>

#include "vulkan/vulkan.h"
#include "vulkan/vk_enum_string_helper.h"

bool VKCheckError(VkResult result);

#define VKCall(code) { bool success = VKCheckError(code); assert(success); }
#define VKSucceed(code) (VKCheckError(code))

// Create VkInstance according to input parameters.
bool VKCreateInstance(VkInstance& vkInst, size_t instExtNum, const char** instExts, size_t enableLayerNum, const char** enableLayers, VkAllocationCallbacks* allocator = nullptr);

void VKSetDebugObjectName(VkDevice vkDevice, VkObjectType vkObjType, uint64_t vkObjHandle, const char* name);

// Pick a physical device supporting the input surface, and with single queue family.
bool VKPickPhysicalDeviceAndOneQueueFamily(VkInstance vkInst, VkSurfaceKHR surf, VkPhysicalDevice* vkPhysicalDevice, uint32_t* vkQueueFamilyIdx);

// Create a logical device with the input device queue create requests.
struct VKDeviceQueueCreateRequest
{
	uint32_t QueueFamilyIdx;
	uint32_t QueueCount;
	std::vector<float> QueuePriorities;
};
bool VKCreateDevice(
	VkDevice& vkDevice,
	VkPhysicalDevice vkPhysicalDevice, 
	const std::vector<VKDeviceQueueCreateRequest>& vkQueueCreateRequests, 
	uint32_t enableLayerNum, const char** enableLayers,
	uint32_t enableExtensionNum, const char** enableExtensions,
	const void* featureLinkedList = nullptr, VkAllocationCallbacks* vkAllocator = nullptr);

void VKPrintSurfaceCapabilities(const char* surfName, VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf);

void VKPrintPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice vkPhysicalDevice);

void VKPrintPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures& vkPhysicalDeviceFeatures);

VkBool32 VKDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);

bool VKRegisterDebugReportCallback(VkInstance vkInst);

bool VKPickSurfaceFormat(VkPhysicalDevice vkPhysicalDevice, VkSurfaceKHR vkSurf, VkSurfaceFormatKHR& surfFmt);


// Helper function.
template<class Func>
class OnScopeExit
{
public:
	OnScopeExit(Func&& func) : m_func(func) {}
	~OnScopeExit() { m_func(); }
private:
	Func m_func;
};

// PhysicalDevice, Device, and the Queues allocated with the device.
// How to design the interface to query queue?

class VKDeviceContext
{
public:
	VKDeviceContext(const char* ctxName);
	~VKDeviceContext();

	std::string GetContextName() const { return m_vkContextName ? m_vkContextName : ""; }

	bool CreateDevice(
		VkInstance vkInst, VkPhysicalDevice physicalDevice, 
		size_t queueCreateQuestCnt, const VKDeviceQueueCreateRequest* queueCreateQuests, 
		size_t layerCnt, const char** layers, size_t extensionCnt, const char** extensions);

	// Instance, PhysicalDevice, Device.
	const VkInstance& GetInstance() const { return m_vkInstance; }
	const VkPhysicalDevice& GetPhysicalDevice() const { return m_vkPhysicalDevice; }
	const VkDevice& GetDevice() const { return m_vkDevice; }

	// Get queue family idx.
	size_t GetQueueFamilyCount() const;
	uint32_t GetQueueFamilyIdx(uint32_t i) const;
	// Get queue.
	size_t GetQueueFamilyQueueCount(uint32_t i) const;
	VkQueue GetQueueFamilyQueue(uint32_t i, uint32_t queueIdx) const;

	// Global vk objects.
	bool CreateGlobalObjects();
	VkPipelineLayout GetGlobalEmptyPipelineLayout() const { return m_vkEmptyPipelineLayout; }

	const VkPipelineVertexInputStateCreateInfo& GetPipelineVertexInputStateCreateInfo_NoVertexInput() const {
		return m_vkPipelineVertexInputStateCInfo_NoVertexInput;
	}

private:
	const char* m_vkContextName = nullptr;

	VkInstance m_vkInstance = VK_NULL_HANDLE;
	VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_vkDevice = VK_NULL_HANDLE;

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

class VKObject
{
public:
	VKObject(const VKDeviceContext& vkDeviceCtx, const char* objName = nullptr, const VkAllocationCallbacks* vkAllocator = nullptr)
		: m_vkDeviceContext(vkDeviceCtx)
		, m_vkAllocator(vkAllocator)
	{
		if (objName) {
			m_objName = m_vkDeviceContext.GetContextName();
			if (!m_objName.empty()) {
				m_objName += ".";
			}
			if (objName) {
				m_objName += objName;
			} else {
				m_objName += "UnnamedObject";
			}
		}
	}

	virtual ~VKObject() {}

	// Forbid copy.
	VKObject(const VKObject&) = delete;
	VKObject& operator=(const VKObject&) = delete;

	const VKDeviceContext& GetDeviceContext() const { return m_vkDeviceContext; }
	const VkInstance& GetInstance() const { return m_vkDeviceContext.GetInstance(); }
	const VkPhysicalDevice& GetPhysicalDevice() const { return m_vkDeviceContext.GetPhysicalDevice(); }
	const VkDevice& GetDevice() const { return m_vkDeviceContext.GetDevice(); }

	const VkAllocationCallbacks* GetAllocator() const { return m_vkAllocator; }

	const char* GetName() const { return m_objName.c_str(); }

private:
	const VKDeviceContext& m_vkDeviceContext;
	const VkAllocationCallbacks* m_vkAllocator = nullptr;
	std::string m_objName;
};

// Swapchain vulkan objects.
class VKSwapchainContext
{
public:

	VKSwapchainContext(const VKDeviceContext& vkDeviceContext, int swapchainImageNum = 3, const VkAllocationCallbacks* vkAllocator = nullptr);
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
	const VKDeviceContext& m_vkCtx;
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
	VKInflightContext(const VKDeviceContext& vkDeviceContext, int inflightFrameNum = 2, const VkAllocationCallbacks* vkAllocator = nullptr);
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
	const VKDeviceContext& m_vkCtx;
	const VkAllocationCallbacks* m_vkAllocator = nullptr;
	const uint32_t m_inflightFrameNum = 2;
	uint32_t m_inflightFrameIdx = 0;
	std::vector<VkSemaphore> m_vkImageAvailableSemaphores;
	std::vector<VkSemaphore> m_vkRenderFinishedSemaphores;
	std::vector<VkFence> m_vkInflightFrameFences;
};

bool VKCompileShader(const std::string& shaderFilepath, const std::string& options, const std::string& outputFilepath);

//class VKShaderModule : public VKObject
//{
//public:
//	VKShaderModule(const VKDeviceContext& vkDeviceCtx, const char* name = "VKShaderModule", const VkAllocationCallbacks* vkAllocator = nullptr)
//		: VKObject(vkDeviceCtx, name, vkAllocator) {}
//	virtual ~VKShaderModule();
//
//	bool CreateShaderModule(const std::string& shaderFilepath);	
//
//	bool IsValid() const { return m_shaderModule != VK_NULL_HANDLE; }
//
//	operator VkShaderModule() { return m_shaderModule; }
//
//private:
//	VkShaderModule m_shaderModule = VK_NULL_HANDLE;
//};

bool VKCreateShaderModuleFromSPV(VkDevice vkDevice, const std::string& filepath, VkShaderModule& shaderModule);

class VKPipelineLayout : public VKObject
{
public:
	VKPipelineLayout(const VKDeviceContext& vkDeviceCtx, const char* name = "VKPipelineLayout", const VkAllocationCallbacks* vkAllocator = nullptr)
		: VKObject(vkDeviceCtx, name, vkAllocator) {}
	virtual ~VKPipelineLayout() override;

	bool CreatePipelineLayout(const VkPipelineLayoutCreateInfo& pplLayoutCInfo);

	operator VkPipelineLayout() { return m_vkPipelineLayout; }

private:
	VkPipelineLayout m_vkPipelineLayout = VK_NULL_HANDLE;
};

class VKGraphicsPipeline : public VKObject
{
public:

	VKGraphicsPipeline(const uint32_t& shaderStageFlagBits, const VKDeviceContext& vkDeviceCtx, const char* name = "VKPipeline", const VkAllocationCallbacks* vkAllocator = nullptr)
		: VKObject(vkDeviceCtx, name, vkAllocator) 
		, m_vkShaderStageFlagBits((VkShaderStageFlagBits)shaderStageFlagBits)
	{}

	virtual ~VKGraphicsPipeline();

	// Stages.
	virtual bool ConfigureVertexStage(VkPipelineShaderStageCreateInfo& vertStageCInfo);
	virtual bool ConfigFragmentStage(VkPipelineShaderStageCreateInfo& fragtStageCInfo);

	// Verte input state.
	virtual void ConfigureVertexInputState(VkPipelineVertexInputStateCreateInfo& vertInputStateCInfo);

	// Input assemble state.
	virtual void ConfigureInputAssemblyState(VkPipelineInputAssemblyStateCreateInfo& inputAssemblyStateCInfo);

	// Viewport state
	virtual void ConfigureViewportState(VkPipelineViewportStateCreateInfo& viewportStateCInfo);

	// Rasterization state.
	virtual void ConfigureRasterizationState(VkPipelineRasterizationStateCreateInfo& rasterizationStateCInfo);

	// Multisample state.
	virtual void ConfigureMultisampleState(VkPipelineMultisampleStateCreateInfo& multisampleStateCInfo);

	// Color blend state.
	virtual void ConfigureColorBlendState(VkPipelineColorBlendStateCreateInfo& blendStateCInfo);

	// Dynamic state.
	virtual void ConfigureDynamicState(VkPipelineDynamicStateCreateInfo& dynamicStateCInfo);

	// PipelineLayout
	// virtual void ConfigurePipelineLayoutCreateInfo(VkPipelineLayoutCreateInfo &pipelineLayoutCInfo) const;

	virtual VkPipelineLayout GetPipelineLayout();

	bool CreateGraphicsPipeline(VkRenderPass renderPass, uint32_t subpass);

	VkPipeline GetPipeline() const { return m_vkPipeline; }
	operator VkPipeline () const { return m_vkPipeline; }
	
private:
	const uint32_t m_vkShaderStageFlagBits = (VkShaderStageFlagBits)0;
	VkPipeline m_vkPipeline = VK_NULL_HANDLE;
};

//class TestGraphicsPipeline : public VKGraphicsPipeline
//{
//public:
//	TestGraphicsPipeline(const VKDeviceContext& vkDeviceCtx, const char* name = "TestGraphicsPipeline")
//		: VKGraphicsPipeline(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vkDeviceCtx, name, nullptr)
//	{}
//
//	virtual ~TestGraphicsPipeline() override;
//
//	virtual bool ConfigureVertexStage(VkPipelineShaderStageCreateInfo& vertStageCInfo) override;
//	virtual bool ConfigFragmentStage(VkPipelineShaderStageCreateInfo& fragStageCInfo) override;
//
//private:
//	VKShaderModule m_vertShaderModule = { GetDeviceContext(), "VertexShaderModule", GetAllocator() };
//	VKShaderModule m_fragShaderModule = { GetDeviceContext(), "FragmentShaderModule", GetAllocator() };
//
//	VkShaderModule m_vertShaderModule2 = VK_NULL_HANDLE;
//	VkShaderModule m_fragShaderModule2 = VK_NULL_HANDLE;
//};

bool VKCreateGraphicsPipelineTest(VkDevice vkDevice, VkRenderPass pass, uint32_t subpass, VkPipeline& pipeline);

//class VKGraphicsPipelineBuilder
//{
//public:
//	VKGraphicsPipelineBuilder(const VKDeviceContext& vkDeviceCtx, const VkAllocationCallbacks* vkAllocator = nullptr)
//		: m_vkCtx(vkDeviceCtx)
//		, m_vkAllocator(vkAllocator)
//	{
//		// 
//	};
//
//private:
//	const VKDeviceContext& m_vkCtx;
//	const VkAllocationCallbacks* m_vkAllocator = nullptr;
//
//};

#endif