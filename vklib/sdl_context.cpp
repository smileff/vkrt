#include <iostream>
#include <vector>
#include <cassert>
#include "sdl_context.h"

SDLContext::~SDLContext()
{
	if (m_mainWin) {
		SDL_DestroyWindow(m_mainWin);
		m_mainWin = nullptr;
	}
	SDL_Quit();
}

bool SDLContext::Run()
{
	// Initialize SDL.
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		std::cerr << "Failed to initialize SDL." << std::endl;
		return false;
	}

	// Create SDL window.
	uint32_t sdlWinFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	m_mainWin = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_winWidth, m_winHeight, sdlWinFlags);
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
	VkInstance vkInstance = VK_NULL_HANDLE;
	if (!VKCreateInstance(vkInstance, vkInstExts.size(), vkInstExts.data(), vkEnableLayers.size(), vkEnableLayers.data())) {
		return false;
	}

	// Create SDL surface.
	VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(m_mainWin, vkInstance, &vkSurface)) {
		std::cerr << "Failed to create SDL vulkan surface." << std::endl;
		return false;
	}

	// Initialize application.
	if (!Initialize(m_mainWin, vkInstance, vkSurface)) {
		std::cerr << "Failed to initialize application." << std::endl;
		return false;
	}

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

		double frameSeconds = m_fpsCounter.CalcFrameSeconds();
		double fps = m_fpsCounter.GetFPS();
		RunOneFrame(frameSeconds, fps);
	}

	if (!Shutdown()) {
		std::cerr << "Failed to shutdown application." << std::endl;
		return false;
	}

	return true;
}

