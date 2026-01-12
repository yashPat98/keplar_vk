// ────────────────────────────────────────────
//  File: vulkan_command_pool.cpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#include "vulkan_command_pool.hpp"
#include "vulkan_command_buffer.hpp"
#include "vulkan_utils.hpp"
#include "utils/logger.hpp"

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
        // destroy command pool
        if (m_vkCommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_vkDevice, m_vkCommandPool, nullptr);
            m_vkCommandPool = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_DEBUG("vulkan command pool destroyed successfully");
        }
    }

    VulkanCommandPool::VulkanCommandPool(VulkanCommandPool&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkCommandPool(other.m_vkCommandPool)
        , m_queueFamilyIndex(other.m_queueFamilyIndex)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkCommandPool = VK_NULL_HANDLE;
        other.m_queueFamilyIndex = std::nullopt;
    }

    VulkanCommandPool& VulkanCommandPool::operator=(VulkanCommandPool&& other) noexcept
    {
        // avoid self-move
        if (this != &other)
        {
            // release current resources
            if (m_vkCommandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(m_vkDevice, m_vkCommandPool, nullptr);
            }

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkCommandPool = other.m_vkCommandPool;
            m_queueFamilyIndex = other.m_queueFamilyIndex;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkCommandPool = VK_NULL_HANDLE;
            other.m_queueFamilyIndex = std::nullopt;
        }

        return *this;
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
        VK_LOG_DEBUG("vulkan command pool created successfully");
        return true;
    }

    bool VulkanCommandPool::reset(VkCommandPoolResetFlags flags) const noexcept
    {
        return VK_CHECK(vkResetCommandPool(m_vkDevice, m_vkCommandPool, flags));
    }

    void VulkanCommandPool::trim(VkCommandPoolTrimFlags flags) const noexcept
    {
        vkTrimCommandPool(m_vkDevice, m_vkCommandPool, flags);
    }

    VulkanCommandBuffer VulkanCommandPool::allocatePrimary() const noexcept
    {
        return allocateInternal(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    }

    VulkanCommandBuffer VulkanCommandPool::allocateSecondary() const noexcept
    {
        return allocateInternal(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    }

    std::vector<VulkanCommandBuffer> VulkanCommandPool::allocatePrimaries(uint32_t count) const 
    {
        return allocateInternal(count, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    }

    std::vector<VulkanCommandBuffer> VulkanCommandPool::allocateSecondaries(uint32_t count) const 
    {
        return allocateInternal(count, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    }

    void VulkanCommandPool::deallocate(VulkanCommandBuffer& commandBuffer) const noexcept
    {
        auto vkCommandBuffer = commandBuffer.get();
        if (vkCommandBuffer != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, 1, &vkCommandBuffer);
            commandBuffer = VulkanCommandBuffer{};
            VK_LOG_DEBUG("VulkanCommandPool::deallocate : command buffer freed successfully"); 
        }
    }

    void VulkanCommandPool::deallocate(std::vector<VulkanCommandBuffer>& commandBuffers) const 
    {
        // validate input
        if (commandBuffers.empty())
        {
            VK_LOG_WARN("VulkanCommandPool::deallocate : no command buffers passed");
            return;
        }

        std::vector<VkCommandBuffer> vkCommandBuffers;
        vkCommandBuffers.reserve(commandBuffers.size());
        for (auto& buffer : commandBuffers)
        {
            if (auto handle = buffer.get(); handle != VK_NULL_HANDLE)
            {
                vkCommandBuffers.emplace_back(handle);
            }
        }

        // free command buffers from the pool
        if (!vkCommandBuffers.empty())
        {
            vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, static_cast<uint32_t>(vkCommandBuffers.size()), vkCommandBuffers.data());
        }

        commandBuffers.clear();
        VK_LOG_DEBUG("VulkanCommandPool::deallocate : command buffers freed successfully : %zu", vkCommandBuffers.size()); 
    }

    VulkanCommandBuffer VulkanCommandPool::allocateInternal(VkCommandBufferLevel level) const noexcept
    {
        // set up command buffer allocation info
        VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo{};
        vkCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        vkCommandBufferAllocateInfo.pNext = nullptr;
        vkCommandBufferAllocateInfo.commandPool = m_vkCommandPool;
        vkCommandBufferAllocateInfo.level = level;
        vkCommandBufferAllocateInfo.commandBufferCount = 1;

        // create command buffer
        VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;
        const VkResult vkResult = vkAllocateCommandBuffers(m_vkDevice, &vkCommandBufferAllocateInfo, &vkCommandBuffer);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateCommandBuffers failed to allocate command buffer : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return {};
        }

        // create command buffer lightweight wrapper 
        VK_LOG_DEBUG("VulkanCommandPool::allocateInternal : single command buffer allocated successfully");
        return VulkanCommandBuffer{ vkCommandBuffer };
    }

    std::vector<VulkanCommandBuffer> VulkanCommandPool::allocateInternal(uint32_t count, VkCommandBufferLevel level) const 
    {
        // validate input
        if (count == 0)
        {
            VK_LOG_WARN("VulkanCommandPool::allocateInternal : count is invalid");
            return {};
        }

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
        for (VkCommandBuffer elm : vkCommandBuffers)
        {
            commandBuffers.emplace_back(VulkanCommandBuffer(elm));
        }

        VK_LOG_DEBUG("VulkanCommandPool::allocateInternal : command buffers allocated from pool successfully");
        return commandBuffers;
    }
}   // namespace keplar
