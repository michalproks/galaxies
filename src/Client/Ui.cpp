#include "Ui.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace Client {
namespace {

    constexpr float INFO_WINDOW_WIDTH = 220.0F;
    constexpr float INFO_WINDOW_HEIGHT = 60.0F;
    constexpr float GAME_TIME_WINDOW_WIDTH = 220.0F;
    constexpr float GAME_TIME_WINDOW_HEIGHT = 48.0F;
    constexpr float WINDOW_PADDING = 8.0F;
    constexpr float SELECTION_LINE_THICKNESS = 2.0F;

    std::string_view speedName(Simulation::GameSpeed speed) {
        switch (speed) {
        case Simulation::GameSpeed::NORMAL:
            return "NORMAL";
        case Simulation::GameSpeed::FAST:
            return "FAST";
        case Simulation::GameSpeed::ULTRA:
            return "ULTRA";
        case Simulation::GameSpeed::PAUSED:
            return "PAUSED";
        }
        return "NORMAL";
    }

} // namespace

Ui::Ui(SDL_Window *window,
       VkInstance instance,
       VkPhysicalDevice physicalDevice,
       VkDevice device,
       std::uint32_t queueFamily,
       VkQueue queue,
       VkRenderPass renderPass,
       VkExtent2D viewport,
       std::uint32_t imageCount)
    : mInstance(instance)
    , mPhysicalDevice(physicalDevice)
    , mDevice(device)
    , mQueueFamily(queueFamily)
    , mQueue(queue)
    , mViewport(viewport) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    try {
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.Fonts->AddFontDefault();
        ImGui::StyleColorsDark();
        if (!ImGui_ImplSDL3_InitForVulkan(window)) {
            throw std::runtime_error("ImGui SDL3 backend initialization failed.");
        }
        mSdlBackendInitialized = true;
        createDescriptorPool(imageCount);
        initializeVulkan(renderPass, imageCount);
    } catch (...) {
        shutdownVulkan();
        if (mSdlBackendInitialized) {
            ImGui_ImplSDL3_Shutdown();
            mSdlBackendInitialized = false;
        }
        ImGui::DestroyContext();
        throw;
    }
}

Ui::~Ui() {
    shutdownVulkan();
    if (mSdlBackendInitialized) {
        ImGui_ImplSDL3_Shutdown();
    }
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
    }
}

void Ui::reinitializeVulkan(VkRenderPass renderPass, VkExtent2D viewport, std::uint32_t imageCount) {
    shutdownVulkan();
    mViewport = viewport;
    createDescriptorPool(imageCount);
    initializeVulkan(renderPass, imageCount);
}

void Ui::initializeVulkan(VkRenderPass renderPass, std::uint32_t imageCount) {
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_0;
    initInfo.Instance = mInstance;
    initInfo.PhysicalDevice = mPhysicalDevice;
    initInfo.Device = mDevice;
    initInfo.QueueFamily = mQueueFamily;
    initInfo.Queue = mQueue;
    initInfo.DescriptorPool = mDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = imageCount;
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        throw std::runtime_error("ImGui Vulkan backend initialization failed.");
    }
    mVulkanBackendInitialized = true;
}

void Ui::shutdownVulkan() {
    if (mVulkanBackendInitialized) {
        ImGui_ImplVulkan_Shutdown();
        mVulkanBackendInitialized = false;
    }
    if (mDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
        mDescriptorPool = VK_NULL_HANDLE;
    }
}

void Ui::createDescriptorPool(std::uint32_t imageCount) {
    if (imageCount == 0) {
        throw std::invalid_argument("The ImGui descriptor pool requires at least one swapchain image.");
    }
    std::uint32_t const extraSets = 4;
    std::uint32_t const descriptorCount = std::max(8U, imageCount + extraSets);
    VkDescriptorPoolSize const poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = imageCount + extraSets;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VkResult const result = vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Creating the ImGui Vulkan descriptor pool failed.");
    }
}

void Ui::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

ImDrawData *Ui::endFrame() {
    ImGui::Render();
    return ImGui::GetDrawData();
}

void Ui::drawSelectionBox(Rectangle const &rectangle) {
    ImGuiIO const &io = ImGui::GetIO();
    float const scaleX = static_cast<float>(io.DisplaySize.x / static_cast<float>(mViewport.width));
    float const scaleY = static_cast<float>(io.DisplaySize.y / static_cast<float>(mViewport.height));
    ImVec2 const min{static_cast<float>(rectangle.min.x) * scaleX, static_cast<float>(rectangle.min.y) * scaleY};
    ImVec2 const max{static_cast<float>(rectangle.max.x) * scaleX, static_cast<float>(rectangle.max.y) * scaleY};
    float const cornerLength = (max.x - min.x) * 0.2F;
    ImDrawList *const drawList = ImGui::GetForegroundDrawList();
    ImU32 const color = 0xFFFFFFFFU;

    drawList->AddLine(min, {min.x + cornerLength, min.y}, color, SELECTION_LINE_THICKNESS);
    drawList->AddLine(min, {min.x, min.y + cornerLength}, color, SELECTION_LINE_THICKNESS);
    drawList->AddLine({max.x, min.y}, {max.x - cornerLength, min.y}, color, SELECTION_LINE_THICKNESS);
    drawList->AddLine({max.x, min.y}, {max.x, min.y + cornerLength}, color, SELECTION_LINE_THICKNESS);
    drawList->AddLine({min.x, max.y}, {min.x + cornerLength, max.y}, color, SELECTION_LINE_THICKNESS);
    drawList->AddLine({min.x, max.y}, {min.x, max.y - cornerLength}, color, SELECTION_LINE_THICKNESS);
    drawList->AddLine(max, {max.x - cornerLength, max.y}, color, SELECTION_LINE_THICKNESS);
    drawList->AddLine(max, {max.x, max.y - cornerLength}, color, SELECTION_LINE_THICKNESS);
}

void Ui::drawInfoBox(std::string_view name) {
    ImGuiIO const &io = ImGui::GetIO();
    float const windowY = std::max(WINDOW_PADDING, io.DisplaySize.y - INFO_WINDOW_HEIGHT - WINDOW_PADDING);
    ImGui::SetNextWindowPos({WINDOW_PADDING, windowY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({INFO_WINDOW_WIDTH, INFO_WINDOW_HEIGHT}, ImGuiCond_Always);
    std::string const title(name);
    ImGui::Begin(title.c_str());
    ImGui::End();
}

void Ui::drawGameTimeBox(Simulation::DateTime const &dateTime, Simulation::GameSpeed speed) {
    ImGuiIO const &io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x - GAME_TIME_WINDOW_WIDTH - WINDOW_PADDING, WINDOW_PADDING},
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize({GAME_TIME_WINDOW_WIDTH, GAME_TIME_WINDOW_HEIGHT}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags FLAGS = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned>(ImGuiWindowFlags_NoTitleBar) | static_cast<unsigned>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned>(ImGuiWindowFlags_NoMove));
    ImGui::Begin("##GameTimeBox", nullptr, FLAGS);
    std::string const dateText = Simulation::format(dateTime);
    ImGui::TextUnformatted(dateText.c_str());
    std::string const speedText = std::string("Speed: ") + std::string(speedName(speed));
    ImGui::TextUnformatted(speedText.c_str());
    ImGui::End();
}

bool Ui::wantsMouseCapture() const {
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

} // namespace Client
