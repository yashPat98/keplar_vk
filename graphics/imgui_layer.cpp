// ────────────────────────────────────────────
//  File: imgui_layer.cpp · Created by Yash Patel · 10-17-2025
// ────────────────────────────────────────────

#include "imgui_layer.hpp"
#include "utils/logger.hpp"
#include "vulkan/vulkan_device.hpp"
#include "vulkan/vulkan_utils.hpp"

namespace 
{
    static int ImGui_ImplWin32_CreateVkSurface(ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface)
    {
        VkWin32SurfaceCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = (HWND)viewport->PlatformHandleRaw;
        createInfo.hinstance = ::GetModuleHandle(nullptr);
        return (int)vkCreateWin32SurfaceKHR((VkInstance)vk_instance, &createInfo, (VkAllocationCallbacks*)vk_allocator, (VkSurfaceKHR*)out_vk_surface);
    }
}

namespace keplar
{
    ImGuiLayer::ImGuiLayer(const VulkanSwapchain& swapchain) noexcept
        : m_swapchain(swapchain)
        , m_vkDevice(VK_NULL_HANDLE)
        , m_vkInstance(VK_NULL_HANDLE)
        , m_initInfo{}
    {
    }

    ImGuiLayer::~ImGuiLayer()
    {
        // destroy vulkan resources 
        m_commandPool.releaseBuffers(m_commandBuffers);

        // shutdown imgui backend
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    bool ImGuiLayer::initialize(const Platform& platform, const VulkanContext& context) noexcept
    { 
        // acquire shared access to instance and device  
        const auto instance = context.getInstance().lock();
        const auto device = context.getDevice().lock();
        if (!instance || !device)
        {
            VK_LOG_DEBUG("ImGuiLayer::initialize : failed to acquire vulkan instace and device");
            return false;
        }
 
        // store vulkan handles and swapchain info
        m_vkInstance  = instance->get();
        m_vkDevice    = device->getDevice();
        m_queueFamily = device->getQueueFamilyIndices().mGraphicsFamily.value();
        m_imageCount  = m_swapchain.getImageCount();
        m_imageExtent = m_swapchain.getExtent();

        // initialize vulkan resources for imgui
        if (!createResourcePool())                       { return false; }
        if (!createCommandBuffers())                     { return false; }
        if (!createRenderPass())                         { return false; } 
        if (!createFramebuffers())                       { return false; }
        if (!createImGuiBackend(platform, *device))      { return false; }

        VK_LOG_DEBUG("ImGuiLayer::initialize successful");
        return true;
    }
    
    void ImGuiLayer::recreate() noexcept
    {
        // teardown vulkan resources
        m_framebuffers.clear();
        m_renderPass.destroy();
        m_commandPool.releaseBuffers(m_commandBuffers);
        m_commandBuffers.clear();

        // store vulkan handles and swapchain info
        m_imageCount  = m_swapchain.getImageCount();
        m_imageExtent = m_swapchain.getExtent();

        // recreate vulkan resources
        if (!createCommandBuffers())  { return; }
        if (!createRenderPass())      { return; } 
        if (!createFramebuffers())    { return; }
        
        // reset render pass to imgui
        m_initInfo.PipelineInfoMain.RenderPass = m_renderPass.get();
    }

    void ImGuiLayer::renderControlPanel() noexcept
    {
        // control panel size and position
        ImGuiIO& io = ImGui::GetIO();
        const float sidebar_width = io.DisplaySize.x * 0.17f; 
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sidebar_width, io.DisplaySize.y), ImGuiCond_Always);

        // begin control panel window
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
        ImGui::Begin("Render Control Panel", nullptr, window_flags);

        // invoke registered imgui widgets
        for (auto& widgetCallback : m_widgetCallbacks)
        {
            widgetCallback();
        }

        if (ImGui::TreeNodeEx("About", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            ImGui::TextWrapped("KEPLAR-VK : v1.0");
            ImGui::Separator();
            ImGui::TextWrapped("A modular Vulkan 1.4+ framework in C++17, built for real-time rendering and efficient resource management.");
            ImGui::Separator();
            ImGui::Text("Dependencies:");
            ImGui::BulletText("ImGui - Immediate mode GUI");
            ImGui::BulletText("TinyGLTF - glTF 3D model loading");
            ImGui::BulletText("GLM - Mathematics library");
            ImGui::Separator();
            ImGui::TextWrapped("© 2025 Yash Patel");
            ImGui::TreePop();
        }

        // end control panel window
        ImGui::End();
    }

