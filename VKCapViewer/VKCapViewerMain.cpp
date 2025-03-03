#include "vklib/vklib.h"

class VKCapViewerApp : public SDLContext
{
public:
	virtual bool Initialize(SDL_Window* sdlWin, const VkInstance& vkInst, const VkSurfaceKHR& vkSurf) override
	{
		m_window = sdlWin;
		m_vkSurface = vkSurf;

		// Pick physical device and queue family.
		VkPhysicalDevice vkPhysicalDevice;
		uint32_t vkQueueFamilyIdx;
		if (!VKPickPhysicalDeviceAndOneQueueFamily(vkInst, vkSurf, &vkPhysicalDevice, &vkQueueFamilyIdx)) {
			return false;
		}

		// 
		m_ctx = std::make_unique<VKDeviceContext>("DeviceContext");

		std::vector<VKDeviceQueueCreateRequest> vkQueueCreateRequests = {
			{ vkQueueFamilyIdx, 1, {1.0f} },
		};
		const char* extensionNames[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		if (!m_ctx->CreateDevice(vkInst, vkPhysicalDevice, vkQueueCreateRequests.size(), vkQueueCreateRequests.data(), 0, nullptr, 1, extensionNames)) {
			return false;
		}
		if (!m_ctx->CreateGlobalObjects()) {
			return false;
		}

		m_vkDevice = m_ctx->GetDevice();
		m_vkQueueFamilyIdx = m_ctx->GetQueueFamilyIdx(0);
		m_vkQueue = m_ctx->GetQueueFamilyQueue(0, 0);

		// Determine the main surface format.
		VkSurfaceFormatKHR vkSwapchainSurfaceFormat;
		if (!VKPickSurfaceFormat(m_ctx->GetPhysicalDevice(), m_vkSurface, vkSwapchainSurfaceFormat)) {
			return false;
		}

		// Get rt size.
		uint32_t rtWidth, rtHeight;
		SDL_Vulkan_GetDrawableSize(m_window, (int*)&rtWidth, (int*)&rtHeight);

		m_swapchainContext = std::make_unique<VKSwapchainContext>(*m_ctx);
		if (!m_swapchainContext->InitSwapchain(m_vkQueueFamilyIdx, m_vkSurface, vkSwapchainSurfaceFormat, rtWidth, rtHeight)) {
			return false;
		}

		m_inflightContext = std::make_unique<VKInflightContext>(*m_ctx);
		if (!m_inflightContext->InitInflight()) {
			return false;
		}

		// Initialize ImGui.
		ImGuiInitialize(
			m_window,
			m_ctx->GetInstance(),
			m_ctx->GetPhysicalDevice(),
			m_vkDevice,
			m_vkQueueFamilyIdx,
			m_vkQueue,
			m_swapchainContext->GetSwapchainImageNum(),
			m_swapchainContext->GetSwapchainRenderPass());

		// Create command pool.
		{
			VkCommandPoolCreateInfo vkCmdPoolCInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = m_vkQueueFamilyIdx,
			};
			VKCall(vkCreateCommandPool(m_vkDevice, &vkCmdPoolCInfo, nullptr, &m_vkCmdPool));
		}

		// Create command buffer.
		uint32_t inflightFrameNum = m_inflightContext->GetInflightFrameNum();
		m_vkCmdBufs.resize(inflightFrameNum);
		{
			VkCommandBufferAllocateInfo vkCmdBufAllocInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = m_vkCmdPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = (uint32_t)m_vkCmdBufs.size(),
			};
			VKCall(vkAllocateCommandBuffers(m_vkDevice, &vkCmdBufAllocInfo, m_vkCmdBufs.data()));
		}

		// Create pipeline.
		//std::string vertShaderPath = "shaders/test.vert";
		//std::string fragShaderPath = "shaders/test.frag";
		//m_vkGraphicsPipeline = std::make_unique<TestGraphicsPipeline>(*m_ctx, "TestGraphicsPipeline");
		//if (!m_vkGraphicsPipeline->CreateGraphicsPipeline(m_swapchainContext->GetSwapchainRenderPass(), 0)) {
		//	return false;
		//}

		if (!VKCreateGraphicsPipelineTest(m_ctx->GetDevice(), m_swapchainContext->GetSwapchainRenderPass(), 0, m_vkGraphicsPipeline)) {
			return false;
		}

		//VkPipeline graphicsPipeline = VK_NULL_HANDLE;
		//VKCreateGraphicsPipelineTest(m_vkDevice, graphicsPipeline);

