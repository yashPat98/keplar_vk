// ────────────────────────────────────────────
//  File: renderer.cpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#include "renderer.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    Renderer::Renderer(const VulkanContext& context) noexcept
        : m_vulkanDevice(context.getDevice())
        , m_vulkanSwapchain(context.getSwapchain())
        , m_vkDevice(m_vulkanDevice.getDevice())
        , m_swapchainImageCount(m_vulkanSwapchain.getImageCount())
        , m_framebuffers(m_swapchainImageCount)
    {
    }

    Renderer::~Renderer()
    {
    }

    bool Renderer::initialize() noexcept
    {
        if (!createCommandBuffers())
        {
            return false;
        }

        if (!createRenderPasses())
        {
            return false;
        }

        if (!createFramebuffers())
        {
            return false;
        }

        VK_LOG_DEBUG("Renderer::initialize successful");
        return true;
    }

    bool Renderer::createCommandBuffers()
    {
        // setup command pool for graphics queue
        const auto graphicsFamilyIndex = m_vulkanDevice.getQueueFamilyIndices().mGraphicsFamily;

        // command pool creation info
        VkCommandPoolCreateInfo vkCommandPoolCreateInfo{};
        vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        vkCommandPoolCreateInfo.pNext = nullptr;
        vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCommandPoolCreateInfo.queueFamilyIndex = graphicsFamilyIndex .value();

        // setup command pool
        if (!m_commandPool.initialize(m_vkDevice, vkCommandPoolCreateInfo))
        {
            VK_LOG_ERROR("Renderer::createCommandBuffers : failed to initialize command pool.");
            return false;
        }

        // allocate primary command buffers per swapchain
        m_commandBuffers = m_commandPool.allocate(m_swapchainImageCount, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        if (m_commandBuffers.empty())
        {
            VK_LOG_ERROR("Renderer::createCommandBuffers failed to allocate command buffers.");
            return false;
        }

        VK_LOG_DEBUG("Renderer::createCommandBuffers successful");
        return true;
    }

    bool Renderer::createRenderPasses()
    {
        // color attachment description
        VkAttachmentDescription colorAttachment{};
        colorAttachment.flags = 0;
        colorAttachment.format = m_vulkanSwapchain.getFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                    // no multisampling
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               // clear before rendering
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             // store after rendering
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    // depth and stencil load operation
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // depth and stencil store operation
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;          // initial layout undefined
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // presentable image

        // color attachment references
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;                                       
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;    

        // depth attachment
        VkAttachmentDescription depthAttachment{};
        // ...

        VkAttachmentReference depthAttachmentRef{};
        // ...

        // subpass description
        VkSubpassDescription subpass{};
        subpass.flags = 0;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = 1;                                   
        subpass.pColorAttachments = &colorAttachmentRef;                    
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;

        std::vector<VkAttachmentDescription> attachments { colorAttachment };
        std::vector<VkSubpassDescription> subpasses { subpass };

        // setup render pass with specified attachments, subpasses and dependencies
        if (!m_renderPass.initialize(m_vkDevice, attachments, subpasses))
        {
            VK_LOG_ERROR("Renderer::createRenderPasses failed to initialize render pass");
            return false;
        }

        VK_LOG_DEBUG("Renderer::createRenderPasses successful");
        return true;
    }

    bool Renderer::createFramebuffers()
    {
        // get swapchain properties
        const auto swapchainImageViews = m_vulkanSwapchain.getImageViews();
        const auto swapchainExtent = m_vulkanSwapchain.getExtent();

        // common framebuffer creation info
        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = nullptr;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = m_renderPass.get();
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.width = swapchainExtent.width;
        framebufferCreateInfo.height = swapchainExtent.height;
        framebufferCreateInfo.layers = 1;

        // create one framebuffer per swapchain image view
        for (uint32_t i = 0; i < m_swapchainImageCount; ++i)
        {
            framebufferCreateInfo.pAttachments = &swapchainImageViews[i];
            if (!m_framebuffers[i].initialize(m_vkDevice, framebufferCreateInfo))
            {
                VK_LOG_ERROR("Renderer::createFramebuffers failed to initialize framebuffer : %u", i);
                return false;
            }
        }

        VK_LOG_DEBUG("Renderer::createFramebuffers successful");
        return true;
    }
}   // namespace keplar
