// ────────────────────────────────────────────
//  File: triangle.cpp · Created by Yash Patel · 9-21-2025
// ────────────────────────────────────────────

#include "triangle.hpp"
#include "utils/logger.hpp"
#include "vulkan/vulkan_utils.hpp"

namespace keplar
{
    Triangle::Triangle() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_presentQueue(VK_NULL_HANDLE)
        , m_graphicsQueue(VK_NULL_HANDLE)
        , m_vkSwapchainKHR(VK_NULL_HANDLE)
        , m_windowWidth(0)
        , m_windowHeight(0)
        , m_sampleCount(VK_SAMPLE_COUNT_1_BIT)
        , m_swapchainImageCount(0)
        , m_maxFramesInFlight(0)
        , m_currentImageIndex(0)
        , m_currentFrameIndex(0)
        , m_readyToRender(false)
    {
    }

    Triangle::~Triangle()
    {
        // stop any new rendering tasks from being submitted
        m_readyToRender.store(false); 

        // ensure GPU has completed all work submitted to the device
        vkDeviceWaitIdle(m_vkDevice);

        // destroy vulkan resources 
        m_commandPool.deallocate(m_primaryCommandBuffers);
        m_commandPool.deallocate(m_secondaryCommandBuffer);
    }

    bool Triangle::initialize(std::weak_ptr<Platform> platform, std::weak_ptr<VulkanContext> context) noexcept
    {
        // store non-owning references
        m_platform = platform;
        m_context = context;

        // acquire shared access to context and platform
        auto contextLocked = m_context.lock(); 
        auto platformLocked = m_platform.lock();
        if (!contextLocked || !platformLocked)
        {
            return false;
        }

        // store non-owning device reference and acquire shared access
        m_device = contextLocked->getDevice();
        auto device = m_device.lock();
        if (!device)
        {
            return false;
        }

        // setup resources and cache vulkan handles
        m_swapchain       = std::make_unique<VulkanSwapchain>(contextLocked->getSurface(), m_device);
        m_vkDevice        = device->getDevice();
        m_presentQueue    = device->getPresentQueue();
        m_graphicsQueue   = device->getGraphicsQueue();
        m_vkSwapchainKHR  = m_swapchain->get();
        m_windowWidth     = platformLocked->getWindowWidth();
        m_windowHeight    = platformLocked->getWindowHeight();

        // initialize vulkan resources
        if (!createSwapchain())             { return false; }
        if (!createMsaaTarget(*device))     { return false; }
        if (!createCommandPool(*device))    { return false; }
        if (!createCommandBuffers())        { return false; }
        if (!createVertexBuffers(*device))  { return false; }
        if (!createUniformBuffers(*device)) { return false; }
        if (!createShaderModules())         { return false; }
        if (!createDescriptorSetLayouts())  { return false; }
        if (!createDescriptorPool())        { return false; }
        if (!createDescriptorSets())        { return false; }
        if (!createRenderPasses())          { return false; }
        if (!createGraphicsPipeline())      { return false; }
        if (!createFramebuffers())          { return false; }
        if (!createSyncPrimitives())        { return false; }
        if (!recordSceneCommandBuffers())   { return false; }

        // initialization complete
        m_readyToRender.store(true);

        VK_LOG_INFO("Triangle::initialize successful");
        return true;
    }

    void Triangle::update(float /* dt */) noexcept
    {
        // update scene state
    }

