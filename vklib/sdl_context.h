#ifndef _SDL_CONTEXT_H_
#define _SDL_CONTEXT_H_

#include <exception>
#include <vector>

#include "vk_function.h"
#include "imgui_function.h"
#include "fps_counter.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

// Hide the SDL context init/shutdown, and SDLWindows manangement.
class SDLContext
{
public:
	virtual ~SDLContext();

	//bool Init(int winWidth, int winHeight);
	//SDL_Window* GetSDLWindow() { return m_mainWin; }
	//VkSurfaceKHR GetVkSurface() { return m_vkSurface; }
	//VkInstance GetVkInstance() { return m_vkInstance; }

	virtual bool Initialize(SDL_Window* sdlWin, const VkInstance& vkInst, const VkSurfaceKHR& surf) { return true; }
	virtual void HandleInput(SDL_Event& event) {}
	virtual void RunOneFrame(double frameSeconds, double FPS) {}
	virtual bool Shutdown() { return true; }

	bool Run(int winWidth, int winHeight);

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