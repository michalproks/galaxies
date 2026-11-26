#include "GameWindow.h"

#include <SDL3/SDL_vulkan.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace Client {
namespace {

    constexpr double PAN_SPEED = 5.0;
    constexpr double ZOOM_FACTOR = 1.1;

    /** True when the previous ImGui frame had the mouse over a window. */
    bool uiWantsMouse() {
        return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
    }

} // namespace

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

    try {
        VkExtent2D const extent = pixelExtent();
        mCamera = Camera({}, Camera::initialScale(extent.width, extent.height));
    } catch (...) {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        throw;
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

bool GameWindow::processEvents(double frameSeconds, bool const blockMapInput) {
    if (!std::isfinite(frameSeconds) || frameSeconds < 0.0) {
        throw std::invalid_argument("Window input requires a non-negative, finite frame time.");
    }

    mCameraCommands.clear();
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            mCameraCommands.clear();
            return false;
        }

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(mWindow)) {
            mCameraCommands.clear();
            return false;
        }

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.windowID == SDL_GetWindowID(mWindow) && !event.key.repeat) {
            if (!blockMapInput && (event.key.key == SDLK_Z || event.key.key == SDLK_X)) {
                CameraCommandType const type =
                    event.key.key == SDLK_Z ? CameraCommandType::ZOOM_IN : CameraCommandType::ZOOM_OUT;
                mCameraCommands.push_back({type});
            } else if (event.key.key == SDLK_1 || event.key.key == SDLK_2 || event.key.key == SDLK_3) {
                switch (event.key.key) {
                case SDLK_1:
                    mPendingSpeed = Simulation::GameSpeed::NORMAL;
                    break;
                case SDLK_2:
                    mPendingSpeed = Simulation::GameSpeed::FAST;
                    break;
                case SDLK_3:
                    mPendingSpeed = Simulation::GameSpeed::ULTRA;
                    break;
                default:
                    break;
                }
            } else if (event.key.key == SDLK_SPACE) {
                mPendingPauseToggle = true;
            }
        }

        // Wheel zoom is suppressed only while the cursor is over an ImGui window, so economy
        // windows scroll themselves without also zooming the map behind them.
        if (!uiWantsMouse() && event.type == SDL_EVENT_MOUSE_WHEEL &&
            event.wheel.windowID == SDL_GetWindowID(mWindow)) {
            double direction = static_cast<double>(event.wheel.y);
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                direction = -direction;
            }
            if (direction != 0.0) {
                Simulation::Vector2 const cursor = toPixelPosition(event.wheel.mouse_x, event.wheel.mouse_y);
                double const factor = std::pow(ZOOM_FACTOR, std::abs(direction));
                mCameraCommands.push_back(
                    {CameraCommandType::ZOOM_AT, cursor.x, cursor.y, direction > 0.0 ? factor : 1.0 / factor});
            }
        }

        if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
            event.button.windowID == SDL_GetWindowID(mWindow) && event.button.button == SDL_BUTTON_MIDDLE) {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !mMiddleButtonDown && !blockMapInput) {
                Simulation::Vector2 const cursor = toPixelPosition(event.button.x, event.button.y);
                mCameraCommands.push_back({CameraCommandType::MIDDLE_DRAG_BEGIN, cursor.x, cursor.y});
                mMiddleButtonDown = true;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && mMiddleButtonDown) {
                mCameraCommands.push_back({CameraCommandType::MIDDLE_DRAG_END});
                mMiddleButtonDown = false;
            }
        }

        if (!blockMapInput && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.windowID == SDL_GetWindowID(mWindow) && event.button.button == SDL_BUTTON_LEFT) {
            mLeftClicks.push_back(toPixelPosition(event.button.x, event.button.y));
        }

        if (!blockMapInput && event.type == SDL_EVENT_MOUSE_MOTION &&
            event.motion.windowID == SDL_GetWindowID(mWindow) && mMiddleButtonDown) {
            Simulation::Vector2 const delta = toPixelDelta(event.motion.xrel, event.motion.yrel);
            mCameraCommands.push_back({CameraCommandType::MIDDLE_DRAG_UPDATE, delta.x, delta.y});
        }
    }

    int keyCount{};
    bool const *const keys = SDL_GetKeyboardState(&keyCount);
    if (!blockMapInput && keys != nullptr && keyCount > SDL_SCANCODE_S) {
        double horizontal = static_cast<double>(keys[SDL_SCANCODE_D]) - static_cast<double>(keys[SDL_SCANCODE_A]);
        double vertical = static_cast<double>(keys[SDL_SCANCODE_W]) - static_cast<double>(keys[SDL_SCANCODE_S]);
        double const directionLength = std::hypot(horizontal, vertical);
        if (directionLength > 1.0) {
            horizontal /= directionLength;
            vertical /= directionLength;
        }
        if (horizontal != 0.0 || vertical != 0.0) {
            mCameraCommands.push_back(
                {CameraCommandType::PAN, horizontal * PAN_SPEED * frameSeconds, vertical * PAN_SPEED * frameSeconds});
        }
    }
    return true;
}

