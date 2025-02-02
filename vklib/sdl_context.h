#ifndef _SDL_CONTEXT_H_
#define _SDL_CONTEXT_H_

#include <exception>
#include <vector>

#include "vk_function.h"
#include "imgui_function.h"
#include "fps_counter.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

class SDLContext
{
public:
	virtual ~SDLContext();

	bool Run();

	virtual bool Initialize(SDL_Window*, VkInstance, VkSurfaceKHR) { return true; }
	virtual void HandleInput(SDL_Event& event) {}
	virtual void RunOneFrame(double frameSeconds, double FPS) {}
	virtual bool Shutdown() { return true; }
private:
	int m_winWidth = 1280;
	int m_winHeight = 800;
 	SDL_Window *m_mainWin = nullptr;
	FPSCounter m_fpsCounter;
};

#endif