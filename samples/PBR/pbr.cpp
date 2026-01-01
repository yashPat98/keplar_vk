// ────────────────────────────────────────────
//  File: pbr.cpp · Created by Yash Patel · 11-08-2025
// ────────────────────────────────────────────

#include "pbr.hpp"
#include "utils/logger.hpp"
#include "vulkan/vulkan_utils.hpp"

namespace keplar
{
    PBR::PBR() noexcept
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

    PBR::~PBR()
    {
        // stop any new rendering tasks from being submitted
        m_readyToRender.store(false); 

        // unregister listeners from the platform
        if (auto platform = m_platform.lock())
        {
            platform->removeListener(m_camera);
        }

        // ensure gpu has finished executing all submitted commands
        vkDeviceWaitIdle(m_vkDevice);

        // destroy vulkan resources 
        GLTFModel::destroySharedResources(m_vkDevice);
        m_swapchain.reset();
        m_commandPool.releaseBuffers(m_commandBuffers);
    }

    bool PBR::initialize(std::weak_ptr<Platform> platform, std::weak_ptr<VulkanContext> context) noexcept
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
        if (!createTextureSamplers(*device)){ return false; }
        if (!loadAssets(*device))           { return false; }
        if (!createUniformBuffers(*device)) { return false; }
        if (!createShaderModules())         { return false; }
        if (!createDescriptorSetLayouts())  { return false; }
        if (!createDescriptorPool())        { return false; }
        if (!createDescriptorSets())        { return false; }
        if (!createRenderPasses())          { return false; }
        if (!createGraphicsPipeline())      { return false; }
        if (!createFramebuffers())          { return false; }
        if (!createSyncPrimitives())        { return false; }
        if (!buildCommandBuffers())         { return false; }
        if (!prepareScene())                { return false; }