    bool Triangle::render() noexcept
    {
        // skip frame if renderer is not ready
        if (!m_readyToRender.load())
        {
            VK_LOG_DEBUG("Triangle::render skipped: renderer not ready");
            return true;
        }

        // wait on the fence for this frame to ensure gpu finished work from last time
        // this prevents cpu from submitting commands for the same frame while gpu is still using it
        auto& frameSync = m_frameSyncPrimitives[m_currentFrameIndex];
        if (!frameSync.mInFlightFence.wait())
        {
            return false;
        }

        // frame-specific semaphores and fence
        const auto imageAcquireSemaphore    = frameSync.mImageAvailableSemaphore.get();
        const auto renderCompleteSemaphore  = frameSync.mRenderCompleteSemaphore.get();
        const auto inFlightFence            = frameSync.mInFlightFence.get();

        // acquire next image from swapchain, signaling the image available semaphore 
        VkResult vkResult = vkAcquireNextImageKHR(m_vkDevice, m_vkSwapchainKHR, UINT64_MAX, imageAcquireSemaphore, VK_NULL_HANDLE, &m_currentImageIndex);
        if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
        {
            VK_LOG_DEBUG("vkAcquireNextImageKHR failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            onWindowResize(m_windowWidth, m_windowHeight);
            return true;
        }
        else if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAcquireNextImageKHR failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // wait if this swapchain image is still in flight
        VkFence& imageFence = m_imagesInFlightFences[m_currentImageIndex];
        if (imageFence != VK_NULL_HANDLE)
        {
            vkResult = vkWaitForFences(m_vkDevice, 1, &imageFence, VK_TRUE, UINT64_MAX);
            if (vkResult != VK_SUCCESS)
            {
                VK_LOG_ERROR("vkWaitForFences failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
                return false;
            }
        }

        // mark this image as now owned by current frame fence
        imageFence = inFlightFence;

        // reset current frame fence before submitting new gpu work
        if (!frameSync.mInFlightFence.reset())
        {
            return false;
        }

        // update per-frame resources
        if (!updatePerFrame(m_currentFrameIndex))
        {
            return false;
        }

        // record primary buffer for this frame targeting the acquired swapchain framebuffer
        if (!recordFrameCommandBuffer(m_currentFrameIndex, m_currentImageIndex))
        {    
            return false;
        }

        // prepare command buffers to submit 
        VkCommandBuffer commandBuffer = m_primaryCommandBuffers[m_currentFrameIndex].get();
        const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        // setup queue submit info
        VkSubmitInfo submitInfo{};
        submitInfo.sType                 = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext                 = nullptr;
        submitInfo.pWaitDstStageMask     = &waitDstStageMask;
        submitInfo.waitSemaphoreCount    = 1;
        submitInfo.pWaitSemaphores       = &imageAcquireSemaphore;
        submitInfo.commandBufferCount    = 1;
        submitInfo.pCommandBuffers       = &commandBuffer;
        submitInfo.signalSemaphoreCount  = 1;
        submitInfo.pSignalSemaphores     = &renderCompleteSemaphore;

        // submit command buffer to graphics queue with fence to track GPU work
        if (!VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, inFlightFence)))
        {
            return false;
        }

        // prepare present info to present the rendered image
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext               = nullptr;
        presentInfo.waitSemaphoreCount  = 1;
        presentInfo.pWaitSemaphores     = &renderCompleteSemaphore;
        presentInfo.swapchainCount      = 1;
        presentInfo.pSwapchains         = &m_vkSwapchainKHR;
        presentInfo.pImageIndices       = &m_currentImageIndex;

        // queue the present operation
        vkResult = vkQueuePresentKHR(m_presentQueue, &presentInfo);
        if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
        {
            VK_LOG_DEBUG("vkQueuePresentKHR failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            onWindowResize(m_windowWidth, m_windowHeight);
            return true;
        }
        else if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkQueuePresentKHR failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // advance to the next frame sync object (cycling through available frames in flight)
        m_currentFrameIndex = (m_currentFrameIndex + 1) % m_maxFramesInFlight;
        return true;
    }

    void Triangle::configureVulkan(VulkanContextConfig& /* config */) noexcept
    {
    }

    void Triangle::onWindowResize(uint32_t width, uint32_t height)
    {    
        // window minimized or device not available: stop rendering
        auto device = m_device.lock();
        if (width == 0 || height == 0 || !device)
        {
            m_readyToRender.store(false);
            return;
        }

        // stop rendering and wait for device to finish all operations
        m_readyToRender.store(false);
        vkDeviceWaitIdle(m_vkDevice);

        // recreate swapchain with new dimensions
        if (!m_swapchain->recreate(width, height))
        {
            return;
        }

        // store old max frames-in-flight for sync check
        const uint32_t previousMaxFramesInFlight = m_maxFramesInFlight;

        // update swapchain handle and image count
        m_vkSwapchainKHR      = m_swapchain->get();
        m_swapchainImageCount = m_swapchain->getImageCount();
        m_maxFramesInFlight   = glm::min(3u, m_swapchainImageCount-1);
        m_maxFramesInFlight   = glm::max(1u, m_maxFramesInFlight);

        // reset per-image fence ownership for the new swapchain images
        m_imagesInFlightFences.assign(m_swapchainImageCount, VK_NULL_HANDLE);

        // teardown all dependent resources (framebuffers, pipeline, renderpass, command buffers)
        for (auto& framebuffer : m_framebuffers)
        {
            framebuffer.destroy();
        }

        m_graphicsPipeline.destroy();
        m_renderPass.destroy();
        m_msaaTarget.destroy();
        m_commandPool.deallocate(m_primaryCommandBuffers);

        // recreate all dependent resources
        if (!createMsaaTarget(*device))   { return; }
        if (!createCommandBuffers())      { return; }
        if (!createRenderPasses())        { return; }
        if (!createGraphicsPipeline())    { return; }
        if (!createFramebuffers())        { return; }
        if (!recordSceneCommandBuffers()) { return; }

        // recreate per-frame sync primitives if max frames-in-flight changed
        if (m_maxFramesInFlight != previousMaxFramesInFlight)
        {
            for (uint32_t i = 0; i < m_maxFramesInFlight; ++i)
            {
                auto& frameSync = m_frameSyncPrimitives[i];
                frameSync.mInFlightFence.destroy();
                frameSync.mRenderCompleteSemaphore.destroy();
                frameSync.mImageAvailableSemaphore.destroy();
            }

            if (!createSyncPrimitives()) 
            { 
                return; 
            }
        }

        // update window dimensions 
        m_windowWidth  = width;
        m_windowHeight = height;

        // resume rendering
        m_readyToRender.store(true);
    }

    bool Triangle::createSwapchain() noexcept
    {
        // setup swapchain
        if (!m_swapchain->initialize(m_windowWidth, m_windowHeight))
        {
            VK_LOG_ERROR("Triangle::createSwapchain : failed to create swapchain fpr presentation.");
            return false;
        }

        // retrieve swapchain handle and image count
        m_vkSwapchainKHR = m_swapchain->get();
        m_swapchainImageCount = m_swapchain->getImageCount();
        
        // vulkan spec suggests max frames in flight should be less than swapchain image count
        // to prevent CPU stalling while waiting for an image to become available for rendering.
        // setting an upper limit of 3 balances throughput and avoids stalls.
        m_maxFramesInFlight = glm::min(3u, m_swapchainImageCount-1);
        m_maxFramesInFlight = glm::max(1u, m_maxFramesInFlight);

        VK_LOG_INFO("Triangle::createSwapchain : swapchain created successfully (max frames in flight: %d)", m_maxFramesInFlight);
        return true;
    }

    bool Triangle::createMsaaTarget(const VulkanDevice& device) noexcept
    {
        // initialize msaa color and depth targets for the current swapchain
        if (m_msaaTarget.initialize(device, *m_swapchain, VK_SAMPLE_COUNT_8_BIT))
        {
            // success
            m_sampleCount = m_msaaTarget.getSampleCount();
            VK_LOG_DEBUG("Triangle::createMsaaTarget : msaa targets created successfully.");
        }
        else 
        {
            // fall back to no msaa
            VK_LOG_WARN("Triangle::createMsaaTarget : failed to create msaa color and depth targets.");
        }

        return true;
    }

    bool Triangle::createCommandPool(const VulkanDevice& device) noexcept
    {
        // setup command pool for graphics queue
        const auto graphicsFamilyIndex = device.getQueueFamilyIndices().mGraphicsFamily;

        // command pool creation info
        VkCommandPoolCreateInfo vkCommandPoolCreateInfo{};
        vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        vkCommandPoolCreateInfo.pNext = nullptr;
        vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCommandPoolCreateInfo.queueFamilyIndex = graphicsFamilyIndex .value();

        // setup command pool
        if (!m_commandPool.initialize(m_vkDevice, vkCommandPoolCreateInfo))
        {
            VK_LOG_ERROR("Triangle::createCommandPool : failed to initialize command pool.");
            return false;
        }

        VK_LOG_DEBUG("Triangle::createCommandPool successful");
        return true;
    }

    bool Triangle::createCommandBuffers() noexcept
    {
        // allocate primary command buffers for each frame in flight
        m_primaryCommandBuffers = m_commandPool.allocatePrimaries(m_maxFramesInFlight);
        if (m_primaryCommandBuffers.empty())
        {
            VK_LOG_ERROR("Triangle::createCommandBuffers failed to allocate primary command buffers.");
            return false;
        }

        // allocate secondary command buffers to cache stable draw workloads
        m_secondaryCommandBuffer = m_commandPool.allocateSecondaries(m_maxFramesInFlight);
        if (m_secondaryCommandBuffer.empty())
        {
            VK_LOG_ERROR("Triangle::createCommandBuffers failed to allocate secondary command buffers.");
            return false;
        }

        VK_LOG_DEBUG("Triangle::createCommandBuffers successful");
        return true;
    }

    bool Triangle::createVertexBuffers(const VulkanDevice& device) noexcept
    {
        // define triangle vertex positions
        float triangle_position[] = 
        {
            // (x, y, z)
            0.0f,  1.0f,  0.0f,
           -1.0f, -1.0f,  0.0f,
            1.0f, -1.0f,  0.0f
        };

        // define triangle vertex colors
        float triangle_colors[] = 
        {
            // (r, g, b)
            1.0f, 0.0f, 0.0f,  
            0.0f, 1.0f, 0.0f,  
            0.0f, 0.0f, 1.0f   
        };

        // create device local buffer via staging buffer for upload
        // vulkan buffer creation info
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;                                    
        bufferCreateInfo.pNext = nullptr;                                                                    
        bufferCreateInfo.flags = 0;                                                                                                                      
        bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;  
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;                                        

        // create triangle position buffer
        bufferCreateInfo.size = sizeof(triangle_position); 
        if (!m_positionBuffer.createDeviceLocal(device, m_commandPool, bufferCreateInfo, triangle_position, sizeof(triangle_position)))
        {
            VK_LOG_ERROR("Triangle::createVertexBuffers failed to create device-local buffer for vertex positions");
            return false;
        }

        // create triangle color buffer
        bufferCreateInfo.size = sizeof(triangle_colors); 
        if (!m_colorBuffer.createDeviceLocal(device, m_commandPool, bufferCreateInfo, triangle_colors, sizeof(triangle_colors)))
        {
            VK_LOG_ERROR("Triangle::createVertexBuffers failed to create device-local buffer for vertex colors");
            return false;
        }
        
        VK_LOG_DEBUG("Triangle::createVertexBuffers successful");
        return true;
    }

    bool Triangle::createUniformBuffers(const VulkanDevice& device) noexcept
    {
        // allocate space for vectors per frame 
        m_uniformBuffers.resize(m_maxFramesInFlight);
        m_uboFrameData.resize(m_maxFramesInFlight);

        // calculate required buffer size for uniform data
        VkDeviceSize bufferSize = sizeof(ubo::FrameData);

        // create a host-visible uniform buffer for each frame
        for (size_t i = 0; i < m_maxFramesInFlight; i++)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.pNext = nullptr;
            bufferCreateInfo.flags = 0;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferCreateInfo.size = bufferSize;

            // create host-visible buffer for uniform data
            if (!m_uniformBuffers[i].createHostVisible(device, bufferCreateInfo, nullptr, 0, true))
            {
                VK_LOG_ERROR("Triangle::createUniformBuffers failed to create host visible buffer for uniform data");
                return false;
            }
        }

        VK_LOG_DEBUG("Triangle::createUniformBuffers successful");
        return true;
    }

