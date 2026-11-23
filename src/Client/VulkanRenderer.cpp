#include "VulkanRenderer.h"

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Client {
namespace {

    void checkVk(VkResult result, char const *operation) {
        if (result != VK_SUCCESS) {
            throw std::runtime_error(std::string(operation) + " failed with VkResult " + std::to_string(result));
        }
    }

    bool supportsSwapchain(VkPhysicalDevice device) {
        std::uint32_t extensionCount{};
        checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr),
                "vkEnumerateDeviceExtensionProperties");

        std::vector<VkExtensionProperties> extensions(extensionCount);
        checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()),
                "vkEnumerateDeviceExtensionProperties");

        return std::ranges::any_of(extensions, [](VkExtensionProperties const &extension) {
            return std::string_view(extension.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        });
    }

} // namespace

VulkanRenderer::VulkanRenderer(GameWindow const &window) {
    try {
        initialize(window);
    } catch (...) {
        destroy();
        throw;
    }
}

VulkanRenderer::~VulkanRenderer() {
    destroy();
}

VkExtent2D VulkanRenderer::chooseSwapchainExtent(SwapchainExtentRequest const &request) {
    VkSurfaceCapabilitiesKHR const &capabilities = request.capabilities;
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    return {
        std::clamp(request.requestedExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(
            request.requestedExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

void VulkanRenderer::initialize(GameWindow const &window) {
    mWindow = window.nativeWindow();
    std::uint32_t extensionCount{};
    char const *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (extensions == nullptr) {
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Galaxies";
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.pEngineName = "Galaxies";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledExtensionCount = extensionCount;
    instanceCreateInfo.ppEnabledExtensionNames = extensions;
    checkVk(vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance), "vkCreateInstance");

    mSurface = window.createVulkanSurface(mInstance);
    selectPhysicalDevice();
    createLogicalDevice();

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = mQueueFamilyIndex;
    checkVk(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mCommandPool), "vkCreateCommandPool");

    createFrameSynchronization();
    recreateSwapchain();
}

void VulkanRenderer::selectPhysicalDevice() {
    std::uint32_t deviceCount{};
    checkVk(vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices");
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU was found.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    checkVk(vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices");

    for (VkPhysicalDevice device : devices) {
        if (!supportsSwapchain(device)) {
            continue;
        }

        std::uint32_t familyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

        for (std::uint32_t index = 0; index < familyCount; ++index) {
            VkBool32 canPresent{};
            checkVk(vkGetPhysicalDeviceSurfaceSupportKHR(device, index, mSurface, &canPresent),
                    "vkGetPhysicalDeviceSurfaceSupportKHR");
            if ((families[index].queueFlags & static_cast<VkQueueFlags>(VK_QUEUE_GRAPHICS_BIT)) != 0 &&
                canPresent == VK_TRUE) {
                mPhysicalDevice = device;
                mQueueFamilyIndex = index;
                return;
            }
        }
    }

    throw std::runtime_error("No Vulkan GPU supports graphics and presentation to the game window.");
}

void VulkanRenderer::createLogicalDevice() {
    float const queuePriority = 1.0F;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = mQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    std::array<char const *, 1> const deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    checkVk(vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice), "vkCreateDevice");
    vkGetDeviceQueue(mDevice, mQueueFamilyIndex, 0, &mQueue);
}

void VulkanRenderer::createFrameSynchronization() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    checkVk(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphore), "vkCreateSemaphore");
    checkVk(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphore), "vkCreateSemaphore");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    checkVk(vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFence), "vkCreateFence");
}

void VulkanRenderer::recreateSwapchain() {
    checkVk(vkDeviceWaitIdle(mDevice), "vkDeviceWaitIdle");
    destroySwapchainResources();
    createSwapchain();
    createRenderPass();
    createImageViews();
    createFramebuffers();
    createCommandBuffers();
}

void VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities{}; // NOLINT(bugprone-invalid-enum-default-initialization): filled by Vulkan.
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    std::uint32_t formatCount{};
    checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr),
            "vkGetPhysicalDeviceSurfaceFormatsKHR");
    if (formatCount == 0) {
        throw std::runtime_error("The Vulkan surface does not expose any image formats.");
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, formats.data()),
            "vkGetPhysicalDeviceSurfaceFormatsKHR");

    VkSurfaceFormatKHR surfaceFormat = formats.front();
    if (formats.size() == 1 && surfaceFormat.format == VK_FORMAT_UNDEFINED) {
        surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
    }
    for (VkSurfaceFormatKHR const &format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    int pixelWidth{};
    int pixelHeight{};
    if (!SDL_GetWindowSizeInPixels(mWindow, &pixelWidth, &pixelHeight)) {
        throw std::runtime_error(std::string("SDL_GetWindowSizeInPixels failed: ") + SDL_GetError());
    }
    if (pixelWidth <= 0 || pixelHeight <= 0) {
        throw std::runtime_error("The game window has no drawable area.");
    }
    mExtent = chooseSwapchainExtent(
        {capabilities, {static_cast<std::uint32_t>(pixelWidth), static_cast<std::uint32_t>(pixelHeight)}});

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    bool hasCompositeAlpha{};
    constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> COMPOSITE_ALPHA_OPTIONS{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
    for (VkCompositeAlphaFlagBitsKHR option : COMPOSITE_ALPHA_OPTIONS) {
        if ((capabilities.supportedCompositeAlpha & static_cast<VkCompositeAlphaFlagsKHR>(option)) != 0) {
            compositeAlpha = option;
            hasCompositeAlpha = true;
            break;
        }
    }
    if (!hasCompositeAlpha) {
        throw std::runtime_error("The Vulkan surface does not support a composite alpha mode.");
    }
    if ((capabilities.supportedUsageFlags & static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) == 0) {
        throw std::runtime_error("The Vulkan surface does not support color-attachment swapchain images.");
    }

    VkSwapchainCreateInfoKHR
        swapchainInfo{}; // NOLINT(bugprone-invalid-enum-default-initialization): enum fields are set below.
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = mSurface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = mExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = compositeAlpha;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    checkVk(vkCreateSwapchainKHR(mDevice, &swapchainInfo, nullptr, &mSwapchain), "vkCreateSwapchainKHR");
    mImageFormat = surfaceFormat.format;

    std::uint32_t actualImageCount{};
    checkVk(vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actualImageCount, nullptr), "vkGetSwapchainImagesKHR");
    mImages.resize(actualImageCount);
    checkVk(vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actualImageCount, mImages.data()), "vkGetSwapchainImagesKHR");
}

