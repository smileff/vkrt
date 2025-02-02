#include "vklib/vklib.h"

class VKCapViewerApp : public SDLContext
{
private:
	virtual bool Initialize(SDL_Window* window, VkInstance vkInst, VkSurfaceKHR vkSurf) override
	{
		m_window = window;
		m_vkInstance = vkInst;
		m_vkSurface = vkSurf;

		// Get rt size.
		SDL_Vulkan_GetDrawableSize(window, (int*)&m_rtWidth, (int*)&m_rtHeight);

		// Pick physical device and queue family.
		if (!VKPickPhysicalDeviceAndOneQueueFamily(m_vkInstance, m_vkSurface, &m_vkPhysicalDevice, &m_vkQueueFamilyIdx)) {
			return false;
		}

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

		// Determine the main surface format.
		VkSurfaceFormatKHR vkSwapchainSurfaceFormat;
		if (!VKPickSurfaceFormat(m_vkPhysicalDevice, m_vkSurface, vkSwapchainSurfaceFormat)) {
			return false;
		}

		if (!m_swapchainContext.InitSwapchain(m_vkDevice, m_vkQueueFamilyIdx, m_vkSurface, vkSwapchainSurfaceFormat, m_rtWidth, m_rtHeight)) {
			return false;
		}

		if (!m_inflightContext.InitInflight(m_vkDevice)) {
			return false;
		}

		// Initialize ImGui.
		ImGuiInitialize(
			window,
			m_vkInstance,
			m_vkPhysicalDevice,
			m_vkDevice,
			m_vkQueueFamilyIdx,
			m_vkQueue,
			m_swapchainContext.GetSwapchainImageNum(),
			m_swapchainContext.GetSwapchainRenderPass());

		// Create command pool.
		{
			VkCommandPoolCreateInfo vkCmdPoolCInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = m_vkQueueFamilyIdx,
			};
			VKCall(vkCreateCommandPool(m_vkDevice, &vkCmdPoolCInfo, g_vkAllocator, &m_vkCmdPool));
		}

		// Create command buffer.
		uint32_t inflightFrameNum = m_inflightContext.GetInflightFrameNum();
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
		if (!m_inflightContext.WaitForInflightFrameFence()) {
			return;
		}

		// ImGui render.
		ImGuiBeginFrame();
		ImGui::ShowDemoWindow();

		uint32_t inflightFrameIdx = m_inflightContext.GetInflightFrameIdx();
		VkCommandBuffer cmdBuf = m_vkCmdBufs[inflightFrameIdx];

		m_swapchainContext.AcquireNextSwapchainImage(m_inflightContext.GetImageAvailableSemaphore(), UINT64_MAX);

		// Do rendering.
		VKCall(vkResetCommandBuffer(cmdBuf, 0));

		VkCommandBufferBeginInfo cmdBufBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		VKCall(vkBeginCommandBuffer(cmdBuf, &cmdBufBeginInfo));


		VkClearValue clearValue = { VkClearColorValue{ 0.1f, 0.0f, 0.0f, 1.0f } };
		VkRenderPassBeginInfo renderPassBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = m_swapchainContext.GetSwapchainRenderPass(),
			.framebuffer = m_swapchainContext.GetSwapchainFramebuffer(),
			.renderArea = m_swapchainContext.GetSwapchainRect(),
			.clearValueCount = 1,
			.pClearValues = &clearValue,
		};
		vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

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
			.pWaitSemaphores = &m_inflightContext.GetImageAvailableSemaphore(),
			.pWaitDstStageMask = waitStages,
			.commandBufferCount = (uint32_t)1,
			.pCommandBuffers = &cmdBuf,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &m_inflightContext.GetRenderFinishedSemaphore(),
		};
		VKCall(vkQueueSubmit(m_vkQueue, 1, &submitInfo, m_inflightContext.GetInflightFrameFence()));

		// Present.
		VkPresentInfoKHR presentInfo = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_inflightContext.GetRenderFinishedSemaphore(),
			.swapchainCount = 1,
			.pSwapchains = &m_swapchainContext.GetSwapchain(),
			.pImageIndices = &m_swapchainContext.GetSwapchainImageIdx(),
		};
		vkQueuePresentKHR(m_vkQueue, &presentInfo);

		m_inflightContext.NextFrame();
	}

	virtual bool Shutdown() override
	{
		vkDeviceWaitIdle(m_vkDevice);

		if (m_vkCmdPool) {
			vkDestroyCommandPool(m_vkDevice, m_vkCmdPool, g_vkAllocator);
		}

		ImGuiShutdown();

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

		if (m_vkSurface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(m_vkInstance, m_vkSurface, g_vkAllocator);
			m_vkSurface = VK_NULL_HANDLE;
		}

		if (m_vkInstance) {
			vkDestroyInstance(m_vkInstance, g_vkAllocator);
			m_vkInstance = VK_NULL_HANDLE;
		}

		return true;
	}

private:
	SDL_Window* m_window = nullptr;

	uint32_t m_rtWidth = 0, m_rtHeight = 0;
	VkInstance m_vkInstance = VK_NULL_HANDLE;
	VkSurfaceKHR m_vkSurface = VK_NULL_HANDLE;
	VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_vkDevice = VK_NULL_HANDLE;
	uint32_t m_vkQueueFamilyIdx = (uint32_t)-1;
	VkQueue m_vkQueue = VK_NULL_HANDLE;

	VKSwapchainContext m_swapchainContext;
	VKInflightContext m_inflightContext;

	VkCommandPool m_vkCmdPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_vkCmdBufs;
};

int main(int argc, char** argv)
{
	VKCapViewerApp app;
	app.Run();

	return 0;
}