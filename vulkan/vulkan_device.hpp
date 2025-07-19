// ────────────────────────────────────────────
//  File: vulkan_device.hpp · Created by Yash Patel · 7-1-2025
// ────────────────────────────────────────────

#pragma once 

#include <vector>
#include <optional>

#include "vk_config.hpp"
#include "vulkan_surface.hpp"

namespace keplar
{
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
            VulkanDevice();
            ~VulkanDevice();

            // disable copy and move semantics to enforce unique ownership
            VulkanDevice(const VulkanDevice&) = delete;
            VulkanDevice& operator=(const VulkanDevice&) = delete;
            VulkanDevice(VulkanDevice&&) = delete;
            VulkanDevice& operator=(VulkanDevice&&) = delete;

            bool initialize(const VulkanInstance& instance, const VulkanSurface& surface, const VulkanContextConfig& config);
            void destroy();

            // accessors
            VkDevice getDevice() const;
            VkPhysicalDevice getPhysicalDevice() const;
            QueueFamilyIndices getQueueFamilyIndices() const;
            VkQueue getGraphicsQueue() const;
            VkQueue getPresentQueue() const;
            VkQueue getComputeQueue() const;
            VkQueue getTransferQueue() const;

            // physical device information
            const VkPhysicalDeviceProperties& getPhysicalDeviceProperties() const;
            const VkPhysicalDeviceFeatures& getPhysicalDeviceFeatures() const;
            const VkPhysicalDeviceMemoryProperties& getPhysicalDeviceMemoryProperties() const;
            bool isExtensionEnabled(const char* extensionName) const;

        private:
            struct PhysicalDeviceInfo;

            bool selectPhysicalDevice(const VulkanSurface& surface);
            bool createLogicalDevice();
            bool getDeviceQueues();
            void validateRequestedFeatures();
            
            bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
            QueueFamilyIndices findRequiredQueueFamilies(VkPhysicalDevice device, const VulkanSurface& surface) const;
            uint64_t scoreDevice(const PhysicalDeviceInfo& physicalDeviceInfo) const;
            void logPhysicalDeviceInfo(const PhysicalDeviceInfo& physicalDeviceInfo) const;

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