    bool Triangle::createShaderModules() noexcept
    {
        // create vertex shader module from SPIR-V
        if (!m_vertexShader.initialize(m_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, "triangle/triangle.vert.spv"))
        {
            VK_LOG_ERROR("Triangle::createShaderModules failed for vertex shader");
            return false;
        }

        // create fragment shader module from SPIR-V
        if (!m_fragmentShader.initialize(m_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, "triangle/triangle.frag.spv"))
        {
            VK_LOG_ERROR("Triangle::createShaderModules failed for fragment shader");
            return false;
        }

        VK_LOG_DEBUG("Triangle::createShaderModules successful");
        return true;
    }

    bool Triangle::createDescriptorSetLayouts() noexcept
    {
        // binding: 0, type: uniform buffer
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        binding.pImmutableSamplers = nullptr;

        // descriptor set layout creation info
        VkDescriptorSetLayoutCreateInfo descriptorSetlayoutInfo{};
        descriptorSetlayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetlayoutInfo.pNext = nullptr;
        descriptorSetlayoutInfo.flags = 0;
        descriptorSetlayoutInfo.bindingCount = 1;
        descriptorSetlayoutInfo.pBindings = &binding;

        // create descriptor set layout
        if (!m_descriptorSetLayout.initialize(m_vkDevice, descriptorSetlayoutInfo))
        {
            VK_LOG_ERROR("Triangle::createDescriptorSetLayouts failed to create descriptor set layout");
            return false;
        }

        VK_LOG_DEBUG("Triangle::createDescriptorSetLayouts successful");
        return true;
    }