		return true;
	}

	virtual void HandleInput(SDL_Event& event) override
	{
		ImGui_ImplSDL2_ProcessEvent(&event);
	}

	virtual void RunOneFrame(double frameSeconds, double FPS) override
	{
		// Update fps to window title.
		std::stringstream ss;
		ss << "FPS: " << std::setprecision(3) << std::fixed << FPS;
		SDL_SetWindowTitle(m_window, ss.str().c_str());

		// Wait for the previous frame to finish.
		if (!m_inflightContext->WaitForInflightFrameFence()) {
			return;
		}

		// ImGui render.
		ImGuiBeginFrame();
		ImGui::ShowDemoWindow();

		uint32_t inflightFrameIdx = m_inflightContext->GetInflightFrameIdx();
		VkCommandBuffer cmdBuf = m_vkCmdBufs[inflightFrameIdx];

		m_swapchainContext->AcquireNextSwapchainImage(m_inflightContext->GetImageAvailableSemaphore(), UINT64_MAX);

		// Do rendering.
		VKCall(vkResetCommandBuffer(cmdBuf, 0));

		VkCommandBufferBeginInfo cmdBufBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		VKCall(vkBeginCommandBuffer(cmdBuf, &cmdBufBeginInfo));

		// Set viewport and scissor.
		VkRect2D swapchainRect = m_swapchainContext->GetSwapchainRect();
		VkViewport viewport = { 0, 0, (float)swapchainRect.extent.width, (float)swapchainRect.extent.height, 0.0f, 1.0f };
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &swapchainRect);

		VkClearValue clearValue = { VkClearColorValue{ 0.1f, 0.0f, 0.0f, 1.0f } };
		VkRenderPassBeginInfo renderPassBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = m_swapchainContext->GetSwapchainRenderPass(),
			.framebuffer = m_swapchainContext->GetSwapchainFramebuffer(),
			.renderArea = m_swapchainContext->GetSwapchainRect(),
			.clearValueCount = 1,
			.pClearValues = &clearValue,
		};
		vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkGraphicsPipeline);

		vkCmdDraw(cmdBuf, 3, 1, 0, 0);

		// ImGui render.
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

		vkCmdEndRenderPass(cmdBuf);

		VKCall(vkEndCommandBuffer(cmdBuf));

		// Render.
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_inflightContext->GetImageAvailableSemaphore(),
			.pWaitDstStageMask = waitStages,
			.commandBufferCount = (uint32_t)1,
			.pCommandBuffers = &cmdBuf,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &m_inflightContext->GetRenderFinishedSemaphore(),
		};
		VKCall(vkQueueSubmit(m_vkQueue, 1, &submitInfo, m_inflightContext->GetInflightFrameFence()));

		// Present.
		VkPresentInfoKHR presentInfo = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_inflightContext->GetRenderFinishedSemaphore(),
			.swapchainCount = 1,
			.pSwapchains = &m_swapchainContext->GetSwapchain(),
			.pImageIndices = &m_swapchainContext->GetSwapchainImageIdx(),
		};
		vkQueuePresentKHR(m_vkQueue, &presentInfo);

		m_inflightContext->NextFrame();
	}

	virtual bool Shutdown() override
	{
		if (m_vkDevice) {
			vkDeviceWaitIdle(m_vkDevice);
		}

		if (m_vkCmdPool) {
			vkDestroyCommandPool(m_vkDevice, m_vkCmdPool, nullptr);
		}

		ImGuiShutdown();

		m_vkQueue = VK_NULL_HANDLE;

		vkDestroyPipeline(m_vkDevice, m_vkGraphicsPipeline, nullptr);

		//m_vkGraphicsPipeline.reset();
		m_swapchainContext.reset();
		m_inflightContext.reset();

		m_ctx.reset();

		return true;
	}

private:
	// VkAllocationCallbacks* m_vkAllocator = nullptr;
	//uint32_t m_rtWidth = 0, m_rtHeight = 0;
	//VkInstance m_vkInstance = VK_NULL_HANDLE;
	//VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
	// VkSurfaceKHR m_vkSurface = VK_NULL_HANDLE;

	SDL_Window* m_window = nullptr;
	VkSurfaceKHR m_vkSurface = VK_NULL_HANDLE;

	std::unique_ptr<VKDeviceContext> m_ctx;

	VkDevice m_vkDevice = VK_NULL_HANDLE;
	uint32_t m_vkQueueFamilyIdx = (uint32_t)-1;
	VkQueue m_vkQueue = VK_NULL_HANDLE;

	std::unique_ptr<VKSwapchainContext> m_swapchainContext;
	std::unique_ptr<VKInflightContext> m_inflightContext;

	VkCommandPool m_vkCmdPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_vkCmdBufs;

	//std::unique_ptr<TestGraphicsPipeline> m_vkGraphicsPipeline;
	VkPipeline m_vkGraphicsPipeline = VK_NULL_HANDLE;
};

int main(int argc, char** argv)
{
	//RunSingleVkDeviceSDLApp(1280, 800, std::make_unique<VKCapViewerApp>());
	
	VKCapViewerApp app;
	app.Run(1280, 800);

	return 0;
}