        VK_LOG_INFO("PBR::initialize successful");
        return true;
    }

    bool PBR::renderFrame() noexcept
    {
        // skip frame if renderer is not ready
        if (!m_readyToRender.load())
        {
            VK_LOG_DEBUG("PBR::beginFrame skipped: renderer not ready");
            return true;
        }

        // wait on the fence for this frame to ensure gpu finished work from last time
        // this prevents cpu from submitting commands for the same frame while gpu is still using it
        auto& frameSync = m_frameSyncPrimitives[m_currentFrameIndex];
        if (!frameSync.mInFlightFence.wait() || !frameSync.mInFlightFence.reset())
        {
            return false;
        }

        // wait on cpu to maintain target frame rate
        m_frameLimiter.waitForNextFrame();

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

        // update per-frame data
        if (!updateFrame(m_currentFrameIndex))
        {
            return false;
        }

        // prepare command buffers to submit
        std::vector<VkCommandBuffer> commandBuffers;
        commandBuffers.emplace_back(m_commandBuffers[m_currentImageIndex].get());
        commandBuffers.emplace_back(m_imguiLayer->recordFrame(m_currentFrameIndex));

        // pipeline wait stages
        const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        // setup queue submit info
        VkSubmitInfo submitInfo{};
        submitInfo.sType                 = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext                 = nullptr;
        submitInfo.pWaitDstStageMask     = &waitDstStageMask;
        submitInfo.waitSemaphoreCount    = 1;
        submitInfo.pWaitSemaphores       = &imageAcquireSemaphore;
        submitInfo.commandBufferCount    = static_cast<uint32_t>(commandBuffers.size());
        submitInfo.pCommandBuffers       = commandBuffers.data();
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

    bool PBR::updateFrame(uint32_t frameIndex) noexcept
    {
        // update camera 
        float dt = m_frameLimiter.getDeltaTime();
        m_camera->update(dt);

        // setup camera uniform data
        ubo::Camera& camera = m_camearUniforms[frameIndex];
        camera.model      = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));
        camera.view       = m_camera->getViewMatrix();
        camera.projection = m_camera->getProjectionMatrix();
        camera.position   = glm::vec4(m_camera->getPosition(), 1.0f);

        // upload to camera uniform buffer
        if (!m_cameraUniformBuffers[frameIndex].uploadHostVisible(&camera, sizeof(camera)))
        {
            VK_LOG_ERROR("PBR::updateFrame : uploadHostVisible() failed for camera: %d", frameIndex);
            return false;
        }

        // setup light uniform data
        ubo::Light& light = m_lightUniforms[frameIndex];
        light.position[0] = m_lightPosition;
        light.color[0]    = glm::vec4(m_lightColor, m_lightIntensity);
        light.count       = glm::ivec4(1, 0, 0, 0);

        // upload to light uniform buffer
        if (!m_lightUniformBuffers[frameIndex].uploadHostVisible(&light, sizeof(light)))
        {
            VK_LOG_ERROR("PBR::updateFrame : uploadHostVisible() failed for light: %d", frameIndex);
            return false;
        }

        return true;
    }

    void PBR::configureVulkan(VulkanContextConfig& config) noexcept
    {
        // enable sampler anisotropy for higher quality texture filtering
        config.mRequestedFeatures.samplerAnisotropy = VK_TRUE;
    }

    void PBR::onWindowResize(uint32_t width, uint32_t height)
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
        m_maxFramesInFlight   = glm::min(3u, m_swapchainImageCount);

        // teardown all dependent resources (framebuffers, pipeline, renderpass, command buffers)
        for (auto& framebuffer : m_framebuffers)
        {
            framebuffer.destroy();
        }
        m_graphicsPipeline.destroy();
        m_renderPass.destroy();
        m_msaaTarget.destroy();
        m_commandPool.releaseBuffers(m_commandBuffers);
        m_commandBuffers.clear();

        // recreate all dependent resources
        if (!createMsaaTarget(*device)) { return; }
        if (!createCommandBuffers())    { return; }
        if (!createRenderPasses())      { return; }
        if (!createGraphicsPipeline())  { return; }
        if (!createFramebuffers())      { return; }
        if (!buildCommandBuffers())     { return; }

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
            
            if (!createSyncPrimitives()) { return; }
        }

        // recreate imgui resources
        if (m_imguiLayer)
        {
            m_imguiLayer->recreate();
        }

        // update window dimensions 
        m_windowWidth  = width;
        m_windowHeight = height;

        // resume rendering
        m_currentFrameIndex = 0;
        m_readyToRender.store(true);
    }

    bool PBR::createSwapchain() noexcept
    {
        // setup swapchain
        if (!m_swapchain->initialize(m_windowWidth, m_windowHeight))
        {
            VK_LOG_ERROR("PBR::createSwapchain : failed to create swapchain fpr presentation.");
            return false;
        }

        // retrieve swapchain handle and image count
        m_vkSwapchainKHR = m_swapchain->get();
        m_swapchainImageCount = m_swapchain->getImageCount();
        
        // vulkan spec suggests max frames in flight should be less than swapchain image count
        // to prevent CPU stalling while waiting for an image to become available for rendering.
        // setting an upper limit of 3 balances throughput and avoids stalls.
        m_maxFramesInFlight = glm::min(3u, m_swapchainImageCount);
        VK_LOG_INFO("PBR::createSwapchain : swapchain created successfully (max frames in flight: %d)", m_maxFramesInFlight);
        return true;
    }

    bool PBR::createMsaaTarget(const VulkanDevice& device) noexcept
    {
        // initialize msaa color and depth targets for the current swapchain
        if (m_msaaTarget.initialize(device, *m_swapchain, VK_SAMPLE_COUNT_4_BIT))
        {
            // success
            m_sampleCount = m_msaaTarget.getSampleCount();
            VK_LOG_DEBUG("PBR::createMsaaTarget : msaa targets created successfully.");
        }
        else 
        {
            // fall back to no msaa
            VK_LOG_WARN("PBR::createMsaaTarget : failed to create msaa color and depth targets.");
        }

        return true;
    }

    bool PBR::createCommandPool(const VulkanDevice& device) noexcept
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
            VK_LOG_ERROR("PBR::createCommandPool : failed to initialize command pool.");
            return false;
        }

        VK_LOG_DEBUG("PBR::createCommandPool successful");
        return true;
    }

    bool PBR::createCommandBuffers() noexcept
    {
        // allocate primary command buffers per swapchain
        m_commandBuffers = m_commandPool.allocateBuffers(m_swapchainImageCount, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        if (m_commandBuffers.empty())
        {
            VK_LOG_ERROR("PBR::createCommandBuffers failed to allocate command buffers.");
            return false;
        }

        VK_LOG_DEBUG("PBR::createCommandBuffers successful");
        return true;
    }

    bool PBR::createTextureSamplers(const VulkanDevice& device) noexcept
    {
        // create predefined samplers
        if (!m_samplers.initialize(device))
        {
            VK_LOG_ERROR("PBR::createTextureSamplers failed to create samplers.");
            return false;
        }

        VK_LOG_DEBUG("PBR::createTextureSamplers successful");
        return true;
    }

    bool PBR::loadAssets(const VulkanDevice& device) noexcept
    {
        // initialize shared resources
        GLTFModel::initSharedResources(m_vkDevice);

        // load gltf model
        if (!m_gltfModel.load(device, m_commandPool, "DamagedHelmet.glb"))
        {
            VK_LOG_DEBUG("PBR::loadAssets failed to load gltf model");
            return false;
        }                                     
        
        VK_LOG_DEBUG("PBR::loadAssets successful");
        return true;
    }

    bool PBR::createUniformBuffers(const VulkanDevice& device) noexcept
    {
        // allocate space for vectors per swapchain image
        m_cameraUniformBuffers.resize(m_swapchainImageCount);
        m_lightUniformBuffers.resize(m_swapchainImageCount);
        m_camearUniforms.resize(m_swapchainImageCount);
        m_lightUniforms.resize(m_swapchainImageCount);

        // create a host-visible uniform buffer for each swapchain image
        for (size_t i = 0; i < m_swapchainImageCount; i++)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.pNext = nullptr;
            bufferCreateInfo.flags = 0;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferCreateInfo.size = sizeof(ubo::Camera);

            // create host-visible buffer for camera uniform data
            if (!m_cameraUniformBuffers[i].createHostVisible(device, bufferCreateInfo, nullptr, 0, true))
            {
                VK_LOG_ERROR("PBR::createUniformBuffers failed to create host visible buffer for camera uniform data");
                return false;
            }

            // create host-visible buffer for light uniform data
            bufferCreateInfo.size = sizeof(ubo::Light);
            if (!m_lightUniformBuffers[i].createHostVisible(device, bufferCreateInfo, nullptr, 0, true))
            {
                VK_LOG_ERROR("PBR::createUniformBuffers failed to create host visible buffer for light uniform data");
                return false;
            }
        }

        VK_LOG_DEBUG("PBR::createUniformBuffers successful");
        return true;
    }

    bool PBR::createShaderModules() noexcept
    {
        // create vertex shader module from SPIR-V
        if (!m_vertexShader.initialize(m_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, "pbr/pbr.vert.spv"))
        {
            VK_LOG_ERROR("PBR::createShaderModules failed for vertex shader");
            return false;
        }

        // create fragment shader module from SPIR-V
        if (!m_fragmentShader.initialize(m_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, "pbr/pbr.frag.spv"))
        {
            VK_LOG_ERROR("PBR::createShaderModules failed for fragment shader");
            return false;
        }

        VK_LOG_DEBUG("PBR::createShaderModules successful");
        return true;
    }

    bool PBR::createDescriptorSetLayouts() noexcept
    {
        // set: 0, binding: 0, type: uniform buffer
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding                     = 0;
        uboBinding.descriptorType              = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount             = 1;
        uboBinding.stageFlags                  = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        uboBinding.pImmutableSamplers          = nullptr;

        // descriptor set layout creation info
        VkDescriptorSetLayoutCreateInfo descriptorSetlayoutInfo{};
        descriptorSetlayoutInfo.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetlayoutInfo.pNext          = nullptr;
        descriptorSetlayoutInfo.flags          = 0;
        descriptorSetlayoutInfo.bindingCount   = 1;
        descriptorSetlayoutInfo.pBindings      = &uboBinding;

        // create descriptor set layout for camera
        if (!m_cameraDescriptorSetLayout.initialize(m_vkDevice, descriptorSetlayoutInfo))
        {
            VK_LOG_ERROR("PBR::createDescriptorSetLayouts failed to create descriptor set layout for camera");
            return false;
        }
        
        // set: 2, binding: 0, type: uniform buffer
        uboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // create descriptor set layout for light
        if (!m_lightDescriptorSetLayout.initialize(m_vkDevice, descriptorSetlayoutInfo))
        {
            VK_LOG_ERROR("PBR::createDescriptorSetLayouts failed to create descriptor set layout for light");
            return false;
        }

        VK_LOG_DEBUG("PBR::createDescriptorSetLayouts successful");
        return true;
    }

    bool PBR::createDescriptorPool() noexcept
    {
        // requirements for camera descriptor sets
        DescriptorRequirements cameraRequirements{};
        cameraRequirements.mMaxSets = m_swapchainImageCount;
        cameraRequirements.mUniformCount = 1;

        // requirements for light descriptor sets
        DescriptorRequirements lightRequirements{};
        lightRequirements.mMaxSets = m_swapchainImageCount;
        lightRequirements.mUniformCount = 1;

        // add requirements
        m_descriptorPool.addRequirements(cameraRequirements);
        m_descriptorPool.addRequirements(lightRequirements);
        m_descriptorPool.addRequirements(m_gltfModel.getDescriptorRequirements());

        // create vulkan descriptor pool
        if (!m_descriptorPool.initialize(m_vkDevice))
        {
            VK_LOG_ERROR("PBR::createDescriptorPool failed");
            return false;
        }

        VK_LOG_DEBUG("PBR::createDescriptorPool successful");
        return true;
    }

    bool PBR::createDescriptorSets() noexcept
    {
        // create identical descriptor set layouts for each swapchain image
        std::vector<VkDescriptorSetLayout> layouts(m_swapchainImageCount, m_cameraDescriptorSetLayout.get());

        // descriptor set allocation info
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pNext = nullptr;
        descriptorSetAllocateInfo.descriptorPool = m_descriptorPool.get();
        descriptorSetAllocateInfo.descriptorSetCount = m_swapchainImageCount;
        descriptorSetAllocateInfo.pSetLayouts = layouts.data();

        // allocate descriptor sets for camera
        m_cameraDescriptorSets.resize(m_swapchainImageCount);
        VkResult vkResult = vkAllocateDescriptorSets(m_vkDevice, &descriptorSetAllocateInfo, m_cameraDescriptorSets.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateDescriptorSets failed to allocate camera descriptor set : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // set light descriptor set layout
        std::fill(layouts.begin(), layouts.end(), m_lightDescriptorSetLayout.get());

        // allocate descriptor sets for light
        m_lightDescriptorSets.resize(m_swapchainImageCount);
        vkResult = vkAllocateDescriptorSets(m_vkDevice, &descriptorSetAllocateInfo, m_lightDescriptorSets.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateDescriptorSets failed to allocate light descriptor set : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // for each swapchain image, bind its corresponding uniform buffer to the descriptor set
        for (uint32_t i = 0; i < m_swapchainImageCount; i++)
        {
            VkWriteDescriptorSet uboWrite[2]{};

            // set: 0, binding: 0 (camera uniform)
            VkDescriptorBufferInfo cameraBufferInfo{};
            cameraBufferInfo.buffer   = m_cameraUniformBuffers[i].get();
            cameraBufferInfo.offset   = 0;
            cameraBufferInfo.range    = sizeof(ubo::Camera);

            uboWrite[0].sType                  = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uboWrite[0].pNext                  = nullptr;
            uboWrite[0].dstSet                 = m_cameraDescriptorSets[i];
            uboWrite[0].dstBinding             = 0;
            uboWrite[0].dstArrayElement        = 0;
            uboWrite[0].descriptorCount        = 1;
            uboWrite[0].descriptorType         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboWrite[0].pImageInfo             = nullptr;
            uboWrite[0].pBufferInfo            = &cameraBufferInfo;
            uboWrite[0].pTexelBufferView       = nullptr;

            // set: 2, binding: 0 (light uniform)
            VkDescriptorBufferInfo lightBufferInfo{};
            lightBufferInfo.buffer   = m_lightUniformBuffers[i].get();
            lightBufferInfo.offset   = 0;
            lightBufferInfo.range    = sizeof(ubo::Light);

            uboWrite[1].sType                  = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uboWrite[1].pNext                  = nullptr;
            uboWrite[1].dstSet                 = m_lightDescriptorSets[i];
            uboWrite[1].dstBinding             = 0;
            uboWrite[1].dstArrayElement        = 0;
            uboWrite[1].descriptorCount        = 1;
            uboWrite[1].descriptorType         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboWrite[1].pImageInfo             = nullptr;
            uboWrite[1].pBufferInfo            = &lightBufferInfo;
            uboWrite[1].pTexelBufferView       = nullptr;

            // commit the binding to the descriptor set
            vkUpdateDescriptorSets(m_vkDevice, 2, uboWrite, 0, nullptr);
        }

        // allocate and update descriptor sets for model
        m_gltfModel.allocateDescriptorSets(m_descriptorPool.get());
        m_gltfModel.updateDescriptorSets(m_samplers);
        
        VK_LOG_DEBUG("PBR::createDescriptorSets successful");
        return true;
    }

    bool PBR::createRenderPasses() noexcept
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
            resolveAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            resolveAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
            VK_LOG_ERROR("PBR::createRenderPasses failed to initialize render pass");
            return false;
        }

        VK_LOG_DEBUG("PBR::createRenderPasses successful");
        return true;
    }

    bool PBR::createGraphicsPipeline() noexcept
    {
        // retrieve shared vertex input layout
        const auto bindings = GLTFModel::getBindings();
        const auto attributes = GLTFModel::getAttributes();

        // vertex input state
        VkPipelineVertexInputStateCreateInfo vertexInputState{};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputState.pNext = nullptr;
        vertexInputState.flags = 0;
        vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        vertexInputState.pVertexBindingDescriptions = bindings.data();
        vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInputState.pVertexAttributeDescriptions = attributes.data();

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
        rasterizationState.cullMode = VK_CULL_MODE_NONE;
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
        pipelineConfig.mDescriptorSetLayouts.emplace_back(m_cameraDescriptorSetLayout.get());
        pipelineConfig.mDescriptorSetLayouts.emplace_back(GLTFModel::getDescriptorSetLayout());
        pipelineConfig.mDescriptorSetLayouts.emplace_back(m_lightDescriptorSetLayout.get());
        pipelineConfig.mPushConstantRanges.emplace_back(GLTFModel::getPushConstantRange());

        // create graphics pipeline
        if (!m_graphicsPipeline.initialize(m_vkDevice, pipelineConfig))
        {
            VK_LOG_ERROR("PBR::createGraphicsPipeline failed");
            return false;
        }

        VK_LOG_DEBUG("PBR::createGraphicsPipeline successful");
        return true;
    }

    bool PBR::createFramebuffers() noexcept
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
                VK_LOG_ERROR("PBR::createFramebuffers failed at swapchain image index %u", i);
                return false;
            }
        }

        VK_LOG_DEBUG("PBR::createFramebuffers successful");
        return true;
    }

    bool PBR::createSyncPrimitives() noexcept
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
                VK_LOG_ERROR("PBR::createSyncPrimitives failed to initialize image available semaphore : %u", i);
                return false;
            }

            // setup render complete semaphore
            if (!frameSync.mRenderCompleteSemaphore.initialize(m_vkDevice, semaphoreCreateInfo))
            {
                VK_LOG_ERROR("PBR::createSyncPrimitives failed to initialize render complete semaphore : %u", i);
                return false;
            }

            // setup in flight fence for command buffer execution
            if (!frameSync.mInFlightFence.initialize(m_vkDevice, fenceCreateInfo))
            {
                VK_LOG_ERROR("PBR::createSyncPrimitives failed to initialize fence : %u", i);
                return false;
            }
        }

        VK_LOG_DEBUG("PBR::createSyncPrimitives successful");
        return true;
    }

    bool PBR::buildCommandBuffers() noexcept
    {
        // check if msaa is enabled
        const bool msaaEnabled = m_sampleCount > VK_SAMPLE_COUNT_1_BIT;

        // clear values for render pass; color: 0, depth-stencil: 1
        VkClearValue clearValues[3]{};
        if (msaaEnabled)
        {
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[2].depthStencil = { 1.0f, 0 };
        }
        else 
        {
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };
        }

        // render pass begin info (framebuffer updated per command buffer)
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = m_renderPass.get();
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = m_swapchain->getExtent();
        renderPassBeginInfo.clearValueCount = msaaEnabled ? 3 : 2;
        renderPassBeginInfo.pClearValues = clearValues;

        // record commands per swapchain image
        for (uint32_t i = 0; i < m_swapchainImageCount; ++i)
        {
            // reset and then begin recording into the command buffer
            if (!m_commandBuffers[i].reset() || !m_commandBuffers[i].begin())
            {
                return false;
            }

            // begin render pass for this frame
            renderPassBeginInfo.framebuffer = m_framebuffers[i].get();
            m_commandBuffers[i].beginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // record graphics pipeline state, resource bindings, and draw commands for this frame
            m_commandBuffers[i].bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.get());
            m_commandBuffers[i].bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.getLayout(), 0, 1, &m_cameraDescriptorSets[i]);
            m_commandBuffers[i].bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.getLayout(), 2, 1, &m_lightDescriptorSets[i]);

            m_gltfModel.render(m_commandBuffers[i].get(), m_graphicsPipeline.getLayout());

            // end current render pass
            m_commandBuffers[i].endRenderPass();

            // finalize the command buffer
            if (!m_commandBuffers[i].end())
            {
                return false;
            }
        }

        VK_LOG_DEBUG("PBR::buildCommandBuffers successful");
        return true;
    }

    bool PBR::prepareScene() noexcept
    {
        auto platform = m_platform.lock();
        auto context = m_context.lock();

        // setup imgui layer
        m_imguiLayer = std::make_unique<ImGuiLayer>(*m_swapchain);
        m_imguiLayer->initialize(*platform, *context);
        platform->enableImGuiEvents(true);
        m_imguiLayer->registerWidget([this]()
        {
            updateUserInterface();
        });

        // calculate aspect ratio of window and initialize camera
        const float aspectRatio = static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight);
        m_camera = std::make_shared<Camera>(45.0f, aspectRatio, 0.1f, 100.0f, true);

        // register listeners with the platform
        platform->addListener(m_camera);

        // setup lighting data
        m_lightPosition  = glm::vec4(2.0f, 0.5f, 2.0f, 1.0f);
        m_lightColor     = glm::vec3(1.0f, 1.0f, 1.0f);
        m_lightIntensity = 100.0f;

        // mark ready for render
        m_readyToRender.store(true);
        return true;
    }

    void PBR::updateUserInterface() noexcept
    {
        if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            ImGui::DragFloat4("Position##light", &m_lightPosition.x, 0.1f);
            ImGui::ColorEdit3("Color##light", &m_lightColor.r);
            ImGui::DragFloat("Intensity", &m_lightIntensity, 1.0f, 0.0f, 1000.0f);
            ImGui::TreePop();
        }
    }

}   // namespace keplar
