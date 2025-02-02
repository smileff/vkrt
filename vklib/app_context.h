#ifndef _APP_CONTEXT_H_
#define _APP_CONTEXT_H_

#include "sdl_context.h"
#include "vk_context.h"
#include "imgui_function.h"

struct AppParameters
{
	const char* WindowTitle = "";
	int WindowWidth = 1280;
	int WindowHeight = 800;
};

struct InitParameters
{
	SDL_Window* window = nullptr;
	VkInstance vkInst = VK_NULL_HANDLE;
	VkSurfaceKHR vkSurf = VK_NULL_HANDLE;
};

class AppContext
{
public:
	bool Run(const AppParameters& appParams);

protected:
	SDLContext m_sdlContext;

private:
	virtual bool Initialize(const InitParameters& initPrams);
	virtual void HandleInput(SDL_Event& event);
	virtual void RunOneFrame();
	virtual bool Shutdown();
};

#endif
