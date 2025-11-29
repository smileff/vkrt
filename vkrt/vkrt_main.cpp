#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <exception>
#include <vector>
#include <deque>
#include <set>
#include <numeric>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <thread>

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include "vulkan/vulkan.h"
#include "vulkan/vk_enum_string_helper.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include "vklib.h"

static const VkAllocationCallbacks* g_vkAllocator = nullptr;

class VKRTApplication : public SDLVulkanApplication
{
public:
	bool Initialize(SDL_Window* sdlWin, const VkInstance& vkInst, const VkSurfaceKHR& vkSurf) override;

	void RunOneFrame(double frameSeconds, double FPS) override;

	bool Shutdown() override;

private:
	std::unique_ptr<VKSingleQueueDeviceContext> m_context;
	VKCommandPoolContextUniquePtr m_cmdPoolCtx;
	std::vector<VkCommandBuffer> m_inflightCmdBufs;
};

bool VKRTApplication::Initialize(SDL_Window* sdlWin, const VkInstance& vkInst, const VkSurfaceKHR& vkSurf)
{
	// Pick a physical device that supports all graphics, compute and transfer.
	VkPhysicalDevice vkPhysicalDevice{ VK_NULL_HANDLE };
	uint32_t queueFamilyIdx{ ~0u };
	if (!VKPickSinglePhysicalDeviceAndQueueFamily(vkInst, vkSurf, &vkPhysicalDevice, &queueFamilyIdx)) {
		return false;
	}

	// Print device infos.
	VKPrintPhysicalDeviceName(vkPhysicalDevice);
	VKPrintPhysicalDeviceFeatures(vkPhysicalDevice);
	VKPrintPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice);

	// Initailize the vulkan device context.
	m_context = std::make_unique<VKSingleQueueDeviceContext>(vkInst, vkPhysicalDevice, queueFamilyIdx);
	std::vector<const char*> enabledLayerNames = {};			// No layer to enable.
	std::vector<const char*> enableExtensionNames = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };			// No extention to enable.
	if (!m_context->InitializeDevice(enabledLayerNames, enableExtensionNames)) {
		return false;
	}

	// Print the capabilities of this surface.	
	VKPrintSurfaceCapabilities(SDL_GetWindowTitle(sdlWin), vkPhysicalDevice, vkSurf);
	// Pick a surface format.
	VkSurfaceFormatKHR surfFmt{};
	if (!VKPickSurfaceFormat(vkPhysicalDevice, vkSurf, &surfFmt)) {
		LogError("Failed to pick a surface format.");
		return false;
	}
	// Get the size of the drawable rect.	
	VkExtent2D surfExt{};
	SDL_Vulkan_GetDrawableSize(sdlWin, (int*)&surfExt.width, (int*)&surfExt.height);
	// Initialize the swapchain context.
	if (!m_context->InitializeSwapchainContext(vkSurf, surfFmt, surfExt)) {
		return false;
	}

	// Initialize the inflight contexts.	
	if (!m_context->InitializeInflightContext()) {
		return false;
	}

	// Create command pool.
	if (!m_context->CreateGraphicsQueueCommandPool(&m_cmdPoolCtx)) {
		return false;
	}

	// std::array<VkCommandBuffer, 3> cmdBufs;
	size_t inflightFrameNum = m_context->GetInflightFrameNum();
	m_inflightCmdBufs.resize(inflightFrameNum, VK_NULL_HANDLE);
	if (!m_cmdPoolCtx->AllocateCommandBuffers(inflightFrameNum, m_inflightCmdBufs)) {
		return false;
	}


	// Allocate a command buffer for each swapchain image.


	//uint32_t inflightFrameNum = m_context->GetInflightFrameNum();
	// std::span<const VkImageView> swapchainImageViews = m_context->GetSwapchainImageViews();

	// Initialize ImgUI
	//// Initialize imgUI
	//IMGUI_CHECKVERSION();
	//ImGui::CreateContext();
	//ImGuiIO& io = ImGui::GetIO();
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	//// io.ConfigFlags != ImGuiConfigFlags_DockingEnable;

	//ImGui_ImplSDL2_InitForVulkan(sdlWin);
	//ImGui_ImplVulkan_InitInfo init_info = {
	//	.Instance = vkInst,
	//	.PhysicalDevice = m_context->GetPhysicalDevice(),
	//	.Device = m_context->GetDevice(),
	//	.QueueFamily = queueFamilyIdx,
	//	.Queue = queue,
	//	.DescriptorPool = VK_NULL_HANDLE,
	//	.RenderPass = m_context->GetSwapchainRenderPass(),
	//	.MinImageCount = inflightFrameNum,
	//	.ImageCount = inflightFrameNum,
	//	.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
	//	.PipelineCache = VK_NULL_HANDLE,
	//	.Subpass = 0,
	//	.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE * 2,
	//	//.UseDynamicRendering = true,
	//	//.PipelineRenderingCreateInfo = {
	//	//	.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	//	.pNext = nullptr,
	//	//	.viewMask = 0x0,
	//	//	.colorAttachmentCount = 1,
	//	//	.pColorAttachmentFormats = &vkSurfFmt.format,
	//	//	.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
	//	//	.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	//	//},
	//	.Allocator = g_vkAllocator,
	//};
	//ImGui_ImplVulkan_Init(&init_info);

	//// Skip loading font.
	//io.Fonts->AddFontDefault();

	return true;
}

