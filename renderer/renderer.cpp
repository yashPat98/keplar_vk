// ────────────────────────────────────────────
//  File: renderer.cpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#include "renderer.hpp"
#include "utils/logger.hpp"
#include "vulkan/vk_utils.hpp"

namespace keplar
{
    Renderer::Renderer(const VulkanContext& context) noexcept
        : m_threadPool(8)
        , m_vulkanDevice(context.getDevice())
        , m_vulkanSwapchain(context.getSwapchain())
        , m_vkDevice(m_vulkanDevice.getDevice())
        , m_vkSwapchainKHR(m_vulkanSwapchain.get())
        , m_presentQueue(m_vulkanDevice.getPresentQueue())
        , m_graphicsQueue(m_vulkanDevice.getGraphicsQueue())
        , m_swapchainImageCount(m_vulkanSwapchain.getImageCount())
        , m_maxFramesInFlight(min(3u, m_swapchainImageCount - 1u))
        , m_currentImageIndex(0)
        , m_currentFrameIndex(0)
        , m_framebuffers(m_swapchainImageCount)
        , m_frameSyncPrimitives(m_maxFramesInFlight)
        , m_readyToRender(false)
        , m_vertexBuffer(m_vulkanDevice)
    {
        // vulkan spec suggests max frames in flight should be less than swapchain image count
        // to prevent CPU stalling while waiting for an image to become available for rendering.
        // setting an upper limit of 3 balances throughput and avoids stalls.
        VK_LOG_INFO("Renderer::Renderer max frames in flight : %d", m_maxFramesInFlight);
    }

    Renderer::~Renderer()
    {
        // stop any new rendering tasks from being submitted
        m_readyToRender.store(false); 

        // wait until all tasks in the thread pool are finished
        m_threadPool.waitIdle();

        // ensure GPU has completed all work submitted to the device
        vkDeviceWaitIdle(m_vkDevice);

        // destroy vulkan resources 
        m_commandPool.freeBuffers(m_commandBuffers);
        m_commandBuffers.clear();
    }

    bool Renderer::initialize() noexcept
    {
        if (!createCommandBuffers()) { return false; }
        if (!createRenderPasses()) { return false; }
        if (!createFramebuffers()) { return false; }
        if (!createSyncPrimitives()) { return false; }
        if (!buildCommandBuffers()) { return false; }
        if (!createVertexBuffer()) { return false; }
        
        m_readyToRender.store(true);
        VK_LOG_DEBUG("Renderer::initialize successful");
        return true;
    }

