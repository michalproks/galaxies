#ifndef GALAXIES_CLIENT_GAME_WINDOW_H
#define GALAXIES_CLIENT_GAME_WINDOW_H

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

namespace Client {

class GameWindow {
public:
    GameWindow();
    ~GameWindow();

    GameWindow(GameWindow const &) = delete;
    GameWindow &operator=(GameWindow const &) = delete;

    /** Returns the SDL window used for event processing and Vulkan surface creation. */
    [[nodiscard]] SDL_Window *nativeWindow() const;

    /** Processes pending SDL events and returns false after a quit or close event. */
    [[nodiscard]] bool processEvents() const;

    /** Creates a Vulkan surface for this window; the caller owns the returned surface. */
    [[nodiscard]] VkSurfaceKHR createVulkanSurface(VkInstance instance) const;

private:
    SDL_Window *mWindow{};
};

} // namespace Client

#endif // GALAXIES_CLIENT_GAME_WINDOW_H