    bool Triangle::createDescriptorPool() noexcept
    {
        // descriptor set requirements
        DescriptorRequirements requirements{};
        requirements.mMaxSets = m_maxFramesInFlight;
        requirements.mUniformCount = 1;
        requirements.mSamplerCount = 0;

        // add requirements
        m_descriptorPool.addRequirements(requirements);

        // create vulkan descriptor pool
        if (!m_descriptorPool.initialize(m_vkDevice))
        {
            VK_LOG_ERROR("Triangle::createDescriptorPool failed");
            return false;
        }

        VK_LOG_DEBUG("Triangle::createDescriptorPool successful");
        return true;
    }

    bool Triangle::createDescriptorSets() noexcept
    {
        // create identical descriptor set layouts for each frame
        std::vector<VkDescriptorSetLayout> layouts(m_maxFramesInFlight, m_descriptorSetLayout.get());

        // descriptor set allocation info
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pNext = nullptr;
        descriptorSetAllocateInfo.descriptorPool = m_descriptorPool.get();
        descriptorSetAllocateInfo.descriptorSetCount = m_maxFramesInFlight;
        descriptorSetAllocateInfo.pSetLayouts = layouts.data();

        // allocate descriptor sets
        m_descriptorSets.resize(m_maxFramesInFlight);
        VkResult vkResult = vkAllocateDescriptorSets(m_vkDevice, &descriptorSetAllocateInfo, m_descriptorSets.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateDescriptorSets failed to allocate descriptor set : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // for each frame, bind its corresponding uniform buffer to the descriptor set
        for (uint32_t i = 0; i < m_maxFramesInFlight; i++)
        {
            // binding buffer info
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffers[i].get();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(ubo::FrameData);

            // specify how the buffer should be bound to the descriptor set
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.pNext = nullptr;
            descriptorWrite.dstSet = m_descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pTexelBufferView = nullptr;

            // commit the binding to the descriptor set
            vkUpdateDescriptorSets(m_vkDevice, 1, &descriptorWrite, 0, nullptr);
        }

        VK_LOG_DEBUG("Triangle::createDescriptorSets successful");
        return true;
    }

    bool Triangle::createRenderPasses() noexcept
    {
        // check if msaa is enabled
        const bool msaaEnabled = m_sampleCount > VK_SAMPLE_COUNT_1_BIT;

        // color attachment: description, reference, subpass dependency
        VkAttachmentDescription colorAttachment{};
        colorAttachment.flags            = 0;
        colorAttachment.format           = m_swapchain->getColorFormat();
        colorAttachment.samples          = m_sampleCount;
        colorAttachment.loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp          = msaaEnabled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout      = msaaEnabled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment    = 0;                                       
        colorAttachmentRef.layout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;   

        // depth attachment: description, reference, subpass dependency
        VkAttachmentDescription depthAttachment{};
        depthAttachment.flags            = 0;
        depthAttachment.format           = m_swapchain->getDepthFormat();
        depthAttachment.samples          = m_sampleCount;                   
        depthAttachment.loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;            
        depthAttachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;             
        depthAttachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   
        depthAttachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;  
        depthAttachment.initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;         
        depthAttachment.finalLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment    = msaaEnabled ? 2 : 1;
        depthAttachmentRef.layout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // resolve attachment: description, reference (msaa only)
        VkAttachmentDescription resolveAttachment{};
        VkAttachmentReference resolveAttachmentRef{};
        if (msaaEnabled)
        {
            resolveAttachment                = colorAttachment;
            resolveAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
            resolveAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            resolveAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            resolveAttachmentRef.attachment  = 1;                                       
            resolveAttachmentRef.layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 
        }    

        // subpass description 
        VkSubpassDescription subpass{};
        subpass.flags                    = 0;
        subpass.pipelineBindPoint        = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount     = 0;
        subpass.pInputAttachments        = nullptr;
        subpass.colorAttachmentCount     = 1;                                   
        subpass.pColorAttachments        = &colorAttachmentRef;      
        subpass.pResolveAttachments      = msaaEnabled ? &resolveAttachmentRef : nullptr;
        subpass.pDepthStencilAttachment  = &depthAttachmentRef;
        subpass.preserveAttachmentCount  = 0;
        subpass.pPreserveAttachments     = nullptr;

        // setup attachments, subpasses and dependencies
        std::vector<VkAttachmentDescription> attachments { colorAttachment }; 
        if (msaaEnabled) { attachments.emplace_back(resolveAttachment); }  
        attachments.emplace_back(depthAttachment); 

        std::vector<VkSubpassDescription> subpasses { subpass };

        // setup render pass 
        if (!m_renderPass.initialize(m_vkDevice, attachments, subpasses, {}))
        {
            VK_LOG_ERROR("Triangle::createRenderPasses failed to initialize render pass");
            return false;
        }

        VK_LOG_DEBUG("Triangle::createRenderPasses successful");
        return true;
    }

    bool Triangle::createGraphicsPipeline() noexcept
    {
        // vertex input bindings
        VkVertexInputBindingDescription vertexBindings[2]{};

        // binding 0: vertex positions
        vertexBindings[0].binding = 0;
        vertexBindings[0].stride = sizeof(float) * 3;
        vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // binding 1: vertex colors
        vertexBindings[1].binding = 1;
        vertexBindings[1].stride = sizeof(float) * 3;
        vertexBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // vertex input attribute descriptions: maps shader locations to bindings
        VkVertexInputAttributeDescription vertexAttributes[2]{};

        // location 0 -> binding 0: position
        vertexAttributes[0].location = 0;
        vertexAttributes[0].binding = 0;
        vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[0].offset = 0;

        // location 1 -> binding 1: color
        vertexAttributes[1].location = 1;
        vertexAttributes[1].binding = 1;
        vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[1].offset = 0;

        // vertex input state
        VkPipelineVertexInputStateCreateInfo vertexInputState{};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputState.pNext = nullptr;
        vertexInputState.flags = 0;
        vertexInputState.vertexBindingDescriptionCount = 2;
        vertexInputState.pVertexBindingDescriptions = vertexBindings;
        vertexInputState.vertexAttributeDescriptionCount = 2;
        vertexInputState.pVertexAttributeDescriptions = vertexAttributes;

        // input assembly state: triangle list
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.pNext = nullptr;
        inputAssembly.flags = 0;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // viewport and scissor state
        auto swapchainExtent = m_swapchain->getExtent();

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchainExtent.width);
        viewport.height = static_cast<float>(swapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent = swapchainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.pNext = nullptr;
        viewportState.flags = 0;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // rasterization state
        VkPipelineRasterizationStateCreateInfo rasterizationState{};
        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.pNext = nullptr;
        rasterizationState.flags = 0;
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationState.lineWidth = 1.0f;

        // multisample state
        VkPipelineMultisampleStateCreateInfo multisampleState{};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.pNext = nullptr;
        multisampleState.flags = 0;
        multisampleState.rasterizationSamples = m_sampleCount;

        // depth stencil state
        VkPipelineDepthStencilStateCreateInfo depthStencilState{};
        depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;                       
        depthStencilState.pNext = NULL;                                                                            
        depthStencilState.flags = 0;                                                                                
        depthStencilState.depthTestEnable = VK_TRUE;                                                                
        depthStencilState.depthWriteEnable = VK_TRUE;                                                               
        depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;                                          
        depthStencilState.depthBoundsTestEnable = VK_FALSE;
        depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
        depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
        depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depthStencilState.stencilTestEnable = VK_FALSE;
        depthStencilState.front = depthStencilState.back;

        // color blend state
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlendState{};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.pNext = nullptr;
        colorBlendState.flags = 0;
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments = &colorBlendAttachment;

        // configure the graphics pipeline
        GraphicsPipelineConfig pipelineConfig{};
        pipelineConfig.mFlags = 0;
        pipelineConfig.mShaderStages.emplace_back(m_vertexShader.getShaderStageInfo());
        pipelineConfig.mShaderStages.emplace_back(m_fragmentShader.getShaderStageInfo());
        pipelineConfig.mVertexInputState = vertexInputState;
        pipelineConfig.mInputAssemblyState = inputAssembly;
        pipelineConfig.mViewportState = viewportState;
        pipelineConfig.mRasterizationState = rasterizationState;
        pipelineConfig.mMultisampleState = multisampleState;
        pipelineConfig.mDepthStencilState = depthStencilState;
        pipelineConfig.mColorBlendState = colorBlendState;
        pipelineConfig.mRenderPass = m_renderPass.get();
        pipelineConfig.mSubpassIndex = 0;
        pipelineConfig.mDescriptorSetLayouts.emplace_back(m_descriptorSetLayout.get());

        // create graphics pipeline
        if (!m_graphicsPipeline.initialize(m_vkDevice, pipelineConfig))
        {
            VK_LOG_ERROR("Triangle::createGraphicsPipeline failed");
            return false;
        }

        VK_LOG_DEBUG("Triangle::createGraphicsPipeline successful");
        return true;
    }

    bool Triangle::createFramebuffers() noexcept
    {
        // allocate framebuffer for each swapchain image
        m_framebuffers.resize(m_swapchainImageCount);

        // get swapchain properties
        const auto swapchainImageViews = m_swapchain->getColorImageViews();
        const auto swapchainExtent     = m_swapchain->getExtent();
        const auto msaaEnabled         = m_sampleCount > VK_SAMPLE_COUNT_1_BIT;

        // prefetch attachments
        VkImageView colorImageView   = VK_NULL_HANDLE;
        VkImageView depthImageView  = VK_NULL_HANDLE;
        if (msaaEnabled)
        {
            colorImageView  = m_msaaTarget.getColorImageView();
            depthImageView  = m_msaaTarget.getDepthImageView();
        }
        else 
        {
            depthImageView  = m_swapchain->getDepthImageView();
        }

        // framebuffer attachments: color, resolve, depth
        VkImageView attachments[3]{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
        
        // framebuffer creation info
        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = nullptr;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = m_renderPass.get();
        framebufferCreateInfo.pAttachments = attachments;
        framebufferCreateInfo.width = swapchainExtent.width;
        framebufferCreateInfo.height = swapchainExtent.height;
        framebufferCreateInfo.layers = 1;

        // create framebuffer per swapchain image view
        for (uint32_t i = 0; i < m_swapchainImageCount; ++i)
        {
            if (msaaEnabled)
            {
                // msaa enabled: [0] multisampled color, [1] swapchain resolve, [2] multisampled depth
                framebufferCreateInfo.attachmentCount = 3;
                attachments[0] = colorImageView;
                attachments[1] = swapchainImageViews[i];
                attachments[2] = depthImageView;
            }
            else 
            {
                // msaa disabled: [0] swapchain color, [1] swapchain depth
                framebufferCreateInfo.attachmentCount = 2;
                attachments[0] = swapchainImageViews[i];
                attachments[1] = depthImageView;
            }
            
            // initialize framebuffer 
            if (!m_framebuffers[i].initialize(m_vkDevice, framebufferCreateInfo))
            {
                VK_LOG_ERROR("Triangle::createFramebuffers failed at swapchain image index %u", i);
                return false;
            }
        }

        VK_LOG_DEBUG("Triangle::createFramebuffers successful");
        return true;
    }

    bool Triangle::createSyncPrimitives() noexcept
    {
        // allocate sync primitives for each frame in flight
        m_frameSyncPrimitives.resize(m_maxFramesInFlight);

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
                VK_LOG_ERROR("Triangle::createSyncPrimitives failed to initialize image available semaphore : %u", i);
                return false;
            }

            // setup render complete semaphore
            if (!frameSync.mRenderCompleteSemaphore.initialize(m_vkDevice, semaphoreCreateInfo))
            {
                VK_LOG_ERROR("Triangle::createSyncPrimitives failed to initialize render complete semaphore : %u", i);
                return false;
            }

            // setup in flight fence for command buffer execution
            if (!frameSync.mInFlightFence.initialize(m_vkDevice, fenceCreateInfo))
            {
                VK_LOG_ERROR("Triangle::createSyncPrimitives failed to initialize fence : %u", i);
                return false;
            }
        }

        // image in flight fences to track which fence currently owns swapchain image
        m_imagesInFlightFences.assign(m_swapchainImageCount, VK_NULL_HANDLE);
        VK_LOG_DEBUG("Triangle::createSyncPrimitives successful");
        return true;
    }

