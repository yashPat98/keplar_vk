// ────────────────────────────────────────────
//  File: imgui_layer.hpp · Created by Yash Patel · 10-17-2025
// ────────────────────────────────────────────

#pragma once

#include <functional>
#include <imgui.h>

#include "platform/platform.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_swapchain.hpp"
#include "vulkan/vulkan_command_pool.hpp"
#include "vulkan/vulkan_command_buffer.hpp"
#include "vulkan/vulkan_descriptor_pool.hpp"
#include "vulkan/vulkan_render_pass.hpp"
#include "vulkan/vulkan_framebuffer.hpp"

#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

namespace keplar
{
    class ImGuiLayer 
    {
        public:
            // creation and destruction
            explicit ImGuiLayer(const VulkanSwapchain& swapchain) noexcept;
            ~ImGuiLayer();

            // lifetime management
            bool initialize(const Platform& platform, const VulkanContext& context, uint32_t maxFramesInFlight) noexcept;
            void recreate(uint32_t maxFramesInFlight) noexcept;

            // usage
            VkCommandBuffer recordFrame(uint32_t frameIndex, uint32_t imageIndex) noexcept;
            void registerWidget(std::function<void()> callback);

        private:
            // internal helpers 
            bool createResourcePool() noexcept;
            bool createCommandBuffers() noexcept;
            bool createRenderPass() noexcept;
            bool createFramebuffers() noexcept;
            bool createImGuiBackend(const Platform& platform, const VulkanDevice& device) noexcept;
            void renderControlPanel() noexcept;

        private:
            // core vulkan dependencies
            const VulkanSwapchain&              m_swapchain;
            uint32_t                            m_maxFramesInFlight;

            // imgui info and vulkan resources
            ImGui_ImplVulkan_InitInfo           m_initInfo;
            VkInstance                          m_vkInstance;
            VkDevice                            m_vkDevice;
            VulkanCommandPool                   m_commandPool;
            VulkanDescriptorPool                m_descriptorPool;
            VulkanRenderPass                    m_renderPass;
            std::vector<VulkanCommandBuffer>    m_commandBuffers;
            std::vector<VulkanFramebuffer>      m_framebuffers;
            uint32_t                            m_queueFamily;
            uint32_t                            m_imageCount;
            VkExtent2D                          m_imageExtent;

            // registered callbacks
            std::vector<std::function<void()>>  m_widgetCallbacks;
    };  
}   // namespace keplar
