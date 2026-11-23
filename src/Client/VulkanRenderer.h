#ifndef GALAXIES_CLIENT_VULKAN_RENDERER_H
#define GALAXIES_CLIENT_VULKAN_RENDERER_H

#include "GameWindow.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <vector>

namespace Client {

struct SwapchainExtentRequest {
    VkSurfaceCapabilitiesKHR const &capabilities;
    VkExtent2D requestedExtent;
};

class VulkanRenderer {
public:
    explicit VulkanRenderer(GameWindow const &window);
    ~VulkanRenderer();

    VulkanRenderer(VulkanRenderer const &) = delete;
    VulkanRenderer &operator=(VulkanRenderer const &) = delete;

    /** Clears and presents one black frame, recreating the swapchain when required. */
    void renderBlackFrame();

    /** Chooses a surface extent from the requested pixel size and surface limits. */
    [[nodiscard]] static VkExtent2D chooseSwapchainExtent(SwapchainExtentRequest const &request);

private:
    void initialize(GameWindow const &window);
    void selectPhysicalDevice();
    void createLogicalDevice();
    void createFrameSynchronization();
    void recreateSwapchain();
    void createSwapchain();
    void createRenderPass();
    void createImageViews();
    void createFramebuffers();
    void createCommandBuffers();
    void recordCommandBuffer(std::size_t imageIndex) const;
    void destroySwapchainResources();
    void destroy();

    VkInstance mInstance{};
    SDL_Window *mWindow{};
    VkSurfaceKHR mSurface{};
    VkPhysicalDevice mPhysicalDevice{};
    std::uint32_t mQueueFamilyIndex{};
    VkDevice mDevice{};
    VkQueue mQueue{};
    VkSwapchainKHR mSwapchain{};
    VkFormat mImageFormat{};
    VkExtent2D mExtent{};
    VkRenderPass mRenderPass{};
    std::vector<VkImage> mImages;
    std::vector<VkImageView> mImageViews;
    std::vector<VkFramebuffer> mFramebuffers;
    VkCommandPool mCommandPool{};
    std::vector<VkCommandBuffer> mCommandBuffers;
    VkSemaphore mImageAvailableSemaphore{};
    VkSemaphore mRenderFinishedSemaphore{};
    VkFence mInFlightFence{};
};

} // namespace Client

#endif // GALAXIES_CLIENT_VULKAN_RENDERER_H
