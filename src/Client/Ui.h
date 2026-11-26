#ifndef GALAXIES_CLIENT_UI_H
#define GALAXIES_CLIENT_UI_H

#include "Selection.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

struct ImDrawData;
namespace Server {
struct GameSnapshot;
}

namespace Client {

class Client;
class DebugOverlay;

class Ui {
public:
    /** Creates the ImGui context and SDL/Vulkan backends for the supplied window and swapchain resources. */
    Ui(SDL_Window *window,
       VkInstance instance,
       VkPhysicalDevice physicalDevice,
       VkDevice device,
       std::uint32_t queueFamily,
       VkQueue queue,
       VkRenderPass renderPass,
       VkExtent2D viewport,
       std::uint32_t imageCount);

    /** Releases ImGui backend resources and destroys the ImGui context. */
    ~Ui();

    Ui(Ui const &) = delete;
    Ui &operator=(Ui const &) = delete;

    /** Recreates only the Vulkan backend against the new swapchain render pass and extent. */
    void reinitializeVulkan(VkRenderPass renderPass, VkExtent2D viewport, std::uint32_t imageCount);

    /** Releases Vulkan backend resources before the renderer destroys its swapchain render pass. */
    void shutdownVulkan();

    /** Starts a new ImGui frame after polling the SDL and Vulkan backends. */
    void beginFrame();

    /** Finalizes the ImGui frame and returns its draw data for Vulkan recording. */
    [[nodiscard]] ImDrawData *endFrame();

    /** Draws a white square selection outline using eight inward-facing corner marks. */
    void drawSelectionBox(Rectangle const &rectangle);

    /** Draws a standard ImGui window with the supplied object name as its title. */
    void drawInfoBox(std::string_view name);

    /** Draws the fixed top-right game date-time and speed display. */
    void drawGameTimeBox(Simulation::DateTime const &dateTime, Simulation::GameSpeed speed);

    /** Mounts the economy debug overlay into the active ImGui frame. */
    void drawDebugOverlay(DebugOverlay &overlay,
                          Client const &client,
                          Server::GameSnapshot const &snapshot,
                          std::optional<std::size_t> selectedBody);

    /** Returns whether the previous ImGui frame requested mouse input capture. */
    [[nodiscard]] bool wantsMouseCapture() const;

private:
    void initializeVulkan(VkRenderPass renderPass, std::uint32_t imageCount);
    void createDescriptorPool(std::uint32_t imageCount);

    VkInstance mInstance{};
    VkPhysicalDevice mPhysicalDevice{};
    VkDevice mDevice{};
    std::uint32_t mQueueFamily{};
    VkQueue mQueue{};
    VkDescriptorPool mDescriptorPool{};
    VkExtent2D mViewport{};
    bool mSdlBackendInitialized{};
    bool mVulkanBackendInitialized{};
};

} // namespace Client

#endif // GALAXIES_CLIENT_UI_H
