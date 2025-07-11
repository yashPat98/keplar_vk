// ────────────────────────────────────────────
//  File: vulkan_device.cpp · Created by Yash Patel · 7-1-2025
// ────────────────────────────────────────────

#include "vulkan_device.hpp"

#include <unordered_set>
#include "utils/logger.hpp"
#include "vk_utils.hpp"

namespace keplar
{
    struct VulkanDevice::PhysicalDeviceInfo
    {
        VkPhysicalDevice mVkPhysicalDevice;
        QueueFamilyIndices mQueueFamilyIndices;
        VkPhysicalDeviceProperties mVkPhysicalDeviceProperties;
        VkPhysicalDeviceMemoryProperties mVkPhysicalDeviceMemoryProperties;
        VkPhysicalDeviceFeatures mVkPhysicalDeviceFeatures;
        uint64_t mScore;
    };

    VulkanDevice::VulkanDevice()
        : m_vkInstance(VK_NULL_HANDLE)
        , m_vkPhysicalDevice(VK_NULL_HANDLE)
        , m_vkDevice(VK_NULL_HANDLE)
        , m_graphicsQueue(VK_NULL_HANDLE)
        , m_computeQueue(VK_NULL_HANDLE)
        , m_transferQueue(VK_NULL_HANDLE)
        , m_vkPhysicalDeviceProperties{}
        , m_vkPhysicalDeviceFeatures{}
        , m_vkPhysicalDeviceMemoryProperties{}
    {
    }

    VulkanDevice::~VulkanDevice()
    {
        destroy();
    }

    bool VulkanDevice::initialize(const VulkanInstance& instance, const VulkanSurface& surface, const VulkanContextConfig& config)
    {
        // store the VkInstance for resource creation and destruction
        m_vkInstance = instance.get();
        
        // copy device config params from builder config
        m_deviceConfig.mDeviceExtensions = config.mDeviceExtensions;
        m_deviceConfig.mRequestedFeatures = config.mRequestedFeatures;
        m_deviceConfig.mPreferDedicatedComputeQueue = config.mPreferDedicatedComputeQueue;
        m_deviceConfig.mPreferDedicatedTransferQueue = config.mPreferDedicatedTransferQueue;

        // select appropriate physical device
        if (!selectPhysicalDevice(surface))
        {
            VK_LOG_FATAL("failed to select a suitable vulkan physical device");
            return false;
        }

        // create logical device 
        if (!createLogicalDevice())
        {
            VK_LOG_FATAL("failed to create logical device from selected vulkan physical device");
            return false;
        }

        // get device queues
        if (!getDeviceQueues())
        {
            VK_LOG_FATAL("failed to retrieve device queues");
            return false;
        }

        return true;
    }

    void VulkanDevice::destroy()
    {
        if (m_vkDevice)
        {
            // queues are destroyed when logical device is destroyed
            vkDestroyDevice(m_vkDevice, nullptr);
            m_vkDevice = VK_NULL_HANDLE;
            m_graphicsQueue = VK_NULL_HANDLE;
            m_computeQueue = VK_NULL_HANDLE;
            m_transferQueue = VK_NULL_HANDLE; 
            VK_LOG_INFO("logical device destroyed successfully.");
        }
    }

    VkDevice VulkanDevice::getDevice() const
    {
        return m_vkDevice;
    }

    VkPhysicalDevice VulkanDevice::getPhysicalDevice() const
    {
        return m_vkPhysicalDevice;
    }

    VkQueue VulkanDevice::getGraphicsQueue() const
    {
        return m_graphicsQueue;
    }

    VkQueue VulkanDevice::getPresentQueue() const
    {
        return m_presentQueue;
    }

    VkQueue VulkanDevice::getComputeQueue() const
    {
        return m_computeQueue;
    }

    VkQueue VulkanDevice::getTransferQueue() const
    {
        return m_transferQueue;
    }

    const VkPhysicalDeviceProperties& VulkanDevice::getPhysicalDeviceProperties() const
    {
        return m_vkPhysicalDeviceProperties;
    }

    const VkPhysicalDeviceFeatures& VulkanDevice::getPhysicalDeviceFeatures() const
    {
        return m_vkPhysicalDeviceFeatures;
    }

    const VkPhysicalDeviceMemoryProperties& VulkanDevice::getPhysicalDeviceMemoryProperties() const
    {
        return m_vkPhysicalDeviceMemoryProperties;
    }