void GameWindow::updateCamera() {
    VkExtent2D const extent = pixelExtent();
    for (CameraCommand const &command : mCameraCommands) {
        switch (command.type) {
        case CameraCommandType::PAN:
            mCamera.pan(command.x, command.y);
            break;
        case CameraCommandType::ZOOM_IN:
            mCamera.zoomIn();
            break;
        case CameraCommandType::ZOOM_OUT:
            mCamera.zoomOut();
            break;
        case CameraCommandType::ZOOM_AT:
            if (extent.width > 0 && extent.height > 0) {
                Simulation::Vector2 const worldPoint =
                    mCamera.screenToWorld({command.x, command.y}, extent.width, extent.height);
                mCamera.zoomAt(worldPoint, command.value);
            }
            break;
        case CameraCommandType::MIDDLE_DRAG_BEGIN:
            if (extent.width > 0 && extent.height > 0) {
                mDragCursor = {command.x, command.y};
                mDragAnchorWorld = mCamera.screenToWorld(mDragCursor, extent.width, extent.height);
                mDragging = true;
            }
            break;
        case CameraCommandType::MIDDLE_DRAG_UPDATE:
            if (mDragging && extent.width > 0 && extent.height > 0) {
                mDragCursor.x += command.x;
                mDragCursor.y += command.y;
                Simulation::Vector2 const currentCenter = mCamera.center();
                Simulation::Vector2 const desiredCenter{
                    mDragAnchorWorld.x - (mDragCursor.x - static_cast<double>(extent.width) / 2.0) / mCamera.scale(),
                    mDragAnchorWorld.y + (mDragCursor.y - static_cast<double>(extent.height) / 2.0) / mCamera.scale(),
                };
                mCamera.pan(desiredCenter.x - currentCenter.x, desiredCenter.y - currentCenter.y);
            }
            break;
        case CameraCommandType::MIDDLE_DRAG_END:
            mDragging = false;
            break;
        }
    }
    mCameraCommands.clear();
}

std::vector<Simulation::Vector2> GameWindow::consumeLeftClicks() {
    std::vector<Simulation::Vector2> clicks = std::move(mLeftClicks);
    mLeftClicks.clear();
    return clicks;
}

std::optional<Simulation::GameSpeed> GameWindow::consumeSpeedChange() {
    std::optional<Simulation::GameSpeed> speed = mPendingSpeed;
    mPendingSpeed.reset();
    return speed;
}

bool GameWindow::consumePauseToggle() {
    bool const toggle = mPendingPauseToggle;
    mPendingPauseToggle = false;
    return toggle;
}

Camera const &GameWindow::camera() const {
    return mCamera;
}

Simulation::Vector2 GameWindow::toPixelPosition(float x, float y) const {
    int windowWidth{};
    int windowHeight{};
    int pixelWidth{};
    int pixelHeight{};
    if (!SDL_GetWindowSize(mWindow, &windowWidth, &windowHeight) ||
        !SDL_GetWindowSizeInPixels(mWindow, &pixelWidth, &pixelHeight)) {
        throw std::runtime_error(std::string("SDL_GetWindowSize failed: ") + SDL_GetError());
    }
    if (windowWidth <= 0 || windowHeight <= 0 || pixelWidth <= 0 || pixelHeight <= 0) {
        return {};
    }
    return {
        static_cast<double>(x) * static_cast<double>(pixelWidth) / static_cast<double>(windowWidth),
        static_cast<double>(y) * static_cast<double>(pixelHeight) / static_cast<double>(windowHeight),
    };
}

Simulation::Vector2 GameWindow::toPixelDelta(float x, float y) const {
    int windowWidth{};
    int windowHeight{};
    int pixelWidth{};
    int pixelHeight{};
    if (!SDL_GetWindowSize(mWindow, &windowWidth, &windowHeight) ||
        !SDL_GetWindowSizeInPixels(mWindow, &pixelWidth, &pixelHeight)) {
        throw std::runtime_error(std::string("SDL_GetWindowSize failed: ") + SDL_GetError());
    }
    if (windowWidth <= 0 || windowHeight <= 0 || pixelWidth <= 0 || pixelHeight <= 0) {
        return {};
    }
    return {
        static_cast<double>(x) * static_cast<double>(pixelWidth) / static_cast<double>(windowWidth),
        static_cast<double>(y) * static_cast<double>(pixelHeight) / static_cast<double>(windowHeight),
    };
}

VkExtent2D GameWindow::pixelExtent() const {
    int pixelWidth{};
    int pixelHeight{};
    if (!SDL_GetWindowSizeInPixels(mWindow, &pixelWidth, &pixelHeight)) {
        throw std::runtime_error(std::string("SDL_GetWindowSizeInPixels failed: ") + SDL_GetError());
    }
    if (pixelWidth <= 0 || pixelHeight <= 0) {
        return {};
    }
    return {static_cast<std::uint32_t>(pixelWidth), static_cast<std::uint32_t>(pixelHeight)};
}

VkSurfaceKHR GameWindow::createVulkanSurface(VkInstance instance) const {
    VkSurfaceKHR surface{};
    if (!SDL_Vulkan_CreateSurface(mWindow, instance, nullptr, &surface)) {
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    return surface;
}

} // namespace Client
