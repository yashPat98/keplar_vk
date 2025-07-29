// ────────────────────────────────────────────
//  File: vulkan_command_pool.cpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#include "vulkan_command_pool.hpp"
#include "vulkan_command_buffer.hpp"
#include "utils/logger.hpp"
#include "vk_utils.hpp"

namespace keplar
{
    VulkanCommandPool::VulkanCommandPool() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkCommandPool(VK_NULL_HANDLE)
        , m_queueFamilyIndex(std::nullopt)
    {
    }

    VulkanCommandPool::~VulkanCommandPool()
    {
        // free all command buffer allocated from pool
        if (!m_ownedCommandBuffers.empty())
        {
            vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, static_cast<uint32_t>(m_ownedCommandBuffers.size()), m_ownedCommandBuffers.data());
            VK_LOG_INFO("command buffers freed successfully : %zu", m_ownedCommandBuffers.size());
        }

        // destroy command pool
        if (m_vkCommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_vkDevice, m_vkCommandPool, nullptr);
            m_vkCommandPool = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_INFO("vulkan command pool destroyed successfully");
        }
    }

    bool VulkanCommandPool::initialize(VkDevice vkDevice, const VkCommandPoolCreateInfo& createInfo) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanCommandPool::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        // create the command pool
        VkResult vkResult = vkCreateCommandPool(vkDevice, &createInfo, nullptr, &m_vkCommandPool);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateCommandPool failed to create command pool : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // store device and queue family index
        m_vkDevice = vkDevice;
        m_queueFamilyIndex = createInfo.queueFamilyIndex;
        VK_LOG_INFO("vulkan command pool created successfully");
        return true;
    }

    bool VulkanCommandPool::reset(VkCommandPoolResetFlags flags) noexcept
    {
        return VK_CHECK(vkResetCommandPool(m_vkDevice, m_vkCommandPool, flags));
    }

    void VulkanCommandPool::trim(VkCommandPoolTrimFlags flags) noexcept
    {
        vkTrimCommandPool(m_vkDevice, m_vkCommandPool, flags);
    }

    std::optional<VulkanCommandBuffer> VulkanCommandPool::allocate(VkCommandBufferLevel level) noexcept
    {
        // set up command buffer allocation info for a single command buffer
        VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo{};
        vkCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        vkCommandBufferAllocateInfo.pNext = nullptr;
        vkCommandBufferAllocateInfo.commandPool = m_vkCommandPool;
        vkCommandBufferAllocateInfo.level = level;
        vkCommandBufferAllocateInfo.commandBufferCount = 1;

        // create command buffer
        VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;
        VkResult vkResult = vkAllocateCommandBuffers(m_vkDevice, &vkCommandBufferAllocateInfo, &vkCommandBuffer);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateCommandBuffers failed to allocate command buffer : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return std::nullopt;
        }

        VK_LOG_INFO("command buffer allocated from pool successfully");
        m_ownedCommandBuffers.emplace_back(vkCommandBuffer);
        return VulkanCommandBuffer(vkCommandBuffer);
    }

    std::vector<VulkanCommandBuffer> VulkanCommandPool::allocate(uint32_t count, VkCommandBufferLevel level) noexcept
    {
        // set up command buffer allocation info 
        VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo{};
        vkCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        vkCommandBufferAllocateInfo.pNext = nullptr;
        vkCommandBufferAllocateInfo.commandPool = m_vkCommandPool;
        vkCommandBufferAllocateInfo.level = level;
        vkCommandBufferAllocateInfo.commandBufferCount = count;

        // create command buffer
        std::vector<VkCommandBuffer> vkCommandBuffers(count);
        VkResult vkResult = vkAllocateCommandBuffers(m_vkDevice, &vkCommandBufferAllocateInfo, vkCommandBuffers.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateCommandBuffers failed to allocate command buffer : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return {};
        }

        // create command buffer lightweight wrappers and track for destruction
        std::vector<VulkanCommandBuffer> commandBuffers;
        commandBuffers.reserve(count);
        m_ownedCommandBuffers.reserve(m_ownedCommandBuffers.size() + count);
        for (const auto& elm : vkCommandBuffers)
        {
            commandBuffers.emplace_back(VulkanCommandBuffer(elm));
            m_ownedCommandBuffers.emplace_back(elm);
        }

        VK_LOG_INFO("command buffers allocated from pool successfully");
        return commandBuffers;
    }

    void VulkanCommandPool::freeCommandBuffers(const std::vector<VulkanCommandBuffer>& commandBuffers) noexcept
    {
        // validate input
        if (commandBuffers.empty())
        {
            VK_LOG_WARN("VulkanCommandPool::freeCommandBuffers : no command buffers passed");
            return;
        }

        // extract raw VkCommandBuffer handles
        std::vector<VkCommandBuffer> vkCommandBuffers;
        vkCommandBuffers.reserve(commandBuffers.size());
        for (const auto& buffer : commandBuffers)
        {
            vkCommandBuffers.emplace_back(buffer.get());
        }

        // free command buffers from the Vulkan command pool
        vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, static_cast<uint32_t>(vkCommandBuffers.size()), vkCommandBuffers.data());
        VK_LOG_INFO("command buffers freed successfully : %zu", vkCommandBuffers.size()); 

        // remove freed buffers 
        for (auto elm : vkCommandBuffers)
        {
            auto it = std::find(m_ownedCommandBuffers.begin(), m_ownedCommandBuffers.end(), elm);
            if (it != m_ownedCommandBuffers.end())
            {
                m_ownedCommandBuffers.erase(it);
            }
        }
    }
}   // namespace keplar