    bool VulkanDevice::isExtensionEnabled(const char* extensionName) const
    {
        return std::find(m_deviceConfig.mDeviceExtensions.begin(), m_deviceConfig.mDeviceExtensions.end(), extensionName) != m_deviceConfig.mDeviceExtensions.end();
    }

    bool VulkanDevice::selectPhysicalDevice(const VulkanSurface& surface)
    {
        // query the number of vulkan compatible physical devices.
        uint32_t deviceCount = 0;
        VkResult vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query physical device count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }
        
        // abort if no vulkan compatible physical devices were found.
        if (deviceCount == 0)
        {
            VK_LOG_FATAL("no vulkan-compatible physical devices found.");
            return false;
        }

        // retrieve the supported vulkan compatible physical devices.
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, physicalDevices.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query physical devices : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // sanitize device extensions
        validateDeviceExtensions();

        // select best vulkan physical device from available devices
        std::optional<PhysicalDeviceInfo> selectedDeviceInfo;
        for (const auto& device : physicalDevices)
        {
            // get device properties early to log device name if skipped.
            PhysicalDeviceInfo physicalDeviceInfo{};
            vkGetPhysicalDeviceProperties(device, &physicalDeviceInfo.mVkPhysicalDeviceProperties);

            // skip devices that don't support required extensions
            if (!checkDeviceExtensionSupport(device))
            {
                VK_LOG_WARN("skipping device %s due to missing required extension support", physicalDeviceInfo.mVkPhysicalDeviceProperties.deviceName);
                continue;
            }

            // device must support a graphics and present queue for rendering
            QueueFamilyIndices indices = findRequiredQueueFamilies(device, surface);
            if (!indices.isComplete())
            {
                VK_LOG_WARN("skipping device %s due to missing graphics or present queue support", physicalDeviceInfo.mVkPhysicalDeviceProperties.deviceName);
                continue;
            }

            // retrieve additional physical device info for scoring.
            physicalDeviceInfo.mVkPhysicalDevice = device;
            physicalDeviceInfo.mQueueFamilyIndices = std::move(indices);
            vkGetPhysicalDeviceMemoryProperties(device, &physicalDeviceInfo.mVkPhysicalDeviceMemoryProperties);
            vkGetPhysicalDeviceFeatures(device, &physicalDeviceInfo.mVkPhysicalDeviceFeatures);
            physicalDeviceInfo.mScore = scoreDevice(physicalDeviceInfo);

            // select the best physical device based on score
            if (!selectedDeviceInfo || physicalDeviceInfo.mScore > selectedDeviceInfo->mScore)
            {
                selectedDeviceInfo = physicalDeviceInfo;
            }
        }

        // abort if no suitable physical device was found.
        if (!selectedDeviceInfo)
        {
            VK_LOG_FATAL("no suitable vulkan physical device found.");
            return false;
        }

        // set selected physical device and its properties
        m_vkPhysicalDevice = selectedDeviceInfo->mVkPhysicalDevice;
        m_queueFamilyIndices = selectedDeviceInfo->mQueueFamilyIndices;
        m_vkPhysicalDeviceProperties = selectedDeviceInfo->mVkPhysicalDeviceProperties;
        m_vkPhysicalDeviceMemoryProperties = selectedDeviceInfo->mVkPhysicalDeviceMemoryProperties;
        m_vkPhysicalDeviceFeatures = selectedDeviceInfo->mVkPhysicalDeviceFeatures;

        // log device info and sanitize requested features
        logPhysicalDeviceInfo(*selectedDeviceInfo);
        validateRequestedFeatures();

