#include "GameWindow.h"

#include <SDL3/SDL_vulkan.h>

#include <stdexcept>
#include <string>

namespace Client {

GameWindow::GameWindow() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        std::string const error = std::string("SDL_Vulkan_LoadLibrary failed: ") + SDL_GetError();
        SDL_Quit();
        throw std::runtime_error(error);
    }

    mWindow = SDL_CreateWindow("Galaxies", 1280, 800, SDL_WINDOW_VULKAN);
    if (mWindow == nullptr) {
        std::string const error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        throw std::runtime_error(error);
    }
}

GameWindow::~GameWindow() {
    SDL_DestroyWindow(mWindow);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
}

SDL_Window *GameWindow::nativeWindow() const {
    return mWindow;
}

bool GameWindow::processEvents() const {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(mWindow)) {
            return false;
        }
    }
    return true;
}

VkSurfaceKHR GameWindow::createVulkanSurface(VkInstance instance) const {
    VkSurfaceKHR surface{};
    if (!SDL_Vulkan_CreateSurface(mWindow, instance, nullptr, &surface)) {
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    return surface;
}

} // namespace Client
