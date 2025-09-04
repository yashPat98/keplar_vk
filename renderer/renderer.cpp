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
        , m_maxFramesInFlight(glm::min(3u, m_swapchainImageCount))
        , m_currentImageIndex(0)
        , m_currentFrameIndex(0)
        , m_framebuffers(m_swapchainImageCount)
        , m_frameSyncPrimitives(m_maxFramesInFlight)
        , m_readyToRender(false)
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
        // initialize vulkan resources
        if (!createCommandPool()) { return false; }
        if (!createCommandBuffers()) { return false; }
        if (!createVertexBuffers()) { return false; }
        if (!createUniformBuffers()) { return false; }
        if (!createShaderModules()) { return false; }
        if (!createDescriptorSetLayouts()) { return false; }
        if (!createDescriptorPool()) { return false; }
        if (!createDescriptorSets()) { return false; }
        if (!createRenderPasses()) { return false; }
        if (!createGraphicsPipeline()) { return false; }
        if (!createFramebuffers()) { return false; }
        if (!createSyncPrimitives()) { return false; }
        if (!buildCommandBuffers()) { return false; }

        // initialization complete
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

        // 🔟 update uniform buffer for the current frame
        updateUniformBuffer();

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

    bool Renderer::createCommandPool()
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
            VK_LOG_ERROR("Renderer::createCommandPool : failed to initialize command pool.");
            return false;
        }

        VK_LOG_DEBUG("Renderer::createCommandPool successful");
        return true;
    }

    bool Renderer::createCommandBuffers()
    {
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

    bool Renderer::createVertexBuffers()
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
        if (!m_positionBuffer.createDeviceLocal(m_vulkanDevice, m_commandPool, bufferCreateInfo, triangle_position, sizeof(triangle_position)))
        {
            VK_LOG_ERROR("Renderer::createVertexBuffers failed to create device-local buffer for vertex positions");
            return false;
        }

        // create triangle color buffer
        bufferCreateInfo.size = sizeof(triangle_colors); 
        if (!m_colorBuffer.createDeviceLocal(m_vulkanDevice, m_commandPool, bufferCreateInfo, triangle_colors, sizeof(triangle_colors)))
        {
            VK_LOG_ERROR("Renderer::createVertexBuffers failed to create device-local buffer for vertex colors");
            return false;
        }
        
        VK_LOG_DEBUG("Renderer::createVertexBuffers successful");
        return true;
    }

    bool Renderer::createUniformBuffers()
    {
        // allocate space for vectors per swapchain image
        m_uniformBuffers.resize(m_swapchainImageCount);
        m_uboFrameData.resize(m_swapchainImageCount);

        // calculate required buffer size for uniform data
        VkDeviceSize bufferSize = sizeof(ubo::FrameData);

        // create a host-visible uniform buffer for each swapchain image
        for (size_t i = 0; i < m_swapchainImageCount; i++)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.pNext = nullptr;
            bufferCreateInfo.flags = 0;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferCreateInfo.size = bufferSize;

            // create host-visible buffer for uniform data
            if (!m_uniformBuffers[i].createHostVisible(m_vulkanDevice, bufferCreateInfo, nullptr, 0, true))
            {
                VK_LOG_ERROR("Renderer::createUniformBuffers failed to create host visible buffer for uniform data");
                return false;
            }
        }

        VK_LOG_DEBUG("Renderer::createUniformBuffers successful");
        return true;
    }

    bool Renderer::createShaderModules()
    {
        // create vertex shader module from SPIR-V
        if (!m_vertexShader.initialize(m_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, "passthrough.vert.spv"))
        {
            VK_LOG_ERROR("Renderer::createShaderModules failed for vertex shader");
            return false;
        }

        // create fragment shader module from SPIR-V
        if (!m_fragmentShader.initialize(m_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, "passthrough.frag.spv"))
        {
            VK_LOG_ERROR("Renderer::createShaderModules failed for fragment shader");
            return false;
        }

        VK_LOG_DEBUG("Renderer::createShaderModules successful");
        return true;
    }

    bool Renderer::createDescriptorSetLayouts()
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
            VK_LOG_ERROR("Renderer::createDescriptorSetLayouts failed to create descriptor set layout");
            return false;
        }

        VK_LOG_DEBUG("Renderer::createDescriptorSetLayouts successful");
        return true;
    }

    bool Renderer::createDescriptorPool()
    {
        // descriptor pool size info
        VkDescriptorPoolSize descriptorPoolSize{};
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSize.descriptorCount = 1;

        // descriptor pool creation info
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.pNext = nullptr;
        descriptorPoolInfo.flags = 0;
        descriptorPoolInfo.poolSizeCount = 1;
        descriptorPoolInfo.pPoolSizes = &descriptorPoolSize;
        descriptorPoolInfo.maxSets = m_swapchainImageCount;

        // create vulkan descriptor pool
        if (!m_descriptorPool.initialize(m_vkDevice, descriptorPoolInfo))
        {
            VK_LOG_ERROR("Renderer::createDescriptorPool failed");
            return false;
        }

        VK_LOG_DEBUG("Renderer::createDescriptorPool successful");
        return true;
    }

    bool Renderer::createDescriptorSets()
    {
        // create identical descriptor set layouts for each swapchain image
        std::vector<VkDescriptorSetLayout> layouts(m_swapchainImageCount, m_descriptorSetLayout.get());

        // descriptor set allocation info
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pNext = nullptr;
        descriptorSetAllocateInfo.descriptorPool = m_descriptorPool.get();
        descriptorSetAllocateInfo.descriptorSetCount = m_swapchainImageCount;
        descriptorSetAllocateInfo.pSetLayouts = layouts.data();

        // allocate descriptor sets
        m_descriptorSets.resize(m_swapchainImageCount);
        VkResult vkResult = vkAllocateDescriptorSets(m_vkDevice, &descriptorSetAllocateInfo, m_descriptorSets.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateDescriptorSets failed to allocate descriptor set : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // for each swapchain image, bind its corresponding uniform buffer to the descriptor set
        for (uint32_t i = 0; i < m_swapchainImageCount; i++)
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

        VK_LOG_DEBUG("Renderer::createDescriptorSets successful");
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

    bool Renderer::createGraphicsPipeline()
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
        auto swapchainExtent = m_vulkanSwapchain.getExtent();

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
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
        pipelineConfig.mColorBlendState = colorBlendState;
        pipelineConfig.mRenderPass = m_renderPass.get();
        pipelineConfig.mSubpassIndex = 0;
        pipelineConfig.mDescriptorSetLayouts.emplace_back(m_descriptorSetLayout.get());

        // create graphics pipeline
        if (!m_graphicsPipeline.initialize(m_vkDevice, pipelineConfig))
        {
            VK_LOG_ERROR("Renderer::createGraphicsPipeline failed");
            return false;
        }

        VK_LOG_DEBUG("Renderer::createGraphicsPipeline successful");
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
        clearColor.color.float32[2] = 0.0f;       // blue
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

        // vertex buffer bindings: position at binding 0, color at binding 1
        VkBuffer vertexBuffers[] = { m_positionBuffer.get(), m_colorBuffer.get() };
        VkDeviceSize offset[] = { 0, 0 };

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
            m_commandBuffers[i].bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.getLayout(), 0, 1, &m_descriptorSets[i]);
            m_commandBuffers[i].bindVertexBuffers(0, 2, vertexBuffers, offset);
            m_commandBuffers[i].draw(6, 1, 0, 0); 

            // end current render pass
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

    bool Renderer::updateUniformBuffer()
    {    
        // calculate aspect ratio
        const VkExtent2D extent = m_vulkanSwapchain.getExtent();
        const float aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);

        // update matrices for the current frame
        ubo::FrameData& frameData = m_uboFrameData[m_currentImageIndex];
        frameData.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
        frameData.view = glm::mat4(1.0f);
        frameData.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        frameData.projection[1][1] *= -1.0f;

        // upload data to the host-visible uniform buffer
        if (!m_uniformBuffers[m_currentImageIndex].uploadHostVisible(&frameData, sizeof(frameData)))
        {
            VK_LOG_ERROR("Renderer::updateUniformBuffers failed for frame: %d", m_currentImageIndex);
            return false;
        }
        
        return true;
    }
}   // namespace keplar