        return true;
    }

    bool VulkanDevice::createLogicalDevice()
    {
        // collect unique queue family indices
        std::unordered_set<uint32_t> uniqueQueueFamilies;
        uniqueQueueFamilies.insert(*m_queueFamilyIndices.mGraphicsFamily);
        uniqueQueueFamilies.insert(*m_queueFamilyIndices.mPresentFamily);
        if (m_queueFamilyIndices.mComputeFamily) uniqueQueueFamilies.insert(*m_queueFamilyIndices.mComputeFamily);
        if (m_queueFamilyIndices.mTransferFamily) uniqueQueueFamilies.insert(*m_queueFamilyIndices.mTransferFamily);

        // create VkDeviceQueueCreateInfo for each unique queue family
        std::vector<VkDeviceQueueCreateInfo> vkQueueCreateInfos;
        vkQueueCreateInfos.reserve(uniqueQueueFamilies.size());

        const float queuePriorities[] = { 1.0f };
        for (const auto& queueFamilyIndex : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.pNext = nullptr;
            queueCreateInfo.flags = 0;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = queuePriorities;
            vkQueueCreateInfos.emplace_back(std::move(queueCreateInfo));
        }

        // setup logical device creation info struct
        VkDeviceCreateInfo vkDeviceCreateInfo{};
        vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        vkDeviceCreateInfo.pNext = nullptr;
        vkDeviceCreateInfo.flags = 0;
        vkDeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(vkQueueCreateInfos.size());
        vkDeviceCreateInfo.pQueueCreateInfos = vkQueueCreateInfos.data();
        vkDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceConfig.mDeviceExtensions.size());
        vkDeviceCreateInfo.ppEnabledExtensionNames = m_deviceConfig.mDeviceExtensions.data();
        vkDeviceCreateInfo.enabledLayerCount = 0;
        vkDeviceCreateInfo.ppEnabledLayerNames = nullptr;
        vkDeviceCreateInfo.pEnabledFeatures = &m_deviceConfig.mRequestedFeatures;              

        // create the Vulkan logical device
        VkResult vkResult = vkCreateDevice(m_vkPhysicalDevice, &vkDeviceCreateInfo, nullptr, &m_vkDevice);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateDevice failed to create logical device : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        return true;
    }

    bool VulkanDevice::getDeviceQueues()
    {
        // graphics queue (required)
        vkGetDeviceQueue(m_vkDevice, *m_queueFamilyIndices.mGraphicsFamily, 0, &m_graphicsQueue);
        if (m_graphicsQueue == VK_NULL_HANDLE)
        {
            VK_LOG_FATAL("vkGetDeviceQueue failed to retrieve graphics queue.");
            return false;
        }

        // helper lamda to get the queue or alias to graphics if it shares the same family
        auto getQueue = [&](const std::optional<uint32_t>& familyIndex, const char* name) -> VkQueue 
        {
            VkQueue vkQueue = VK_NULL_HANDLE;
            if (!familyIndex.has_value())
            {
                VK_LOG_WARN("no %s queue family found.", name);
                return vkQueue;
            }

            if (*familyIndex == *m_queueFamilyIndices.mGraphicsFamily)
            {
                VK_LOG_INFO("%s queue shares family with graphics queue", name);
                return m_graphicsQueue;
            }

            vkGetDeviceQueue(m_vkDevice, *familyIndex, 0, &vkQueue);
            if (vkQueue == VK_NULL_HANDLE)
            {
                VK_LOG_WARN("failed to retrieve %s queue.", name);
            }

            return vkQueue;
        };

        // present queue (required)
        m_presentQueue = getQueue(m_queueFamilyIndices.mPresentFamily, "present");
        if (m_presentQueue == VK_NULL_HANDLE)
        {
            return false;
        }
 
        // compute and transfer queue (optional)
        m_computeQueue = getQueue(m_queueFamilyIndices.mComputeFamily, "compute");
        m_transferQueue = getQueue(m_queueFamilyIndices.mTransferFamily, "transfer");
        return true;
    }

    QueueFamilyIndices VulkanDevice::findRequiredQueueFamilies(VkPhysicalDevice device, const VulkanSurface& surface) const 
    {
        // query queue family count
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        // retrieve queue family properties
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

        QueueFamilyIndices indices; 

        // find optimal queue families
        for (uint32_t index = 0; index < queueFamilyCount; index++)
        {
            const bool supportsGraphics = queueFamilyProperties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT;
            const bool supportsCompute  = queueFamilyProperties[index].queueFlags & VK_QUEUE_COMPUTE_BIT;
            const bool supportsTransfer = queueFamilyProperties[index].queueFlags & VK_QUEUE_TRANSFER_BIT;

            // graphics with present queue family 
            if (!indices.mGraphicsFamily && supportsGraphics && surface.canQueueFamilyPresent(device, index))
            {
                indices.mGraphicsFamily = index;
                indices.mPresentFamily = index;

                // share graphics queue for compute operations if dedicated is not requested
                if (!m_deviceConfig.mPreferDedicatedComputeQueue && supportsCompute)
                {
                    indices.mComputeFamily = index;
                }

                // share graphics queue for transfer operations if dedicated is not requested
                if (!m_deviceConfig.mPreferDedicatedTransferQueue && supportsTransfer)
                {
                    indices.mTransferFamily = index;
                }
            }

            // dedicated compute queue family
            if (!indices.mComputeFamily && m_deviceConfig.mPreferDedicatedComputeQueue 
                && supportsCompute && !supportsGraphics)
            {
                indices.mComputeFamily = index;
            }

            // dedicated transfer queue family
            if (!indices.mTransferFamily && m_deviceConfig.mPreferDedicatedTransferQueue &&
                supportsTransfer && !supportsGraphics && !supportsCompute)
            {
                indices.mTransferFamily = index;
            }

            // early return if all queues are found
            if (indices.mGraphicsFamily && indices.mPresentFamily && 
                indices.mComputeFamily && indices.mTransferFamily)
            {
                return indices;
            }
        }

        // fallback: find any available queue families regardless of preferences
        if (!indices.isComplete())
        {
            for (uint32_t index = 0; index < queueFamilyCount; index++)
            {
                const bool supportsGraphics = queueFamilyProperties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT;
                const bool supportsCompute  = queueFamilyProperties[index].queueFlags & VK_QUEUE_COMPUTE_BIT;
                const bool supportsTransfer = queueFamilyProperties[index].queueFlags & VK_QUEUE_TRANSFER_BIT;

                // graphics queue family 
                if (!indices.mGraphicsFamily && supportsGraphics)
                {
                    indices.mGraphicsFamily = index;
                }

                // present queue family 
                if (!indices.mPresentFamily && surface.canQueueFamilyPresent(device, index))
                {
                    indices.mPresentFamily = index;
                }

                // compute queue family 
                if (!indices.mComputeFamily && supportsCompute)
                {
                    indices.mComputeFamily = index;
                }

                // transfer queue family 
                if (!indices.mTransferFamily && supportsTransfer)
                {
                    indices.mTransferFamily = index;
                }
            }
        }

        return indices;
    }

    bool VulkanDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) const 
    {
        // vulkan spec permits logical device creation without any extensions
        if (m_deviceConfig.mDeviceExtensions.empty())
        {
            VK_LOG_WARN("no device extensions requested");
            return true;
        }

        // query the number of supported device extensions
        uint32_t extensionCount = 0;
        VkResult vkResult = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query device extension count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // retrieve supported device extensions.
        std::vector<VkExtensionProperties> extensionProperties(extensionCount);
        vkResult = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionProperties.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query device extension properties : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // check if all the requested extensions are present in supported extensions
        std::unordered_set<std::string_view> required(m_deviceConfig.mDeviceExtensions.begin(), m_deviceConfig.mDeviceExtensions.end());
        for (const auto& extension  : extensionProperties)
        {
            required.erase(extension.extensionName);
        }

        // log the requested extensions not supported by device
        for (const auto& ext : required) 
        {
            VK_LOG_WARN("unsupported device extension: %s", ext.data());
        }

        return required.empty();
    }

    uint64_t VulkanDevice::scoreDevice(const PhysicalDeviceInfo& physicalDeviceInfo) const
    {
        uint64_t score = 0;
        const auto& properties = physicalDeviceInfo.mVkPhysicalDeviceProperties;
        const auto& memoryProperties = physicalDeviceInfo.mVkPhysicalDeviceMemoryProperties;

        // device type preference
        switch (properties.deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                score += 1000;
                break;

            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                score += 200;
                break;

            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                score += 100;
                break;
        }

        // device local memory (1 point per 1GB)
        uint64_t bytesPerPoint = 1024 * 1024 * 1024;
        for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; i++)
        {
            if (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                score += static_cast<uint64_t>(memoryProperties.memoryHeaps[i].size / bytesPerPoint);
            }
        }

        // rendering quality and capacity
        const auto& limits = properties.limits;
        score += static_cast<uint64_t>(limits.maxImageDimension2D / 1024);
        score += static_cast<uint64_t>(limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_8_BIT ? 20 : 0);

        // shading and pipeline resources
        score += std::min<uint64_t>(limits.maxPerStageDescriptorSampledImages / 8, 32u);
        score += std::min<uint64_t>(limits.maxPerStageDescriptorUniformBuffers / 4, 32u);
        score += std::min<uint64_t>(limits.maxFragmentCombinedOutputResources / 4, 32u);
        score += std::min<uint64_t>(limits.maxPushConstantsSize / 32, 16u);
        score += std::min<uint64_t>(limits.maxBoundDescriptorSets * 2, 16u);

        return score;
    }

    void VulkanDevice::logPhysicalDeviceInfo(const PhysicalDeviceInfo& physicalDeviceInfo) const
    {
        const auto& properties = physicalDeviceInfo.mVkPhysicalDeviceProperties;
        const auto& indices = physicalDeviceInfo.mQueueFamilyIndices;

        // log details of the selected physical device.
        VK_LOG_INFO("─────────────── Selected Vulkan Physical Device ───────────────");
        VK_LOG_INFO("Device Name           : %s", properties.deviceName);
        VK_LOG_INFO("Device Type           : %s", string_VkPhysicalDeviceType(properties.deviceType));
        VK_LOG_INFO("API Version           : %u.%u.%u",
                    VK_VERSION_MAJOR(properties.apiVersion),
                    VK_VERSION_MINOR(properties.apiVersion),
                    VK_VERSION_PATCH(properties.apiVersion));
        VK_LOG_INFO("Driver Version        : %u", properties.driverVersion);
        VK_LOG_INFO("Vendor ID             : 0x%X", properties.vendorID);
        VK_LOG_INFO("Device ID             : 0x%X", properties.deviceID);
        VK_LOG_INFO("Score                 : %d", physicalDeviceInfo.mScore);
        VK_LOG_INFO("Graphics Queue Index  : %d", indices.mGraphicsFamily.value_or(-1));
        VK_LOG_INFO("Present Queue Index   : %d", indices.mPresentFamily.value_or(-1));
        VK_LOG_INFO("Compute Queue Index   : %d", indices.mComputeFamily.value_or(-1));
        VK_LOG_INFO("Transfer Queue Index  : %d", indices.mTransferFamily.value_or(-1));
        VK_LOG_INFO("───────────────────────────────────────────────────────────────");
    }

    void VulkanDevice::validateDeviceExtensions()
    {
        auto& extensions = m_deviceConfig.mDeviceExtensions;
        if (extensions.empty())
        {
            return;
        }

        std::unordered_set<std::string> seen;
        std::vector<const char*> sanitized;
        sanitized.reserve(extensions.size());
        for (const auto& extension : extensions)
        {
            // skip empty or null extension and duplicates 
            if (!extension || *extension == '\0' || !seen.insert(extension).second)
            {
                continue;
            }
            sanitized.emplace_back(extension);
        }
        extensions = std::move(sanitized);
    }

    void VulkanDevice::validateRequestedFeatures()
    {
        // array of pairs: pointer-to-member + feature name
        constexpr std::pair<VkBool32 VkPhysicalDeviceFeatures::*, const char*> featureMap[] = 
        {
            { &VkPhysicalDeviceFeatures::robustBufferAccess, "robustBufferAccess" },
            { &VkPhysicalDeviceFeatures::fullDrawIndexUint32, "fullDrawIndexUint32" },
            { &VkPhysicalDeviceFeatures::imageCubeArray, "imageCubeArray" },
            { &VkPhysicalDeviceFeatures::independentBlend, "independentBlend" },
            { &VkPhysicalDeviceFeatures::geometryShader, "geometryShader" },
            { &VkPhysicalDeviceFeatures::tessellationShader, "tessellationShader" },
            { &VkPhysicalDeviceFeatures::sampleRateShading, "sampleRateShading" },
            { &VkPhysicalDeviceFeatures::dualSrcBlend, "dualSrcBlend" },
            { &VkPhysicalDeviceFeatures::logicOp, "logicOp" },
            { &VkPhysicalDeviceFeatures::multiDrawIndirect, "multiDrawIndirect" },
            { &VkPhysicalDeviceFeatures::drawIndirectFirstInstance, "drawIndirectFirstInstance" },
            { &VkPhysicalDeviceFeatures::depthClamp, "depthClamp" },
            { &VkPhysicalDeviceFeatures::depthBiasClamp, "depthBiasClamp" },
            { &VkPhysicalDeviceFeatures::fillModeNonSolid, "fillModeNonSolid" },
            { &VkPhysicalDeviceFeatures::depthBounds, "depthBounds" },
            { &VkPhysicalDeviceFeatures::wideLines, "wideLines" },
            { &VkPhysicalDeviceFeatures::largePoints, "largePoints" },
            { &VkPhysicalDeviceFeatures::alphaToOne, "alphaToOne" },
            { &VkPhysicalDeviceFeatures::multiViewport, "multiViewport" },
            { &VkPhysicalDeviceFeatures::samplerAnisotropy, "samplerAnisotropy" },
            { &VkPhysicalDeviceFeatures::textureCompressionETC2, "textureCompressionETC2" },
            { &VkPhysicalDeviceFeatures::textureCompressionASTC_LDR, "textureCompressionASTC_LDR" },
            { &VkPhysicalDeviceFeatures::textureCompressionBC, "textureCompressionBC" },
            { &VkPhysicalDeviceFeatures::occlusionQueryPrecise, "occlusionQueryPrecise" },
            { &VkPhysicalDeviceFeatures::pipelineStatisticsQuery, "pipelineStatisticsQuery" },
            { &VkPhysicalDeviceFeatures::vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics" },
            { &VkPhysicalDeviceFeatures::fragmentStoresAndAtomics, "fragmentStoresAndAtomics" },
            { &VkPhysicalDeviceFeatures::shaderTessellationAndGeometryPointSize, "shaderTessellationAndGeometryPointSize" },
            { &VkPhysicalDeviceFeatures::shaderImageGatherExtended, "shaderImageGatherExtended" },
            { &VkPhysicalDeviceFeatures::shaderStorageImageExtendedFormats, "shaderStorageImageExtendedFormats" },
            { &VkPhysicalDeviceFeatures::shaderStorageImageMultisample, "shaderStorageImageMultisample" },
            { &VkPhysicalDeviceFeatures::shaderStorageImageReadWithoutFormat, "shaderStorageImageReadWithoutFormat" },
            { &VkPhysicalDeviceFeatures::shaderStorageImageWriteWithoutFormat, "shaderStorageImageWriteWithoutFormat" },
            { &VkPhysicalDeviceFeatures::shaderUniformBufferArrayDynamicIndexing, "shaderUniformBufferArrayDynamicIndexing" },
            { &VkPhysicalDeviceFeatures::shaderSampledImageArrayDynamicIndexing, "shaderSampledImageArrayDynamicIndexing" },
            { &VkPhysicalDeviceFeatures::shaderStorageBufferArrayDynamicIndexing, "shaderStorageBufferArrayDynamicIndexing" },
            { &VkPhysicalDeviceFeatures::shaderStorageImageArrayDynamicIndexing, "shaderStorageImageArrayDynamicIndexing" },
            { &VkPhysicalDeviceFeatures::shaderClipDistance, "shaderClipDistance" },
            { &VkPhysicalDeviceFeatures::shaderCullDistance, "shaderCullDistance" },
            { &VkPhysicalDeviceFeatures::shaderFloat64, "shaderFloat64" },
            { &VkPhysicalDeviceFeatures::shaderInt64, "shaderInt64" },
            { &VkPhysicalDeviceFeatures::shaderInt16, "shaderInt16" },
            { &VkPhysicalDeviceFeatures::shaderResourceResidency, "shaderResourceResidency" },
            { &VkPhysicalDeviceFeatures::shaderResourceMinLod, "shaderResourceMinLod" },
            { &VkPhysicalDeviceFeatures::sparseBinding, "sparseBinding" },
            { &VkPhysicalDeviceFeatures::sparseResidencyBuffer, "sparseResidencyBuffer" },
            { &VkPhysicalDeviceFeatures::sparseResidencyImage2D, "sparseResidencyImage2D" },
            { &VkPhysicalDeviceFeatures::sparseResidencyImage3D, "sparseResidencyImage3D" },
            { &VkPhysicalDeviceFeatures::sparseResidency2Samples, "sparseResidency2Samples" },
            { &VkPhysicalDeviceFeatures::sparseResidency4Samples, "sparseResidency4Samples" },
            { &VkPhysicalDeviceFeatures::sparseResidency8Samples, "sparseResidency8Samples" },
            { &VkPhysicalDeviceFeatures::sparseResidency16Samples, "sparseResidency16Samples" },
            { &VkPhysicalDeviceFeatures::sparseResidencyAliased, "sparseResidencyAliased" },
            { &VkPhysicalDeviceFeatures::variableMultisampleRate, "variableMultisampleRate" },
            { &VkPhysicalDeviceFeatures::inheritedQueries, "inheritedQueries" }
        };

        // sanitize the requested features
        const auto& supportedFeatures = m_vkPhysicalDeviceFeatures;
        auto& requestedFeatures = m_deviceConfig.mRequestedFeatures;

        for (const auto& [memberPtr, name] : featureMap)
        {
            VkBool32& requested = requestedFeatures.*memberPtr;
            VkBool32 supported = supportedFeatures.*memberPtr;
            if (requested && !supported)
            {
                VK_LOG_WARN("requested feature '%s' is not supported", name);
                requested = VK_FALSE;
            }
        }
    }
}   // namespace keplar