void VKRTApplication::RunOneFrame(double frameSeconds, double FPS)
{
	if (!m_context->BeginFrame()) {
		return;
	}

	size_t inflightFrameIndex = m_context->GetInflightFrameIndex();
	VkImage swapchainImage = m_context->GetSwapchainImage();


	VkDevice vkDevice = m_context->GetDevice();
	VkSwapchainKHR vkSwapchain = m_context->GetSwapchain();
	std::span<const VkImageView> vkSwapchainImages = m_context->GetSwapchainImageViews();

	// ImGui handles events.

	//// ImGui start new frame.
	//ImGui_ImplSDL2_NewFrame();
	//ImGui_ImplVulkan_NewFrame();
	//ImGui::NewFrame();
	//ImGui::ShowDemoWindow();
	//ImGui::ShowMetricsWindow();
	//ImGui::ShowDebugLogWindow();
	//ImGui::ShowIDStackToolWindow();
	//ImGui::ShowAboutWindow();

	// Do rendering.
	// Just clear the backbuffer now.
	// 				
	// Reset command buffer.
	VkCommandBuffer vkCmdBuf = m_inflightCmdBufs[inflightFrameIndex];
	VKCall(vkResetCommandBuffer(vkCmdBuf, 0));

	// Begin the command buffer.
	VkCommandBufferBeginInfo cmdBufBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VKCall(vkBeginCommandBuffer(vkCmdBuf, &cmdBufBeginInfo));

	//// Transfer the image to the color attachment layout.
	//VkImageMemoryBarrier imgBarrier = {
	//	.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	//	.srcAccessMask = 0,
	//	.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	//	.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	//	.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	//	.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	//	.image = vkSwapchainImages[inflightIdx],
	//	.subresourceRange = {
	//		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	//		.baseMipLevel = 0,
	//		.levelCount = 1,
	//		.baseArrayLayer = 0,
	//		.layerCount = 1,
	//	},
	//};
	//vkCmdPipelineBarrier(
	//	vkCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
	//	0, nullptr, 0, nullptr, 1, &imgBarrier);

	//// Fill the image with red color
	//VkClearColorValue clearColor = {
	//	.float32 = { 1.0f, 0.0f, 0.0f, 1.0f },
	//};
	//VkImageSubresourceRange fillRange = {
	//	.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	//	.baseMipLevel = 0,
	//	.levelCount = 1,
	//	.baseArrayLayer = 0,
	//	.layerCount = 1,
	//};
	//vkCmdClearColorImage(vkCmdBuf, vkSwapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &fillRange);

	// Transfer the swapchain image to the present layout.
	VkImageMemoryBarrier presentImgBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = swapchainImage,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	vkCmdPipelineBarrier(
		vkCmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		0, nullptr, 0, nullptr, 1, &presentImgBarrier);


	VkClearValue clearValue = { 0.75f, 0.0f, 0.0f, 1.0f };
	VkRenderPassBeginInfo renderPassBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_context->GetSwapchainRenderPass(),
		.framebuffer = m_context->GetSwapchainFramebuffer(),
		.renderArea = m_context->GetSwapchainRect(),
		.clearValueCount = 1,
		.pClearValues = &clearValue,
	};
	vkCmdBeginRenderPass(vkCmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	//// ImGui render.
	//ImGui::Render();
	//ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdBuf);

	vkCmdEndRenderPass(vkCmdBuf);

	// End the command buffer.
	VKCall(vkEndCommandBuffer(vkCmdBuf));


	m_context->EndFrame(std::span<VkCommandBuffer>(&vkCmdBuf, 1), true);
}

bool VKRTApplication::Shutdown()
{
	// Wait for all vulkan operations to complete.
	m_context->DeviceWaitIdle();

	// ImGuid shutdown.
	//ImGui_ImplVulkan_Shutdown();
	//ImGui_ImplSDL2_Shutdown();
	//ImGui::DestroyContext();

	// Release the command pool.

	m_cmdPoolCtx.reset();

	m_context.reset();

	return true;
}

int main(int argc, char** argv)
{	
	VKRTApplication app;
	app.Run(1280, 720);
	
	return 0;
}