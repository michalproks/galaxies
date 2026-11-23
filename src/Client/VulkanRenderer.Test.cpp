#include "VulkanRenderer.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("Swapchain extent uses the surface's fixed extent") {
    VkSurfaceCapabilitiesKHR capabilities{}; // NOLINT(bugprone-invalid-enum-default-initialization): set before use.
    capabilities.currentExtent = {1280, 800};
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    VkExtent2D const extent = Client::VulkanRenderer::chooseSwapchainExtent({capabilities, {640, 480}});

    CHECK(extent.width == 1280);
    CHECK(extent.height == 800);
}

TEST_CASE("Swapchain extent clamps the requested pixel size to surface limits") {
    VkSurfaceCapabilitiesKHR capabilities{}; // NOLINT(bugprone-invalid-enum-default-initialization): set before use.
    capabilities.currentExtent = {UINT32_MAX, UINT32_MAX};
    capabilities.minImageExtent = {640, 480};
    capabilities.maxImageExtent = {1920, 1080};
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    VkExtent2D const extent = Client::VulkanRenderer::chooseSwapchainExtent({capabilities, {2560, 400}});

    CHECK(extent.width == 1920);
    CHECK(extent.height == 480);
}
