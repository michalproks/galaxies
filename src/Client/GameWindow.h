#ifndef GALAXIES_CLIENT_GAME_WINDOW_H
#define GALAXIES_CLIENT_GAME_WINDOW_H

#include "Camera.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Client {

enum class CameraCommandType : std::uint8_t {
    PAN,
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_AT,
    RIGHT_DRAG_BEGIN,
    RIGHT_DRAG_UPDATE,
    RIGHT_DRAG_END,
};

/** Carries world deltas for PAN, pixel positions for ZOOM_AT and RIGHT_DRAG_BEGIN, pixel deltas for drag updates,
 * and a positive zoom multiplier for ZOOM_AT.
 */
struct CameraCommand {
    CameraCommandType type{CameraCommandType::PAN};
    double x{};
    double y{};
    double value{1.0};
};

class GameWindow {
public:
    /** Initializes SDL/Vulkan window support and centers its camera on the star. */
    GameWindow();

    /** Releases the SDL window, Vulkan library, and SDL video subsystem. */
    ~GameWindow();

    GameWindow(GameWindow const &) = delete;
    GameWindow &operator=(GameWindow const &) = delete;

    /** Returns the SDL window used for event processing and Vulkan surface creation. */
    [[nodiscard]] SDL_Window *nativeWindow() const;

    /** Pumps SDL events, records camera commands, and adds a frame-time-scaled WASD pan.
     * The elapsed frame time must be non-negative and finite. Returns false after a quit or close event.
     */
    [[nodiscard]] bool processEvents(double frameSeconds);

    /** Applies pending client-side camera commands and clears the command list. */
    void updateCamera();

    /** Returns the window-owned camera. */
    [[nodiscard]] Camera const &camera() const;

    /** Creates a Vulkan surface for this window; the caller owns the returned surface. */
    [[nodiscard]] VkSurfaceKHR createVulkanSurface(VkInstance instance) const;

private:
    [[nodiscard]] Simulation::Vector2 toPixelPosition(float x, float y) const;
    [[nodiscard]] Simulation::Vector2 toPixelDelta(float x, float y) const;
    [[nodiscard]] VkExtent2D pixelExtent() const;

    SDL_Window *mWindow{};
    Camera mCamera;
    std::vector<CameraCommand> mCameraCommands;
    Simulation::Vector2 mDragAnchorWorld{};
    Simulation::Vector2 mDragCursor{};
    bool mRightButtonDown{};
    bool mDragging{};
};

} // namespace Client

#endif // GALAXIES_CLIENT_GAME_WINDOW_H