    VkCommandBuffer ImGuiLayer::recordFrame(uint32_t frameIndex) noexcept
    {
        // validate frame index
        if (frameIndex >= m_commandBuffers.size())
        {
            return VK_NULL_HANDLE;
        }

        // start a new imgui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        renderControlPanel();

        // retrieve imgui draw data
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData == NULL)
        {
            return VK_NULL_HANDLE;
        }

        // begin recording 
        auto& commandBuffer = m_commandBuffers[frameIndex];
        commandBuffer.reset();
        commandBuffer.begin();

        VkClearValue clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass.get();
        renderPassInfo.framebuffer = m_framebuffers[frameIndex].get();
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_imageExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearValue;

        commandBuffer.beginRenderPass(renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // render imgui draw data
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer.get());

        // end recording 
        commandBuffer.endRenderPass();
        commandBuffer.end();
        return commandBuffer.get();
    }

    void ImGuiLayer::registerWidget(std::function<void()> callback)
    {
        m_widgetCallbacks.emplace_back(callback);
    }

    bool ImGuiLayer::createResourcePool() noexcept
    {
        // command pool creation info
        VkCommandPoolCreateInfo vkCommandPoolCreateInfo{};
        vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        vkCommandPoolCreateInfo.pNext = nullptr;
        vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCommandPoolCreateInfo.queueFamilyIndex = m_queueFamily;

        // setup command pool
        if (!m_commandPool.initialize(m_vkDevice, vkCommandPoolCreateInfo))
        {
            VK_LOG_ERROR("ImGuiLayer::createResourcePool : failed to initialize command pool.");
            return false;
        }

        // descriptor set requirements
        DescriptorRequirements requirements{};
        requirements.mMaxSets       = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
        requirements.mUniformCount  = 0;
        requirements.mSamplerCount  = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;

        // add requirements
        m_descriptorPool.addRequirements(requirements);

        // create vulkan descriptor pool
        if (!m_descriptorPool.initialize(m_vkDevice))
        {
            VK_LOG_ERROR("ImGuiLayer::createResourcePool : failed to craete descriptor pool");
            return false;
        }

        return true;
    }

    bool ImGuiLayer::createCommandBuffers() noexcept
    {
        // allocate command buffer per swapchain image
        m_commandBuffers = m_commandPool.allocateBuffers(m_imageCount, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        if (m_commandBuffers.empty())
        {
            VK_LOG_ERROR("ImGuiLayer::createCommandBuffers : failed to allocate command buffers.");
            return false;
        }

        return true;
    }

    bool ImGuiLayer::createRenderPass() noexcept
    {
        // color attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.flags          = 0;
        colorAttachment.format         = m_swapchain.getColorFormat(); 
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;   
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // attachment reference 
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment  = 0;
        colorAttachmentRef.layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // subpass description (single subpass, color-only)
        VkSubpassDescription subpass{};
        subpass.flags                   = 0;
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount    = 0;
        subpass.pInputAttachments       = nullptr;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorAttachmentRef;
        subpass.pResolveAttachments     = nullptr;
        subpass.pDepthStencilAttachment = nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments    = nullptr;

        // assemble attachments and subpasses
        std::vector<VkAttachmentDescription> attachments { colorAttachment };
        std::vector<VkSubpassDescription> subpasses { subpass };

        // initialize render pass 
        if (!m_renderPass.initialize(m_vkDevice, attachments, subpasses, {}))
        {
            VK_LOG_ERROR("ImGuiLayer::createRenderPass failed to initialize render pass");
            return false;
        }

        return true;
    }

    bool ImGuiLayer::createFramebuffers() noexcept
    {
        // get swapchain properties
        const auto& swapchainImageViews = m_swapchain.getColorImageViews();
        const auto& swapchainExtent     = m_swapchain.getExtent();
        
        // resize framebuffers
        m_framebuffers.resize(m_imageCount);

        // framebuffer creation info
        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext           = nullptr;
        framebufferCreateInfo.flags           = 0;
        framebufferCreateInfo.renderPass      = m_renderPass.get();  
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.width           = swapchainExtent.width;
        framebufferCreateInfo.height          = swapchainExtent.height;
        framebufferCreateInfo.layers          = 1;

        for (uint32_t i = 0; i < m_imageCount; ++i)
        {
            // initialize framebuffer
            const VkImageView attachment = swapchainImageViews[i];
            framebufferCreateInfo.pAttachments = &attachment;
            if (!m_framebuffers[i].initialize(m_vkDevice, framebufferCreateInfo))
            {
                VK_LOG_ERROR("ImGuiLayer::createFramebuffers failed at swapchain image index %u", i);
                return false;
            }
        }

        return true;
    }

    bool ImGuiLayer::createImGuiBackend(const Platform& platform, const VulkanDevice& device) noexcept
    {
        // create imgui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // setup style 
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding     = 5.0f;
        style.FrameRounding      = 3.0f;
        style.ChildRounding      = 3.0f;
        style.PopupRounding      = 3.0f;
        style.GrabRounding       = 3.0f;
        style.WindowBorderSize   = 0.0f;
        style.FrameBorderSize    = 0.0f;
        style.TabBorderSize      = 0.0f;
        style.WindowPadding      = ImVec2(0.0f, 0.0f);
        style.FramePadding       = ImVec2(6.0f, 4.0f);

        // setup colors
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg]         = ImVec4(0.12f, 0.12f, 0.12f, 0.65f); 
        colors[ImGuiCol_ChildBg]          = ImVec4(0.15f, 0.15f, 0.15f, 0.9f);
        colors[ImGuiCol_PopupBg]          = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
        colors[ImGuiCol_Header]           = ImVec4(0.75f, 0.2f, 0.2f, 0.7f); 
        colors[ImGuiCol_HeaderHovered]    = ImVec4(0.85f, 0.3f, 0.3f, 0.9f);
        colors[ImGuiCol_HeaderActive]     = ImVec4(0.95f, 0.4f, 0.4f, 1.0f);
        colors[ImGuiCol_Button]           = ImVec4(0.75f, 0.2f, 0.2f, 0.7f);
        colors[ImGuiCol_ButtonHovered]    = ImVec4(0.85f, 0.3f, 0.3f, 0.9f);
        colors[ImGuiCol_ButtonActive]     = ImVec4(0.95f, 0.4f, 0.4f, 1.0f);
        colors[ImGuiCol_FrameBg]          = ImVec4(0.20f, 0.20f, 0.20f, 0.9f); 
        colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
        colors[ImGuiCol_FrameBgActive]    = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
        colors[ImGuiCol_CheckMark]        = ImVec4(0.95f, 0.4f, 0.4f, 1.0f); 
        colors[ImGuiCol_SliderGrab]       = ImVec4(0.95f, 0.4f, 0.4f, 1.0f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
        colors[ImGuiCol_Tab]              = ImVec4(0.2f, 0.1f, 0.1f, 0.8f);
        colors[ImGuiCol_TabHovered]       = ImVec4(0.85f, 0.3f, 0.3f, 0.9f);
        colors[ImGuiCol_TabActive]        = ImVec4(0.95f, 0.4f, 0.4f, 1.0f);
        colors[ImGuiCol_TitleBg]          = ImVec4(0.12f, 0.12f, 0.12f, 0.95f);
        colors[ImGuiCol_TitleBgActive]    = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.12f, 0.12f, 0.95f);

        // initialize platform backend
        if (!ImGui_ImplWin32_Init(platform.getWindowHandle()))
        {
            VK_LOG_DEBUG("ImGuiLayer::initialize : failed to initialize Win32 backend");
            return false;
        }

        // set platform specific surface creation callback
        ImGui::GetPlatformIO().Platform_CreateVkSurface = ImGui_ImplWin32_CreateVkSurface;

        /// setup imgui vulkan init info
        m_initInfo.ApiVersion                    = kDefaultVulkanApiVersion;
        m_initInfo.Instance                      = m_vkInstance;
        m_initInfo.PhysicalDevice                = device.getPhysicalDevice();
        m_initInfo.Device                        = m_vkDevice;
        m_initInfo.QueueFamily                   = m_queueFamily;
        m_initInfo.Queue                         = device.getGraphicsQueue();
        m_initInfo.DescriptorPool                = m_descriptorPool.get();
        m_initInfo.MinImageCount                 = 2;
        m_initInfo.ImageCount                    = m_imageCount;
        m_initInfo.PipelineCache                 = VK_NULL_HANDLE;
        m_initInfo.PipelineInfoMain.RenderPass   = m_renderPass.get();   
        m_initInfo.PipelineInfoMain.Subpass      = 0;                          
        m_initInfo.PipelineInfoMain.MSAASamples  = VK_SAMPLE_COUNT_1_BIT;      
        m_initInfo.CheckVkResultFn               = [](VkResult vkResult) { VK_CHECK(vkResult); };
        
        // initialize vulkan backend
        if (!ImGui_ImplVulkan_Init(&m_initInfo))
        {
            VK_LOG_DEBUG("ImGuiLayer::initialize : failed to initialize vulkan backend");
            return false;
        }

        // set fonts 
        ImFontConfig fontConfig;
        fontConfig.OversampleH = 3;
        fontConfig.OversampleV = 3;
        fontConfig.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("resources/fonts/MonaspaceNeon.otf", 15.0f, &fontConfig);
        return true;
    }
}   // namespace keplar
