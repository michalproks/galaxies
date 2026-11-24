#ifndef GALAXIES_CLIENT_UI_H
#define GALAXIES_CLIENT_UI_H

#include "Selection.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string_view>

struct ImDrawData;

namespace Client {

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
