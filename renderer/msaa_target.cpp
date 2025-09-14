// ────────────────────────────────────────────
//  File: msaa_target.cpp · Created by Yash Patel · 9-12-2025
// ────────────────────────────────────────────

#include "msaa_target.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    MsaaTarget::MsaaTarget() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_colorImage(VK_NULL_HANDLE)
        , m_colorMemory(VK_NULL_HANDLE)
        , m_colorImageView(VK_NULL_HANDLE)
        , m_depthImage(VK_NULL_HANDLE)
        , m_depthMemory(VK_NULL_HANDLE)
        , m_depthImageView(VK_NULL_HANDLE)
        , m_extent{}
        , m_sampleCount(VK_SAMPLE_COUNT_1_BIT)
    {
    }

    MsaaTarget::~MsaaTarget()
    {
        destroy();
    }

    MsaaTarget::MsaaTarget(MsaaTarget&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_colorImage(other.m_colorImage)
        , m_colorMemory(other.m_colorMemory)
        , m_colorImageView(other.m_colorImageView)
        , m_depthImage(other.m_depthImage)
        , m_depthMemory(other.m_depthMemory)
        , m_depthImageView(other.m_depthImageView)
        , m_extent(other.m_extent)
        , m_sampleCount(other.m_sampleCount)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_colorImage = VK_NULL_HANDLE;
        other.m_colorMemory = VK_NULL_HANDLE;
        other.m_colorImageView = VK_NULL_HANDLE;
        other.m_depthImage = VK_NULL_HANDLE;
        other.m_depthMemory = VK_NULL_HANDLE;
        other.m_depthImageView = VK_NULL_HANDLE;
    }

    MsaaTarget& MsaaTarget::operator=(MsaaTarget&& other) noexcept
    {
        // avoid self-move
        if (this != &other)
        {
            // release current resources
            destroy();

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_colorImage = other.m_colorImage;
            m_colorMemory = other.m_colorMemory;
            m_colorImageView = other.m_colorImageView;
            m_depthImage = other.m_depthImage;
            m_depthMemory = other.m_depthMemory;
            m_depthImageView = other.m_depthImageView;
            m_extent = other.m_extent;
            m_sampleCount = other.m_sampleCount;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_colorImage = VK_NULL_HANDLE;
            other.m_colorMemory = VK_NULL_HANDLE;
            other.m_colorImageView = VK_NULL_HANDLE;
            other.m_depthImage = VK_NULL_HANDLE;
            other.m_depthMemory = VK_NULL_HANDLE;
            other.m_depthImageView = VK_NULL_HANDLE;
        }

        return *this;
    }

    bool MsaaTarget::initialize(const VulkanDevice& device, const VulkanSwapchain& swapchain, VkSampleCountFlagBits desiredSampleCount) noexcept
    {
        // save device handle for cleanup
        m_vkDevice = device.getDevice();
        m_extent   = swapchain.getExtent();

        // retrieve device properties once
        const auto deviceProperties = device.getPhysicalDeviceProperties();

        // choose msaa sample count
        if (!chooseSampleCount(deviceProperties, desiredSampleCount))
        {
            VK_LOG_ERROR("MsaaTarget::initialize - no suitable sample count available");
            return false;
        }

        // create color target
        if (!createColorTarget(device, swapchain))
        {
            VK_LOG_ERROR("MsaaTarget::initialize - failed to create msaa color target");
            return false;
        }

        // create depth-stencil target
        if (!createDepthTarget(device, swapchain))
        {
            VK_LOG_ERROR("MsaaTarget::initialize - failed to create msaa depth-stencil target");
            return false;
        }

        // success
        VK_LOG_DEBUG("MsaaTarget created successfully (sample count: %d)", m_sampleCount);
        return true;
    }

    void MsaaTarget::destroy() noexcept
    {
        // destroy depth image view
        if (m_depthImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_vkDevice, m_depthImageView, nullptr);
            m_depthImageView = VK_NULL_HANDLE;
        }

        // destroy depth image
        if (m_depthImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(m_vkDevice, m_depthImage, nullptr);
            m_depthImage = VK_NULL_HANDLE;
        }

        // free depth memory
        if (m_depthMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_vkDevice, m_depthMemory, nullptr);
            m_depthMemory = VK_NULL_HANDLE;
        }

        // destroy color image view
        if (m_colorImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_vkDevice, m_colorImageView, nullptr);
            m_colorImageView = VK_NULL_HANDLE;
        }

        // destroy color image
        if (m_colorImage!= VK_NULL_HANDLE)
        {
            vkDestroyImage(m_vkDevice, m_colorImage, nullptr);
            m_colorImage = VK_NULL_HANDLE;
        }

        // free color memory
        if (m_colorMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_vkDevice, m_colorMemory, nullptr);
            m_colorMemory = VK_NULL_HANDLE;
        }
    }

    bool MsaaTarget::chooseSampleCount(const VkPhysicalDeviceProperties& deviceProperties, VkSampleCountFlagBits desiredSampleCount) noexcept
    {
        // supported counts for color and depth
        VkSampleCountFlags colorCounts = deviceProperties.limits.framebufferColorSampleCounts;
        VkSampleCountFlags depthCounts = deviceProperties.limits.framebufferDepthSampleCounts;
        VkSampleCountFlags supportedCounts = colorCounts & depthCounts;

        // candidate msaa levels
        const VkSampleCountFlagBits candidates[] = 
        {
            VK_SAMPLE_COUNT_64_BIT,
            VK_SAMPLE_COUNT_32_BIT,
            VK_SAMPLE_COUNT_16_BIT,
            VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_4_BIT,
            VK_SAMPLE_COUNT_2_BIT
        };

        // pick the highest supported sample count <= desired
        for (auto count : candidates) 
        {
            if (count <= desiredSampleCount && (supportedCounts & count)) 
            {
                // success
                m_sampleCount = count;
                return true;
            }
        }

        // failure
        VK_LOG_WARN("msaa is not supported beyond sample count 1");
        return false;
    }

    bool MsaaTarget::createColorTarget(const VulkanDevice& device, const VulkanSwapchain& swapchain) noexcept
    {
        // get swapchain color format
        const auto colorFormat = swapchain.getColorFormat();

        // msaa color image info
        VkImageCreateInfo colorImageInfo{};
        colorImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        colorImageInfo.pNext = nullptr;
        colorImageInfo.flags = 0;
        colorImageInfo.imageType = VK_IMAGE_TYPE_2D;
        colorImageInfo.format = colorFormat;
        colorImageInfo.extent.width = m_extent.width;
        colorImageInfo.extent.height = m_extent.height;
        colorImageInfo.extent.depth = 1;
        colorImageInfo.mipLevels = 1;
        colorImageInfo.arrayLayers = 1;
        colorImageInfo.samples = m_sampleCount;
        colorImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        colorImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        colorImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        colorImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // create msaa color image
        VkResult vkResult = vkCreateImage(m_vkDevice, &colorImageInfo, nullptr, &m_colorImage);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateImage failed to create msaa color image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // get image memory properties
        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_vkDevice, m_colorImage, &memoryRequirements);

        // find suitable memory type
        auto memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
        if (!memoryTypeIndex.has_value())
        {
            memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }
        if (!memoryTypeIndex.has_value())
        {
            VK_LOG_FATAL("findMemoryType failed to find suitable memory type for msaa color image");
            return false;
        }

        // memory allocation info
        VkMemoryAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocationInfo.pNext = nullptr;
        allocationInfo.allocationSize = memoryRequirements.size;
        allocationInfo.memoryTypeIndex = *memoryTypeIndex;

        // allocate device memory
        vkResult = vkAllocateMemory(m_vkDevice, &allocationInfo, nullptr, &m_colorMemory);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateMemory failed to allocate memory for msaa color image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // bind msaa color image to device memory
        vkResult = vkBindImageMemory(m_vkDevice, m_colorImage, m_colorMemory, 0);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkBindImageMemory failed to bind msaa color image to device memory : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // msaa color image view info
        VkImageViewCreateInfo colorImageViewInfo{};
        colorImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorImageViewInfo.pNext = nullptr;
        colorImageViewInfo.flags = 0;
        colorImageViewInfo.image = m_colorImage;
        colorImageViewInfo.format = colorFormat;
        colorImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        colorImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        colorImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        colorImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        colorImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageViewInfo.subresourceRange.baseMipLevel = 0;
        colorImageViewInfo.subresourceRange.levelCount = 1;
        colorImageViewInfo.subresourceRange.baseArrayLayer = 0;
        colorImageViewInfo.subresourceRange.layerCount = 1;

        // create msaa color image view
        vkResult = vkCreateImageView(m_vkDevice, &colorImageViewInfo, nullptr, &m_colorImageView);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateImageView failed to create view for msaa color image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // success
        return true;
    }

    bool MsaaTarget::createDepthTarget(const VulkanDevice& device, const VulkanSwapchain& swapchain) noexcept
    {
        // get swapchain depth format
        const auto depthFormat = swapchain.getDepthFormat();

        // msaa depth image info
        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.pNext = nullptr;
        depthImageInfo.flags = 0;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = depthFormat;
        depthImageInfo.extent.width = m_extent.width;
        depthImageInfo.extent.height = m_extent.height;
        depthImageInfo.extent.depth = 1;
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.samples = m_sampleCount;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        // create msaa depth image
        VkResult vkResult = vkCreateImage(m_vkDevice, &depthImageInfo, nullptr, &m_depthImage);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateImage failed to create msaa depth image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // get image memory properties
        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_vkDevice, m_depthImage, &memoryRequirements);

        // find suitable memory type
        auto memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
        if (!memoryTypeIndex.has_value())
        {
            memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }
        if (!memoryTypeIndex.has_value())
        {
            VK_LOG_FATAL("findMemoryType failed to find suitable memory type for msaa depth image");
            return false;
        }

        // memory allocation info
        VkMemoryAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocationInfo.pNext = nullptr;
        allocationInfo.allocationSize = memoryRequirements.size;
        allocationInfo.memoryTypeIndex = *memoryTypeIndex;

        // allocate device memory
        vkResult = vkAllocateMemory(m_vkDevice, &allocationInfo, nullptr, &m_depthMemory);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateMemory failed to allocate memory for msaa depth image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // bind msaa depth image to device memory
        vkResult = vkBindImageMemory(m_vkDevice, m_depthImage, m_depthMemory, 0);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkBindImageMemory failed to bind msaa depth image to device memory : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // msaa depth image view info
        VkImageViewCreateInfo depthImageViewInfo{};
        depthImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthImageViewInfo.pNext = nullptr;
        depthImageViewInfo.flags = 0;
        depthImageViewInfo.image = m_depthImage;
        depthImageViewInfo.format = depthFormat;
        depthImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		depthImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		depthImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		depthImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        depthImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthImageViewInfo.subresourceRange.baseMipLevel = 0;
        depthImageViewInfo.subresourceRange.levelCount = 1;
        depthImageViewInfo.subresourceRange.baseArrayLayer = 0;
        depthImageViewInfo.subresourceRange.layerCount = 1;

        // add stencil aspect for depth formats that include a stencil component
        if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            depthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
            depthFormat == VK_FORMAT_D16_UNORM_S8_UINT)
        {
            depthImageViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        // create msaa depth image view
        vkResult = vkCreateImageView(m_vkDevice, &depthImageViewInfo, nullptr, &m_depthImageView);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateImageView failed to create view for msaa depth image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // success
        return true;
    }
}   // namespace keplar