    bool Triangle::recordSceneCommandBuffers() noexcept
    {
        // secondary command buffer inherts render pass state from the primary
        VkCommandBufferInheritanceInfo inheritanceInfo{};
        inheritanceInfo.sType                = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritanceInfo.pNext                = nullptr;
        inheritanceInfo.renderPass           = m_renderPass.get();
        inheritanceInfo.subpass              = 0;
        inheritanceInfo.framebuffer          = VK_NULL_HANDLE;
        inheritanceInfo.occlusionQueryEnable = VK_FALSE;
        inheritanceInfo.queryFlags           = 0;
        inheritanceInfo.pipelineStatistics   = 0;

        // secondary command buffer begin info
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        beginInfo.pInheritanceInfo = &inheritanceInfo;

        for (uint32_t i = 0; i < m_maxFramesInFlight; ++i)
        {
            // reset and begin recording into secondary
            if (!m_secondaryCommandBuffer[i].reset() || !m_secondaryCommandBuffer[i].begin(beginInfo))
            {
                return false;
            }

            // vertex buffer bindings: position at binding 0, color at binding 1
            VkBuffer vertexBuffers[] = { m_positionBuffer.get(), m_colorBuffer.get() };
            VkDeviceSize offset[] = { 0, 0 };

            // record graphics pipeline state, resource bindings, and draw commands for this frame
            m_secondaryCommandBuffer[i].bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.get());
            m_secondaryCommandBuffer[i].bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.getLayout(), 0, 1, &m_descriptorSets[i]);
            m_secondaryCommandBuffer[i].bindVertexBuffers(0, 2, vertexBuffers, offset);
            m_secondaryCommandBuffer[i].draw(3, 1, 0, 0); 

