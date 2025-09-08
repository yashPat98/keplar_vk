// ────────────────────────────────────────────
//  File: vulkan_render_pass.cpp · Created by Yash Patel · 7-27-2025
// ────────────────────────────────────────────

#include "vulkan_render_pass.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanRenderPass::VulkanRenderPass() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkRenderPass(VK_NULL_HANDLE)
    {
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        destroy();
    }

    VulkanRenderPass::VulkanRenderPass(VulkanRenderPass&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkRenderPass(other.m_vkRenderPass)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkRenderPass = VK_NULL_HANDLE;
    }

    VulkanRenderPass& VulkanRenderPass::operator=(VulkanRenderPass&& other) noexcept
    {
        if (this != &other)
        {
            // release current resources
            destroy();

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkRenderPass = other.m_vkRenderPass;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkRenderPass = VK_NULL_HANDLE;
        }

        return *this;
    }
 
    bool VulkanRenderPass::initialize(VkDevice vkDevice,
                                const std::vector<VkAttachmentDescription>& attachments,
                                const std::vector<VkSubpassDescription>& subpasses,
                                const std::vector<VkSubpassDependency>& dependencies) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanRenderPass::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        // as per vulkan spec render pass must contain at least one subpass
        if (subpasses.empty())
        {
            VK_LOG_ERROR("VulkanRenderPass::initialize failed: at least one subpass is required");
            return false;
        }

        // set up render pass creation info
        VkRenderPassCreateInfo vkRenderPassCreateInfo{};
        vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        vkRenderPassCreateInfo.pNext = nullptr;
        vkRenderPassCreateInfo.flags = 0;
        vkRenderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        vkRenderPassCreateInfo.pAttachments = attachments.data();
        vkRenderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
        vkRenderPassCreateInfo.pSubpasses = subpasses.data();
        vkRenderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        vkRenderPassCreateInfo.pDependencies = dependencies.data();

        // create the render pass
        VkResult vkResult = vkCreateRenderPass(vkDevice, &vkRenderPassCreateInfo, nullptr, &m_vkRenderPass);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateRenderPass failed to create render pass : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // store device handle for destruction
        m_vkDevice = vkDevice;
        VK_LOG_DEBUG("vulkan render pass created successfully");
        return true;
    }

    void VulkanRenderPass::destroy() noexcept
    {
        // destroy render pass
        if (m_vkRenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_vkDevice, m_vkRenderPass, nullptr);
            m_vkDevice = VK_NULL_HANDLE;
            m_vkRenderPass = VK_NULL_HANDLE;
            VK_LOG_DEBUG("vulkan render pass destroyed successfully");
        }
    }
}   // namespace keplar
