#ifndef GALAXIES_CLIENT_VULKAN_RENDERER_H
#define GALAXIES_CLIENT_VULKAN_RENDERER_H

#include "Camera.h"
#include "GameWindow.h"

#include <Server.h>
#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Client {

/** Requests a swapchain extent constrained by the surface's capabilities. */
struct SwapchainExtentRequest {
    VkSurfaceCapabilitiesKHR const &capabilities;
    VkExtent2D requestedExtent;
};

class VulkanRenderer {
public:
    /** Initializes Vulkan resources for the supplied SDL window. */
    explicit VulkanRenderer(GameWindow const &window);

    /** Waits for queued work and releases Vulkan resources owned by this renderer. */
    ~VulkanRenderer();

    VulkanRenderer(VulkanRenderer const &) = delete;
    VulkanRenderer &operator=(VulkanRenderer const &) = delete;

    /** Projects and draws every snapshot body, then presents the frame; viewport must match the swapchain extent. */
    void renderFrame(Camera const &camera, Server::GameSnapshot const &snapshot, VkExtent2D viewport);

    /** Returns the current drawable swapchain dimensions in pixels. */
    [[nodiscard]] VkExtent2D viewport() const;

    /** Chooses a surface extent from the requested pixel size and surface limits. */
    [[nodiscard]] static VkExtent2D chooseSwapchainExtent(SwapchainExtentRequest const &request);

private:
    struct Vertex {
        float x{};
        float y{};
    };

    struct BodyDraw {
        std::uint32_t firstVertex{};
        std::uint32_t vertexCount{};
    };

    void initialize(GameWindow const &window);
    void selectPhysicalDevice();
    void createLogicalDevice();
    void createVertexBuffer();
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void createFrameSynchronization();
    void recreateSwapchain();
    void createSwapchain();
    void createRenderPass();
    void createGraphicsPipeline();
    void createImageViews();
    void createFramebuffers();
    void createCommandBuffers();
    void recordCommandBuffer(std::size_t imageIndex,
                             Server::GameSnapshot const &snapshot,
                             std::vector<BodyDraw> const &bodyDraws) const;
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
    VkPipelineLayout mPipelineLayout{};
    VkPipeline mGraphicsPipeline{};
    std::vector<VkImage> mImages;
    std::vector<VkImageView> mImageViews;
    std::vector<VkFramebuffer> mFramebuffers;
    VkCommandPool mCommandPool{};
    std::vector<VkCommandBuffer> mCommandBuffers;
    VkBuffer mVertexBuffer{};
    VkDeviceMemory mVertexMemory{};
    VkDeviceSize mVertexBufferSize{};
    VkSemaphore mImageAvailableSemaphore{};
    VkSemaphore mRenderFinishedSemaphore{};
    VkFence mInFlightFence{};
};

} // namespace Client

#endif // GALAXIES_CLIENT_VULKAN_RENDERER_H