    bool Renderer::renderFrame() noexcept
    {
        // 1️⃣ ensure initialization is completed before rendering 
        if (!m_readyToRender.load())
        {
            VK_LOG_WARN("Renderer::renderFrame : initialization is not complete");
            return false;
        }

        // 2️⃣ get sync primitives for current frame
        auto& frameSync = m_frameSyncPrimitives[m_currentFrameIndex];

        // 3️⃣ wait on the fence for this frame to ensure GPU finished work from last time
        // This prevents CPU from submitting commands for the same frame while GPU is still using it
        if (!frameSync.mInFlightFence.wait() || !frameSync.mInFlightFence.reset())
        {
            return false;
        }

        // 4️⃣ acquire next image from swapchain, signaling the image available semaphore 
        if (!VK_CHECK(vkAcquireNextImageKHR(m_vkDevice, m_vkSwapchainKHR, UINT64_MAX, frameSync.mImageAvailableSemaphore.get(), VK_NULL_HANDLE, &m_currentImageIndex)))
        {
            // handle swapchain recreation errors
            return false;
        }

        // 5️⃣ prepare submit info to submit command buffer with sync info
        const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSemaphore waitSemaphore = frameSync.mImageAvailableSemaphore.get();
        VkSemaphore signalSemaphore = frameSync.mRenderCompleteSemaphore.get();
        VkCommandBuffer commandBuffer = m_commandBuffers[m_currentImageIndex].get();

        // setup queue submit info
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.pWaitDstStageMask = &waitDstStageMask;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSemaphore;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphore;

        // 6️⃣ submit command buffer to graphics queue with fence to track GPU work
        if (!VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frameSync.mInFlightFence.get())))
        {
            return false;
        }

        // 7️⃣ prepare present info to present the rendered image
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &signalSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_vkSwapchainKHR;
        presentInfo.pImageIndices = &m_currentImageIndex;

        // 8️⃣ queue the present operation
        if (!VK_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo)))
        {
            return false;
        }

        // 9️⃣ advance to the next frame sync object (cycling through available frames in flight)
        m_currentFrameIndex = (m_currentFrameIndex + 1) % m_maxFramesInFlight;
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
        m_commandBuffers = m_commandPool.allocateBuffers(m_swapchainImageCount, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
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

        // create framebuffer per swapchain image view
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

    bool Renderer::createSyncPrimitives()
    {
        // semaphore creation info (binary semaphore)
        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = nullptr;                                    // default semaphore type is binary
        semaphoreCreateInfo.flags = 0;

        // fence creation info (signaled state)
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;           

        // create sync primitives per frame
        for (uint32_t i = 0; i < m_maxFramesInFlight; ++i)
        {
            auto& frameSync = m_frameSyncPrimitives[i];

            // setup image available semaphore
            if (!frameSync.mImageAvailableSemaphore.initialize(m_vkDevice, semaphoreCreateInfo))
            {
                VK_LOG_ERROR("Renderer::createSyncPrimitives failed to initialize image available semaphore : %u", i);
                return false;
            }

            // setup render complete semaphore
            if (!frameSync.mRenderCompleteSemaphore.initialize(m_vkDevice, semaphoreCreateInfo))
            {
                VK_LOG_ERROR("Renderer::createSyncPrimitives failed to initialize render complete semaphore : %u", i);
                return false;
            }

            // setup in flight fence for command buffer execution
            if (!frameSync.mInFlightFence.initialize(m_vkDevice, fenceCreateInfo))
            {
                VK_LOG_ERROR("Renderer::createSyncPrimitives failed to initialize fence : %u", i);
                return false;
            }
        }

        VK_LOG_DEBUG("Renderer::createSyncPrimitives successful");
        return true;
    }

    bool Renderer::buildCommandBuffers()
    {
        // clear color used at the start of render pass
        VkClearValue clearColor{};
        clearColor.color.float32[0] = 0.0f;       // red
        clearColor.color.float32[1] = 0.0f;       // green
        clearColor.color.float32[2] = 1.0f;       // blue
        clearColor.color.float32[3] = 1.0f;       // alpha

        // render pass begin info (framebuffer updated per command buffer)
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = m_renderPass.get();
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = m_vulkanSwapchain.getExtent();
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearColor;

        // record commands per swapchain image
        for (uint32_t i = 0; i < m_swapchainImageCount; ++i)
        {
            // reset and then begin recording into the command buffer
            if (!m_commandBuffers[i].reset() || !m_commandBuffers[i].begin())
            {
                return false;
            }

            // setup render pass recording
            renderPassBeginInfo.framebuffer = m_framebuffers[i].get();
            m_commandBuffers[i].beginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            // drawing commands here ... 
            m_commandBuffers[i].endRenderPass();

            // finalize the command buffer
            if (!m_commandBuffers[i].end())
            {
                return false;
            }
        }

        VK_LOG_DEBUG("Renderer::buildCommandBuffers successful");
        return true;
    }

    bool Renderer::createVertexBuffer()
    {
         // define triangle vertices
        float triangle_position[] = 
        {
            // (x, y, z)
            0.0f,  1.0f,  0.0f,
           -1.0f, -1.0f,  0.0f,
            1.0f, -1.0f,  0.0f
        };

        // vulkan buffer creation info
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;                                    
        bufferCreateInfo.pNext = nullptr;                                                                    
        bufferCreateInfo.flags = 0;                                                                       
        bufferCreateInfo.size = sizeof(triangle_position);                                                
        bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;  
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;                                        

        // create device local buffer via staging buffer for upload
        if (!m_vertexBuffer.createDeviceLocal(m_commandPool, bufferCreateInfo, triangle_position, sizeof(triangle_position), m_threadPool))
        {
            VK_LOG_ERROR("Renderer::createVertexBuffer failed to create device-local vertex buffer");
            return false;
        }
        
        VK_LOG_DEBUG("Renderer::CreateVertexBuffer successful");
        return true;
    }
}   // namespace keplar
