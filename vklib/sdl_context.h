#ifndef _SDL_CONTEXT_H_
#define _SDL_CONTEXT_H_

#include <exception>
#include <vector>
#include "vulkan/vulkan.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

// SDLVulkanApplication, the framework of a SDL application using vulkan. Encapsulate:
// * SDL library initializtion, window creation.
// * The vulkan instance creation.
class SDLVulkanApplication
{
public:
	// Call this function to run the applicaiton.
	bool Run(int winWidth, int winHeight);

	// Override the following virtual functions.
	virtual bool Initialize(SDL_Window* sdlWin, const VkInstance& vkInst, const VkSurfaceKHR& vkSurf) { return true; }
	virtual void HandleInput(SDL_Event& event) {}
	virtual void RunOneFrame(double frameSeconds, double FPS) {}
	virtual bool Shutdown() { return true; }

	virtual ~SDLVulkanApplication();
private:
 	SDL_Window *m_mainWin = nullptr;
	VkInstance m_vkInstance = VK_NULL_HANDLE;
	VkSurfaceKHR m_vkSurface = VK_NULL_HANDLE;
};

// App works on one physial device.
//class SDLApp
//{
//public:
//	virtual bool Initialize(SDL_Window* sdlWin, const VkSurfaceKHR& surf, const VKDeviceContext& vkDeviceCtx) { return true; }
//	virtual void HandleInput(SDL_Event& event) {}
//	virtual void RunOneFrame(double frameSeconds, double FPS) {}
//	virtual bool Shutdown() { return true; }
//};
//
//bool RunSingleVkDeviceSDLApp(int winWidth, int winHeight, std::unique_ptr<SDLApp> app);

#endif