            // finalize the command buffer
            if (!m_secondaryCommandBuffer[i].end())
            {
                return false;
            }
        }

        VK_LOG_DEBUG("Triangle::recordSceneCommandBuffers successful");
        return true;
    }

    bool Triangle::recordFrameCommandBuffer(uint32_t frameIndex, uint32_t imageIndex) noexcept
    {
        // begin primary recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = 0;

        // reset and then begin recording into the command buffer
        auto& commandBuffer = m_primaryCommandBuffers[frameIndex];
        if (!commandBuffer.reset() || !commandBuffer.begin(beginInfo))
        {
            return false;
        }

        // check if msaa is enabled
        const bool msaaEnabled = m_sampleCount > VK_SAMPLE_COUNT_1_BIT;

        // clear values for render pass; color: 0, depth-stencil: 1
        VkClearValue clearValues[3]{};
        if (msaaEnabled)
        {
            clearValues[0].color        = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].color        = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[2].depthStencil = { 1.0f, 0 };
        }
        else 
        {
            clearValues[0].color        = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };
        }

        // render pass begin info (framebuffer updated per command buffer)
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext             = nullptr;
        renderPassBeginInfo.framebuffer       = m_framebuffers[imageIndex].get();
        renderPassBeginInfo.renderPass        = m_renderPass.get();
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = m_swapchain->getExtent();
        renderPassBeginInfo.clearValueCount   = msaaEnabled ? 3 : 2;
        renderPassBeginInfo.pClearValues      = clearValues;
        
        // begin render pass for this frame
        commandBuffer.beginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

        // execute the cached secondary command buffer
        commandBuffer.executeCommands(m_secondaryCommandBuffer[frameIndex]);

        // end current render pass
        commandBuffer.endRenderPass();

        // finalize the command buffer
        return commandBuffer.end();
    }
    
    bool Triangle::updatePerFrame(uint32_t frameIndex) noexcept
    {
        // calculate aspect ratio
        const float aspectRatio = static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight);

        // setup uniform data
        ubo::FrameData& frameData = m_uboFrameData[frameIndex];
        frameData.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
        frameData.view = glm::mat4(1.0f);
        frameData.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        frameData.projection[1][1] *= -1.0f;

        // upload to uniform buffer
        if (!m_uniformBuffers[frameIndex].uploadHostVisible(&frameData, sizeof(frameData)))
        {
            VK_LOG_ERROR("Triangle::updatePerFrame failed for frame: %d", frameIndex);
            return false;
        }

        return true;
    }
}   // namespace keplar
