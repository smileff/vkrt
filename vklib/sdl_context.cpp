#include <iostream>
#include <vector>
#include <cassert>
#include "sdl_context.h"

SDLContext::~SDLContext()
{
	if (m_vkSurface) {
		vkDestroySurfaceKHR(m_vkInstance, m_vkSurface, nullptr);
		m_vkSurface = VK_NULL_HANDLE;
	}

	if (m_vkInstance) {
		vkDestroyInstance(m_vkInstance, nullptr);
		m_vkInstance = VK_NULL_HANDLE;
	}

	if (m_mainWin) {
		SDL_DestroyWindow(m_mainWin);
		m_mainWin = nullptr;
	}

	SDL_Quit();
}

//bool SDLContext::Init(int winWidth, int winHeight)
//{
//	// Initialize SDL.
//	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
//		std::cerr << "Failed to initialize SDL." << std::endl;
//		return false;
//	}
//
//	// Create SDL window.
//	uint32_t sdlWinFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
//	m_mainWin = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winWidth, winHeight, sdlWinFlags);
//	if (!m_mainWin) {
//		std::cerr << "Failed to create SDL window." << std::endl;
//		return false;
//	}
//
//	// Get the vk instance extensions required.
//	unsigned int extNum = 0;
//	SDL_Vulkan_GetInstanceExtensions(m_mainWin, &extNum, nullptr);
//	std::vector<const char*> vkInstExts(extNum);
//	SDL_Vulkan_GetInstanceExtensions(m_mainWin, &extNum, vkInstExts.data());
//	// vkInstExts.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
//
//	// Determine the to-enable layers
//	std::vector<const char*> vkEnableLayers = {
//		"VK_LAYER_KHRONOS_validation",	// Enable validation layer.
//	};
//
//	// Create vulkan instance.
//	if (!VKCreateInstance(m_vkInstance, vkInstExts.size(), vkInstExts.data(), vkEnableLayers.size(), vkEnableLayers.data(), nullptr)) {
//		return false;
//	}
//
//	// Create SDL surface.
//	if (!SDL_Vulkan_CreateSurface(m_mainWin, m_vkInstance, &m_vkSurface)) {
//		std::cerr << "Failed to create SDL vulkan surface." << std::endl;
//		return false;
//	}
//
//	return true;
//}

bool SDLContext::Run(int winWidth, int winHeight)
{
	// Initialize SDL.
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		std::cerr << "Failed to initialize SDL." << std::endl;
		return false;
	}

	// Create SDL window.
	uint32_t sdlWinFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	m_mainWin = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winWidth, winHeight, sdlWinFlags);
	if (!m_mainWin) {
		std::cerr << "Failed to create SDL window." << std::endl;
		return false;
	}

	// Get the vk instance extensions required.
	unsigned int extNum = 0;
	SDL_Vulkan_GetInstanceExtensions(m_mainWin, &extNum, nullptr);
	std::vector<const char*> vkInstExts(extNum);
	SDL_Vulkan_GetInstanceExtensions(m_mainWin, &extNum, vkInstExts.data());
	// vkInstExts.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	// Determine the to-enable layers
	std::vector<const char*> vkEnableLayers = {
		"VK_LAYER_KHRONOS_validation",	// Enable validation layer.
	};

	// Create vulkan instance.
	if (!VKCreateInstance(m_vkInstance, vkInstExts.size(), vkInstExts.data(), vkEnableLayers.size(), vkEnableLayers.data(), nullptr)) {
		return false;
	}

	// Create SDL surface.
	if (!SDL_Vulkan_CreateSurface(m_mainWin, m_vkInstance, &m_vkSurface)) {
		std::cerr << "Failed to create SDL vulkan surface." << std::endl;
		return false;
	}

	// Initialize application.
	if (!Initialize(m_mainWin, m_vkInstance, m_vkSurface)) {
		std::cerr << "Failed to initialize application." << std::endl;
		Shutdown();
		return false;
	}

	FPSCounter fpsCounter;
	// Window loop.
	bool quit = false;
	while (!quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				quit = true;
			}

			// Handle 
			HandleInput(event);
		}

		double frameSeconds = fpsCounter.CalcFrameSeconds();
		double fps = fpsCounter.GetFPS();
		RunOneFrame(frameSeconds, fps);
	}

	if (!Shutdown()) {
		std::cerr << "Failed to shutdown application." << std::endl;
		return false;
	}

	return true;
}

//bool RunSingleVkDeviceSDLApp(int winWidth, int winHeight, std::unique_ptr<SDLApp> app)
//{
//	SDLContext sdlCtx;
//	if (!sdlCtx.Init(winWidth, winHeight)) {
//		return false;
//	}
//
//	SDL_Window* window = sdlCtx.GetSDLWindow();
//	VkInstance vkInstance = sdlCtx.GetVkInstance();
//	VkSurfaceKHR vkSurface = sdlCtx.GetVkSurface();
//
//	// Pick physical device and queue family.
//	VkPhysicalDevice vkPhysicalDevice;
//	uint32_t vkQueueFamilyIdx;
//	if (!VKPickPhysicalDeviceAndOneQueueFamily(vkInstance, vkSurface, &vkPhysicalDevice, &vkQueueFamilyIdx)) {
//		return false;
//	}
//
//	// 
//	std::unique_ptr<VKDeviceContext> vkDeviceCtx = std::make_unique<VKDeviceContext>("DeviceContext");
//
//	std::vector<VKDeviceQueueCreateRequest> vkQueueCreateRequests = {
//		{ vkQueueFamilyIdx, 1, {1.0f} },
//	};
//	const char* extensionNames[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
//	if (!vkDeviceCtx->CreateDevice(vkInstance, vkPhysicalDevice, vkQueueCreateRequests.size(), vkQueueCreateRequests.data(), 0, nullptr, 1, extensionNames)) {
//		return false;
//	}
//	if (!vkDeviceCtx->CreateGlobalObjects()) {
//		return false;
//	}
//
//	// Initialize application.
//	if (!app->Initialize(window, vkSurface, *vkDeviceCtx)) {
//		std::cerr << "Failed to initialize application." << std::endl;
//		return false;
//	}
//
//	FPSCounter fpsCounter;
//	// Window loop.
//	bool quit = false;
//	while (!quit) {
//		SDL_Event event;
//		while (SDL_PollEvent(&event)) {
//			if (event.type == SDL_QUIT) {
//				quit = true;
//			}
//
//			// Handle 
//			app->HandleInput(event);
//		}
//
//		double frameSeconds = fpsCounter.CalcFrameSeconds();
//		double fps = fpsCounter.GetFPS();
//		app->RunOneFrame(frameSeconds, fps);
//	}
//
//	if (!app->Shutdown()) {
//		std::cerr << "Failed to shutdown application." << std::endl;
//		return false;
//	}
//
//	app.reset();
//	vkDeviceCtx.reset();
//
//	return true;
//}
