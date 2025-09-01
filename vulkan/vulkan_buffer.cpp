// ────────────────────────────────────────────
//  File: vulkan_buffer.cpp · Created by Yash Patel · 8-2-2025
// ────────────────────────────────────────────

#include "vulkan_buffer.hpp"
#include "utils/logger.hpp"
#include "vulkan/vulkan_command_buffer.hpp"

namespace keplar
{
    VulkanBuffer::VulkanBuffer() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkBuffer(VK_NULL_HANDLE)
        , m_vkDeviceMemory(VK_NULL_HANDLE)
        , m_allocationSize(0)
        , m_mappedData(nullptr)
    {
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_mappedData)
        {
            vkUnmapMemory(m_vkDevice, m_vkDeviceMemory);
            VK_LOG_INFO("vulkan memory unmapped successfully");
            m_mappedData = nullptr;
        }

        if (m_vkDeviceMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_vkDevice, m_vkDeviceMemory, nullptr);
            m_vkDeviceMemory = VK_NULL_HANDLE;
            VK_LOG_INFO("vulkan buffer memory freed successfully");
        }

        if (m_vkBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(m_vkDevice, m_vkBuffer, nullptr);
            m_vkBuffer = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_INFO("vulkan buffer destroyed successfully");
        }
    }

    VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkBuffer(other.m_vkBuffer)
        , m_vkDeviceMemory(other.m_vkDeviceMemory)
        , m_allocationSize(other.m_allocationSize)
        , m_copyFence(std::move(other.m_copyFence))
        , m_mappedData(other.m_mappedData)
    {
        // reset the other
        other.m_vkDevice       = VK_NULL_HANDLE;
        other.m_vkBuffer       = VK_NULL_HANDLE;
        other.m_vkDeviceMemory = VK_NULL_HANDLE;
        other.m_allocationSize = 0;
        other.m_mappedData     = nullptr;
    }

    VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
    {
        // avoid self-move
        if (this != &other)
        {
            // release existing vulkan resources
            if (m_mappedData)
            {
                vkUnmapMemory(m_vkDevice, m_vkDeviceMemory);
            }

            if (m_vkDeviceMemory != VK_NULL_HANDLE)
            {
                vkFreeMemory(m_vkDevice, m_vkDeviceMemory, nullptr);
            }

            if (m_vkBuffer != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(m_vkDevice, m_vkBuffer, nullptr);
            }

            // transfer ownership
            m_vkDevice       = other.m_vkDevice;
            m_vkBuffer       = other.m_vkBuffer;
            m_vkDeviceMemory = other.m_vkDeviceMemory;
            m_allocationSize = other.m_allocationSize;
            m_copyFence      = std::move(other.m_copyFence);
            m_mappedData     = other.m_mappedData;  
            
            // reset the other
            other.m_vkDevice       = VK_NULL_HANDLE;
            other.m_vkBuffer       = VK_NULL_HANDLE;
            other.m_vkDeviceMemory = VK_NULL_HANDLE;
            other.m_allocationSize = 0;
            other.m_mappedData     = nullptr;
        }

        return *this;
    }

    bool VulkanBuffer::createHostVisible(const VulkanDevice& device, 
                                         const VkBufferCreateInfo& createInfo, 
                                         const void* data,
                                         size_t size, 
                                         bool persistMapped) noexcept
    {
        // get raw vulkan device handle
        m_vkDevice = device.getDevice();

        // create vertex buffer and allocate memory
        VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if (!createBuffer(device, createInfo, propertyFlags, m_vkBuffer, m_vkDeviceMemory))
        {
            return false;
        }
        
        // map memory 
        void* mappedData = nullptr;
        VkResult vkResult = vkMapMemory(m_vkDevice, m_vkDeviceMemory, 0, m_allocationSize, 0, &mappedData);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkMapMemory failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // copy initial data if provided
        if (data != nullptr && size != 0)
        {
            std::memcpy(mappedData, data, size);
        }

        // optionally keep memory mapped for frequent updates
        if (persistMapped)
        {
            m_mappedData = mappedData;
        }
        else 
        {
            vkUnmapMemory(m_vkDevice, m_vkDeviceMemory);
        }

        VK_LOG_DEBUG("VulkanBuffer::createHostVisible successful");
        return true;
    }

    bool VulkanBuffer::createDeviceLocal(const VulkanDevice& device,
                                         const VulkanCommandPool& commandPool, 
                                         const VkBufferCreateInfo& createInfo, 
                                         const void* data, 
                                         size_t size, 
                                         std::optional<std::reference_wrapper<ThreadPool>> threadPool) noexcept
    {
        // validate input data
        if (data == nullptr || size == 0)
        {
            VK_LOG_WARN("VulkanBuffer::createDeviceLocal invalid input data or size");
            return false;
        }

        // get raw vulkan device handle
        m_vkDevice = device.getDevice();

        // staging buffer creation info
        VkBufferCreateInfo createInfoStaging{};
        createInfoStaging.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfoStaging.pNext = nullptr;
        createInfoStaging.flags = 0;
        createInfoStaging.size = size;
        createInfoStaging.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        createInfoStaging.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
        // staging buffer resources used for temporary host-to-device copy
        VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBuffer vkBufferStaging = VK_NULL_HANDLE;
        VkDeviceMemory vkDeviceMemoryStaging = VK_NULL_HANDLE;

        // create staging vertex buffer and allocate memory 
        if (!createBuffer(device, createInfoStaging, propertyFlags, vkBufferStaging, vkDeviceMemoryStaging))
        {
            return false;
        }

        // map memory 
        void* mappedData = nullptr;
        VkResult vkResult = vkMapMemory(m_vkDevice, vkDeviceMemoryStaging, 0, m_allocationSize, 0, &mappedData);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkMapMemory failed for staging buffer : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, vkDeviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // copy data to mapped memory
        std::memcpy(mappedData, data, size);

        // unmap memory
        vkUnmapMemory(m_vkDevice, vkDeviceMemoryStaging);

        // create device local buffer and allocate memory 
        if (!createBuffer(device, createInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vkBuffer, m_vkDeviceMemory))
        {
            vkFreeMemory(m_vkDevice, vkDeviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // allocate one-time submit command buffer from pool for host to device copy
        auto commandBufferOpt = commandPool.allocateBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        if (!commandBufferOpt)
        {
            vkFreeMemory(m_vkDevice, vkDeviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // safely unwrap the optional
        auto& commandBuffer = *commandBufferOpt;

        // begin recording commands
        if (!commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
        {
            vkFreeMemory(m_vkDevice, vkDeviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // copy from staging host buffer to device-local buffer
        VkBufferCopy vkBufferCopy{};
        vkBufferCopy.srcOffset = 0;
        vkBufferCopy.dstOffset = 0;
        vkBufferCopy.size = size;
        commandBuffer.copyBuffer(vkBufferStaging, m_vkBuffer, 1, &vkBufferCopy);

        // end recording commands
        if (!commandBuffer.end())
        {
            vkFreeMemory(m_vkDevice, vkDeviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // create a fence to track copy completion
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;
        fenceCreateInfo.flags = 0;  
        m_copyFence.initialize(m_vkDevice, fenceCreateInfo);
 
        // submit info for executing the command buffer
        VkCommandBuffer vkCommandBuffer = commandBuffer.get();
        VkSubmitInfo vkSubmitInfo{};
        vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        vkSubmitInfo.pNext = nullptr;
        vkSubmitInfo.commandBufferCount = 1;
        vkSubmitInfo.pCommandBuffers = &vkCommandBuffer;

        // submit the copy command and signal the fence on completion
        vkResult = vkQueueSubmit(device.getGraphicsQueue(), 1, &vkSubmitInfo, m_copyFence.get());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkQueueSubmit failed for buffer copy from host to device : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, vkDeviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        if (threadPool)
        { 
            // cleanup resources asynchronously on a worker thread
            threadPool->get().dispatch([this, 
                                        vkBufferStaging, 
                                        vkDeviceMemoryStaging, 
                                        vkCommandPool = commandPool.get(), 
                                        vkCommandBuffer = commandBuffer.get()]()
            {
                m_copyFence.wait();  
                vkFreeCommandBuffers(m_vkDevice, vkCommandPool, 1, &vkCommandBuffer);
                vkFreeMemory(m_vkDevice, vkDeviceMemoryStaging, nullptr);
                vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
                VK_LOG_DEBUG("VulkanBuffer::createDeviceLocal staging resources freed successfully");
            });
        }
        else 
        {
            // cleanup resources synchronously on current thread
            m_copyFence.wait();
            commandPool.freeBuffer(commandBuffer);
            vkFreeMemory(m_vkDevice, vkDeviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
        }

        VK_LOG_DEBUG("VulkanBuffer::createDeviceLocal successful");
        return true;
    }

    bool VulkanBuffer::uploadHostVisible(const void* data, size_t size, VkDeviceSize offset) noexcept
    {
        // validate input data
        if (data == nullptr || size == 0)
        {
            VK_LOG_WARN("VulkanBuffer::uploadHostVisible invalid input data or size");
            return false;
        }

        // use persisted mapped pointer if available
        if (m_mappedData)
        {
            std::memcpy(m_mappedData, data, size);
            return true;
        }

        // map buffer memory for host access
        void* mappedData = nullptr;
        VkResult vkResult = vkMapMemory(m_vkDevice, m_vkDeviceMemory, offset, m_allocationSize, 0, &mappedData);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkMapMemory failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // copy data to mapped memory
        std::memcpy(mappedData, data, size);

        // unmap buffer memory after copy
        vkUnmapMemory(m_vkDevice, m_vkDeviceMemory);
        return true;
    }

    void VulkanBuffer::waitForStagingUpload() noexcept
    {
        // wait until the GPU completes the buffer copy command
        if (m_copyFence.isValid())
        {
            m_copyFence.wait();
        }
    }

    bool VulkanBuffer::createBuffer(const VulkanDevice& device,
                                    const VkBufferCreateInfo& createInfo, 
                                    VkMemoryPropertyFlags propertyFlags, 
                                    VkBuffer& vkBuffer, 
                                    VkDeviceMemory& vkDeviceMemory) noexcept
    {
        // create vulkan buffer
        VkResult vkResult = vkCreateBuffer(m_vkDevice, &createInfo, nullptr, &vkBuffer);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateBuffer failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // query memory requirements
        VkMemoryRequirements vkMemoryRequirements{};
        vkGetBufferMemoryRequirements(m_vkDevice, vkBuffer, &vkMemoryRequirements);

        // find suitable memory type
        auto memoryTypeIndex = device.findMemoryType(vkMemoryRequirements.memoryTypeBits, propertyFlags);
        if (!memoryTypeIndex)
        {
            return false;
        }

        // allocate device memory from vulkan heap
        VkMemoryAllocateInfo vkMemoryAllocateInfo{};
        vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vkMemoryAllocateInfo.pNext = nullptr;
        vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
        vkMemoryAllocateInfo.memoryTypeIndex = *memoryTypeIndex;

        vkResult = vkAllocateMemory(m_vkDevice, &vkMemoryAllocateInfo, nullptr, &vkDeviceMemory);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateMemory failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // bind memory to buffer
        vkResult = vkBindBufferMemory(m_vkDevice, vkBuffer, vkDeviceMemory, 0);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkBindBufferMemory failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // store allocation size for mapping 
        m_allocationSize = vkMemoryAllocateInfo.allocationSize;
        VK_LOG_DEBUG("VulkanBuffer::createBuffer successful");
        return true;
    }
}   // namespace keplar
