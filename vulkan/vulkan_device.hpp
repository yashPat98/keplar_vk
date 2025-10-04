// ────────────────────────────────────────────
//  File: vulkan_device.hpp · Created by Yash Patel · 7-1-2025
// ────────────────────────────────────────────

#pragma once 

#include <memory>
#include <vector>
#include <optional>

#include "vulkan_config.hpp"
#include "vulkan_surface.hpp"

namespace keplar
{
    // device creation config
    struct VulkanDeviceConfig
    {
        std::vector<const char*> mDeviceExtensions;
        VkPhysicalDeviceFeatures mRequestedFeatures;
        bool mPreferDedicatedComputeQueue = false;
        bool mPreferDedicatedTransferQueue = false;

        inline void setDeviceExtensions(const std::vector<std::string_view>& extensions)
        {
            mDeviceExtensions.reserve(extensions.size());
            for (const auto& extension : extensions)
            {
                mDeviceExtensions.emplace_back(extension.data());
            }
        }
    };

    // selected queue family indices
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> mGraphicsFamily;
        std::optional<uint32_t> mPresentFamily;
        std::optional<uint32_t> mComputeFamily;
        std::optional<uint32_t> mTransferFamily; 

        inline bool isComplete() const { return (mGraphicsFamily && mPresentFamily); }
    };

    class VulkanDevice final
    {
        public:
            // creation and destruction
            static std::shared_ptr<VulkanDevice> create(const VulkanInstance& instance, 
                                                        const VulkanSurface& surface, 
                                                        const VulkanContextConfig& config) noexcept;
            ~VulkanDevice();

            // disable copy and move semantics to enforce unique ownership
            VulkanDevice(const VulkanDevice&) = delete;
            VulkanDevice& operator=(const VulkanDevice&) = delete;
            VulkanDevice(VulkanDevice&&) = delete;
            VulkanDevice& operator=(VulkanDevice&&) = delete;

            // accessors
            VkDevice getDevice() const noexcept;
            VkPhysicalDevice getPhysicalDevice() const noexcept;
            QueueFamilyIndices getQueueFamilyIndices() const noexcept;
            VkQueue getGraphicsQueue() const noexcept;
            VkQueue getPresentQueue() const noexcept;
            VkQueue getComputeQueue() const noexcept;
            VkQueue getTransferQueue() const noexcept;

            // physical device information
            const VkPhysicalDeviceProperties& getPhysicalDeviceProperties() const noexcept;
            const VkPhysicalDeviceFeatures& getPhysicalDeviceFeatures() const noexcept;
            const VkPhysicalDeviceFeatures& getEnabledFeatures() const noexcept;
            const VkPhysicalDeviceMemoryProperties& getPhysicalDeviceMemoryProperties() const noexcept;
            bool isExtensionEnabled(const char* extensionName) const noexcept;

            // query properties
            std::optional<uint32_t> findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags propertyFlags) const noexcept;

        private:
            struct PhysicalDeviceInfo;

            // construction helpers
            VulkanDevice() noexcept;
            bool initialize(const VulkanInstance& instance, const VulkanSurface& surface, const VulkanContextConfig& config) noexcept;
            bool selectPhysicalDevice(const VulkanSurface& surface) noexcept;
            bool createLogicalDevice() noexcept;
            bool getDeviceQueues() noexcept;
            void validateRequestedFeatures() noexcept;
         
            bool checkDeviceExtensionSupport(VkPhysicalDevice device) const noexcept;
            QueueFamilyIndices findRequiredQueueFamilies(VkPhysicalDevice device, const VulkanSurface& surface) const noexcept;
            uint64_t scoreDevice(const PhysicalDeviceInfo& physicalDeviceInfo) const noexcept;
            void logPhysicalDeviceInfo(const PhysicalDeviceInfo& physicalDeviceInfo) const noexcept;

        private:
            // vulkan handles
            VkInstance m_vkInstance;
            VkPhysicalDevice m_vkPhysicalDevice;
            VkDevice m_vkDevice;

            // queues
            VkQueue m_graphicsQueue;
            VkQueue m_presentQueue;
            VkQueue m_computeQueue;
            VkQueue m_transferQueue;
            QueueFamilyIndices m_queueFamilyIndices;

            // device properties and features
            VkPhysicalDeviceProperties m_vkPhysicalDeviceProperties;
            VkPhysicalDeviceFeatures m_vkPhysicalDeviceFeatures;
            VkPhysicalDeviceMemoryProperties m_vkPhysicalDeviceMemoryProperties;
            VulkanDeviceConfig m_deviceConfig;
    };
}   // namespace keplar
