#ifndef _IMGUI_FUNCTION_H_
#define _IMGUI_FUNCTION_H_

#include "sdl_context.h"
#include "vk_context.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

void ImGuiInitialize(
	SDL_Window* sdlWindow,
	VkInstance vkInst,
	VkPhysicalDevice vkPhysicalDevice,
	VkDevice vkDevice,
	uint32_t vkQueueFamilyIdx,
	VkQueue vkQueue,
	uint32_t vkSwapchainImageNum,
	VkRenderPass vkSwapchainRenderPass);

void ImGuiBeginFrame();

void ImGuiShutdown();

#endif