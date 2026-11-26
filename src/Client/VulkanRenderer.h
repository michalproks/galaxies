#ifndef GALAXIES_CLIENT_VULKAN_RENDERER_H
#define GALAXIES_CLIENT_VULKAN_RENDERER_H

#include "Camera.h"
#include "GameWindow.h"

#include <Server.h>
#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct ImDrawData;

namespace Client {

class Ui;
class Client;

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

    /** Projects and draws every body plus optional selection UI, then presents; viewport must match the swapchain. */
    void renderFrame(Camera const &camera,
                     Server::GameSnapshot const &snapshot,
                     VkExtent2D viewport,
                     std::optional<std::size_t> selectedBody,
                     Client const &client);

    /** Returns whether the previous ImGui frame requested mouse input capture. */
    [[nodiscard]] bool wantsMouseCapture() const;

    /** Returns whether the economy modal blocks map camera input. */
    [[nodiscard]] bool blocksMapInput() const;

    /** Returns and clears the colony selected in the empire screen ranking table. */
    [[nodiscard]] std::optional<Simulation::Economy::ColonyId> consumeRequestedEconomyColony() noexcept;

    /** Returns the selected colony request without clearing it. */
    [[nodiscard]] std::optional<Simulation::Economy::ColonyId> requestedEconomyColony() const noexcept;

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
                             std::vector<BodyDraw> const &bodyDraws,
                             ImDrawData *drawData) const;
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
    std::unique_ptr<Ui> mUi;
};

} // namespace Client

#endif // GALAXIES_CLIENT_VULKAN_RENDERER_H
