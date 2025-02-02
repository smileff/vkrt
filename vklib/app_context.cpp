#include "app_context.h"

bool AppContext::Initialize(const InitParameters& initParams)
{
	return true;
}

void AppContext::HandleInput(SDL_Event& sdlEvent)
{}

void AppContext::RunOneFrame()
{}

bool AppContext::Shutdown()
{
	return true;
}

bool AppContext::Run(const AppParameters& appParams)
{
	// Create window.
	m_sdlContext.CreateWindow(appParams.WindowTitle, appParams.WindowWidth, appParams.WindowHeight);

	// Get the vk instance extensions required.
	std::vector<const char*> vkInstExts;
	m_sdlContext.GetRequiredVkInstanceExtensions(&vkInstExts);
	vkInstExts.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

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
	if (!m_sdlContext.CreateVkSurface(vkInstance, &vkSurface)) {
		std::cerr << "Failed to create SDL vulkan surface." << std::endl;
		return false;
	}

	// Get drawable size.
	uint32_t rtWidth = 0, rtHeight = 0;
	m_sdlContext.GetDrawableSize(&rtWidth, &rtHeight);

	// Initialize application.
	InitParameters initParams = {
		.window = m_sdlContext.GetWindow(),
		.vkInst = vkInstance,
		.vkSurf = vkSurface,
	};
	if (!Initialize(initParams)) {
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

		RunOneFrame();
	}

	if (!Shutdown()) {
		std::cerr << "Failed to shutdown application." << std::endl;
		return false;
	}

	return true;
}

