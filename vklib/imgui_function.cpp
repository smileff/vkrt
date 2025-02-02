#include "imgui_function.h"

void ImGuiInitialize(
	SDL_Window* sdlWindow, 
	VkInstance vkInst, 
	VkPhysicalDevice vkPhysicalDevice, 
	VkDevice vkDevice, 
	uint32_t vkQueueFamilyIdx, 
	VkQueue vkQueue, 
	uint32_t vkSwapchainImageNum,
	VkRenderPass vkSwapchainRenderPass)
{
	// Initialize imgUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	// io.ConfigFlags != ImGuiConfigFlags_DockingEnable;

	ImGui_ImplSDL2_InitForVulkan(sdlWindow);
	ImGui_ImplVulkan_InitInfo init_info = {
		.Instance = vkInst,
		.PhysicalDevice = vkPhysicalDevice,
		.Device = vkDevice,
		.QueueFamily = vkQueueFamilyIdx,
		.Queue = vkQueue,
		.DescriptorPool = VK_NULL_HANDLE,
		.RenderPass = vkSwapchainRenderPass,
		.MinImageCount = vkSwapchainImageNum,
		.ImageCount = vkSwapchainImageNum,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		.PipelineCache = VK_NULL_HANDLE,
		.Subpass = 0,
		.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE * 2,
		.Allocator = g_vkAllocator,
	};
	ImGui_ImplVulkan_Init(&init_info);
}

void ImGuiBeginFrame()
{
	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplVulkan_NewFrame();
	ImGui::NewFrame();
}

void ImGuiShutdown()
{
	// ImGuid shutdown.
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