void VulkanRenderer::createRenderPass() {
    VkAttachmentDescription
        colorAttachment{}; // NOLINT(bugprone-invalid-enum-default-initialization): samples is set below.
    colorAttachment.format = mImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    checkVk(vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass), "vkCreateRenderPass");
}

void VulkanRenderer::createImageViews() {
    mImageViews.reserve(mImages.size());
    for (VkImage image : mImages) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = mImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VkImageView imageView{};
        checkVk(vkCreateImageView(mDevice, &viewInfo, nullptr, &imageView), "vkCreateImageView");
        mImageViews.push_back(imageView);
    }
}

void VulkanRenderer::createFramebuffers() {
    mFramebuffers.reserve(mImageViews.size());
    for (VkImageView imageView : mImageViews) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = mRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &imageView;
        framebufferInfo.width = mExtent.width;
        framebufferInfo.height = mExtent.height;
        framebufferInfo.layers = 1;
        VkFramebuffer framebuffer{};
        checkVk(vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
        mFramebuffers.push_back(framebuffer);
    }
}

void VulkanRenderer::createCommandBuffers() {
    mCommandBuffers.resize(mFramebuffers.size());
    VkCommandBufferAllocateInfo allocationInfo{};
    allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocationInfo.commandPool = mCommandPool;
    allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocationInfo.commandBufferCount = static_cast<std::uint32_t>(mCommandBuffers.size());
    checkVk(vkAllocateCommandBuffers(mDevice, &allocationInfo, mCommandBuffers.data()), "vkAllocateCommandBuffers");

    for (std::size_t index = 0; index < mCommandBuffers.size(); ++index) {
        recordCommandBuffer(index);
    }
}

void VulkanRenderer::recordCommandBuffer(std::size_t imageIndex) const {
    VkCommandBuffer const commandBuffer = mCommandBuffers[imageIndex];
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    VkClearValue clearValue{};
    clearValue.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = mRenderPass;
    renderPassInfo.framebuffer = mFramebuffers[imageIndex];
    renderPassInfo.renderArea.extent = mExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(commandBuffer);
    checkVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}

void VulkanRenderer::renderBlackFrame() {
    for (;;) {
        checkVk(vkWaitForFences(mDevice, 1, &mInFlightFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()),
                "vkWaitForFences");

        std::uint32_t imageIndex{};
        VkResult const acquireResult = vkAcquireNextImageKHR(mDevice,
                                                             mSwapchain,
                                                             std::numeric_limits<std::uint64_t>::max(),
                                                             mImageAvailableSemaphore,
                                                             VK_NULL_HANDLE,
                                                             &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            continue;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            checkVk(acquireResult, "vkAcquireNextImageKHR");
        }

        checkVk(vkResetFences(mDevice, 1, &mInFlightFence), "vkResetFences");

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &mImageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &mCommandBuffers[imageIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &mRenderFinishedSemaphore;
        checkVk(vkQueueSubmit(mQueue, 1, &submitInfo, mInFlightFence), "vkQueueSubmit");

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &mRenderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &mSwapchain;
        presentInfo.pImageIndices = &imageIndex;
        VkResult const presentResult = vkQueuePresentKHR(mQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
            acquireResult == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
        } else {
            checkVk(presentResult, "vkQueuePresentKHR");
        }
        return;
    }
}

void VulkanRenderer::destroySwapchainResources() {
    if (mDevice == VK_NULL_HANDLE) {
        return;
    }

    if (!mCommandBuffers.empty() && mCommandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(
            mDevice, mCommandPool, static_cast<std::uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());
        mCommandBuffers.clear();
    }
    for (VkFramebuffer framebuffer : mFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    mFramebuffers.clear();
    if (mRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }
    for (VkImageView imageView : mImageViews) {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }
    mImageViews.clear();
    mImages.clear();
    if (mSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::destroy() {
    if (mDevice != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(mDevice);
        destroySwapchainResources();
        if (mCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        }
        if (mInFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(mDevice, mInFlightFence, nullptr);
        }
        if (mRenderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, mRenderFinishedSemaphore, nullptr);
        }
        if (mImageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, mImageAvailableSemaphore, nullptr);
        }
        vkDestroyDevice(mDevice, nullptr);
    }
    if (mSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    }
    if (mInstance != VK_NULL_HANDLE) {
        vkDestroyInstance(mInstance, nullptr);
    }
}

} // namespace Client
