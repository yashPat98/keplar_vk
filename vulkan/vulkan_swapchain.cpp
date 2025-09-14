// ────────────────────────────────────────────
//  File: vulkan_swapchain.cpp · Created by Yash Patel · 7-11-2025
// ────────────────────────────────────────────

#include "vulkan_swapchain.hpp"

#include <algorithm>

#include "vulkan_context.hpp"
#include "vulkan_surface.hpp"
#include "vulkan_device.hpp"
#include "vulkan_utils.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanSwapchain::VulkanSwapchain(const VulkanContext& context) noexcept
        : m_device(context.getDevice())
        , m_surface(context.getSurface())
        , m_vkDevice(m_device.getDevice())
        , m_vkSwapchainKHR(VK_NULL_HANDLE)
        , m_vkSurfaceFormatKHR{}
        , m_imageCount(0)
        , m_imageExtent{}
        , m_preTransform(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        , m_depthImage(VK_NULL_HANDLE)
        , m_depthMemory(VK_NULL_HANDLE)
        , m_depthImageView(VK_NULL_HANDLE)
        , m_depthFormat(VK_FORMAT_UNDEFINED)
    {
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        destroy();
    }

    bool VulkanSwapchain::initialize(uint32_t width, uint32_t height) noexcept
    {
        // validate window dimensions
        if (width == 0 || height == 0)
        {
            VK_LOG_WARN("initialize :: window extent is invalid (%d x %d)", width, height);
            return false;
        }

        // select surface format & presentation mode
        if (!chooseSurfaceFormat())                 { return false; }
        if (!choosePresentMode())                   { return false; }
        
        // query surface capabilities and configure swapchain
        const auto& surfaceCapabilities = m_surface.getCapabilities(m_device.getPhysicalDevice());
        chooseImageCount(surfaceCapabilities);
        chooseSwapExtent(surfaceCapabilities, { width, height });
        choosePreTransform(surfaceCapabilities);

        // create swapchain and its attachments
        if (!createSwapchain(m_vkSwapchainKHR))     { return false; }
        if (!createColorAttachment())               { return false; }
        if (!createDepthAttachment())               { return false; }

        VK_LOG_DEBUG("initialize :: swapchain created successfully.");
        return true;
    }

    bool VulkanSwapchain::recreate(uint32_t width, uint32_t height) noexcept
    {
        // teardown previous swapchain images/views
        destroyAttachments();

        // initialize swapchain & attachments (old swapchain destroyed internally)
        if (!initialize(width, height))               
        { 
            VK_LOG_FATAL("recreate :: swapchain re-creation failed.");
            return false; 
        }

        VK_LOG_DEBUG("recreate :: swapchain created successfully.");
        return true;
    }

    void VulkanSwapchain::destroy() noexcept
    {
        // destror color and depth-stencil resources
        destroyAttachments();

        if (m_vkSwapchainKHR != VK_NULL_HANDLE)
        {
            // swapchain images are destroyed when swapchain is destroyed
            vkDestroySwapchainKHR(m_vkDevice, m_vkSwapchainKHR, nullptr);
            m_vkSwapchainKHR = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_DEBUG("swapchain destroyed successfully");
        }
    }

    bool VulkanSwapchain::chooseSurfaceFormat() noexcept
    {
        // get supported surface formats
        const auto& surfaceFormats = m_surface.getSupportedFormats(m_device.getPhysicalDevice());
        if (surfaceFormats.empty())
        {
            VK_LOG_FATAL("chooseSurfaceFormat :: no supported surface formats found for this device.");
            return false;
        }

        // if the surface has no preferred format choose the best format
        if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
        {
            m_vkSurfaceFormatKHR.format = VK_FORMAT_B8G8R8A8_UNORM;
            m_vkSurfaceFormatKHR.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            return true;
        }

        // find the preferred surface format
        for (const auto& availableFormat : surfaceFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && 
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                m_vkSurfaceFormatKHR = availableFormat;
                return true;
            }
        }

        // fallback to first available format
        m_vkSurfaceFormatKHR = surfaceFormats[0];
        return true;
    }

    bool VulkanSwapchain::choosePresentMode() noexcept
    {
        // get supported present modes
        const auto& presentModes = m_surface.getSupportedPresentModes(m_device.getPhysicalDevice());
        if (presentModes.empty())
        {
            VK_LOG_FATAL("choosePresentMode :: no supported present modes found for this device.");
            return false;
        }

        // prefer VK_PRESENT_MODE_MAILBOX_KHR if available 
        for (const auto& presentMode : presentModes)
        {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                // mailbox mode provides low latency and avoids tearing 
                m_vkPresentModeKHR = presentMode;
                return true;
            }
        }

        // fallback: guaranteed to be supported on all vulkan implementations, enables vsync
        m_vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
        return true;
    }

    void VulkanSwapchain::chooseImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) noexcept
    {
        // start with one more than the minimum image count to enable triple buffering
        uint32_t desiredImageCount = surfaceCapabilities.minImageCount + 1;

        // clamp to the maximum supported image count
        if (surfaceCapabilities.maxImageCount > 0 && desiredImageCount > surfaceCapabilities.maxImageCount)
        {
            desiredImageCount = surfaceCapabilities.maxImageCount;
        }

        m_imageCount = desiredImageCount;
    }

    void VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities, VkExtent2D windowExtent) noexcept
    {
        if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
        {
            // surface size is already defined by the window system
            m_imageExtent = surfaceCapabilities.currentExtent;
        }
        else 
        {
            // surface size is not defined - choose based on platform 
            m_imageExtent.width = std::clamp(windowExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
            m_imageExtent.height = std::clamp(windowExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
        }
    }

    void VulkanSwapchain::choosePreTransform(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) noexcept
    {
        if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        {
            m_preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        }
        else 
        {
            m_preTransform = surfaceCapabilities.currentTransform;
        }
    }

    bool VulkanSwapchain::createSwapchain(VkSwapchainKHR oldSwapchain) noexcept
    {
        // setup swapchain create info
        VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR{};
        vkSwapchainCreateInfoKHR.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        vkSwapchainCreateInfoKHR.pNext = nullptr;
        vkSwapchainCreateInfoKHR.flags = 0;
        vkSwapchainCreateInfoKHR.surface = m_surface.get();
        vkSwapchainCreateInfoKHR.minImageCount = m_imageCount;
        vkSwapchainCreateInfoKHR.imageFormat = m_vkSurfaceFormatKHR.format;
        vkSwapchainCreateInfoKHR.imageColorSpace = m_vkSurfaceFormatKHR.colorSpace;
        vkSwapchainCreateInfoKHR.imageExtent = m_imageExtent;
        vkSwapchainCreateInfoKHR.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        vkSwapchainCreateInfoKHR.preTransform = m_preTransform;
        vkSwapchainCreateInfoKHR.imageArrayLayers = 1;
        vkSwapchainCreateInfoKHR.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        vkSwapchainCreateInfoKHR.presentMode = m_vkPresentModeKHR;
        vkSwapchainCreateInfoKHR.clipped = VK_TRUE;
        vkSwapchainCreateInfoKHR.oldSwapchain = oldSwapchain;

        // log swapchain creation info
        VK_LOG_DEBUG("swapchain creation info :: image count: %d, extent: %d x %d", m_imageCount, m_imageExtent.width, m_imageExtent.height);

        // configure sharing mode:
        QueueFamilyIndices indices = m_device.getQueueFamilyIndices();
        if (indices.mGraphicsFamily.value() != indices.mPresentFamily.value())
        {
            uint32_t queueFamilyIndices[2] = { *indices.mGraphicsFamily, *indices.mPresentFamily };
            vkSwapchainCreateInfoKHR.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            vkSwapchainCreateInfoKHR.queueFamilyIndexCount = 2;
            vkSwapchainCreateInfoKHR.pQueueFamilyIndices = queueFamilyIndices;
            VK_LOG_DEBUG("swapchain creation info :: image sharing mode is VK_SHARING_MODE_CONCURRENT");
        }
        else 
        {
            vkSwapchainCreateInfoKHR.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkSwapchainCreateInfoKHR.queueFamilyIndexCount = 0;
            vkSwapchainCreateInfoKHR.pQueueFamilyIndices = nullptr;
            VK_LOG_DEBUG("swapchain creation info :: image sharing mode is VK_SHARING_MODE_EXCLUSIVE");
        }

        // create swapchain 
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkResult vkResult = vkCreateSwapchainKHR(m_vkDevice, &vkSwapchainCreateInfoKHR, nullptr, &swapchain);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateSwapchainKHR failed to create swapchain : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // destroy old swapchain
        if (oldSwapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_vkDevice, oldSwapchain, nullptr);
            VK_LOG_DEBUG("old swapchain destroyed successfully");
        }

        m_vkSwapchainKHR = swapchain;
        VK_LOG_DEBUG("swapchain created successfully");
        return true;
    }

    bool VulkanSwapchain::createColorAttachment() noexcept
    {
        // query swapchain image count
        uint32_t swapchainImageCount = 0;
        VkResult vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchainKHR, &swapchainImageCount, nullptr);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("failed to query swapchain image count : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // abort if no swapchain image count returned 
        if (!swapchainImageCount)
        {
            VK_LOG_FATAL("vkGetSwapchainImagesKHR returned 0 swapchain image count");
            return false;
        }

        // retrieve the swapchain images
        m_colorImages.resize(swapchainImageCount, VK_NULL_HANDLE);
        vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchainKHR, &swapchainImageCount, m_colorImages.data());
        if (vkResult != VK_SUCCESS && vkResult != VK_INCOMPLETE)
        {
            VK_LOG_FATAL("failed to retrieve swapchain images : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // If VK_INCOMPLETE, driver returned fewer than requested
        if (vkResult == VK_INCOMPLETE)
        {
            m_colorImages.resize(swapchainImageCount);
            VK_LOG_WARN("vkGetSwapchainImagesKHR returned VK_INCOMPLETE; resized swapchain images to %u.", swapchainImageCount);
        }

        m_colorImageViews.resize(swapchainImageCount, VK_NULL_HANDLE);

        // create image views 
        VkImageViewCreateInfo vkImageViewCreateInfo{};
        vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vkImageViewCreateInfo.pNext = nullptr;
        vkImageViewCreateInfo.flags = 0;
        vkImageViewCreateInfo.format = m_vkSurfaceFormatKHR.format;
        vkImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        vkImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        vkImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        vkImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        vkImageViewCreateInfo.subresourceRange.levelCount = 1;
        vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        vkImageViewCreateInfo.subresourceRange.layerCount = 1;
        vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        // create image views for all retrieved color images 
        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            vkImageViewCreateInfo.image = m_colorImages[i];
            if (!VK_CHECK(vkCreateImageView(m_vkDevice, &vkImageViewCreateInfo, nullptr, &m_colorImageViews[i])))
            {
                return false;
            }
        }

        // update swapchain image count
        m_imageCount = swapchainImageCount;
        VK_LOG_DEBUG("swapchain color image views are created successfully : %d", m_imageCount);
        return true;
    }

    bool VulkanSwapchain::createDepthAttachment() noexcept
    {
        // depth format candidate from best to worst
        VkFormat depthFormats[] = 
        {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM,
        };

        // choose depth format  
        for (auto format : depthFormats)
        {
            VkFormatProperties formatProperties{};
            vkGetPhysicalDeviceFormatProperties(m_device.getPhysicalDevice(), format, &formatProperties);

            // check for depth stencil support for the format
            if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                m_depthFormat = format;
                break;
            }
        }

        // check if a valid depth format is supported
        if (m_depthFormat == VK_FORMAT_UNDEFINED)
        {
            VK_LOG_FATAL("failed to find suitable depth format");
            return false;
        }

        // initialize image create info for depth image
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext = nullptr;
        imageCreateInfo.flags = 0;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = m_depthFormat;
        imageCreateInfo.extent.width = m_imageExtent.width;
        imageCreateInfo.extent.height = m_imageExtent.height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        // create image for depth
        VkResult vkResult = vkCreateImage(m_vkDevice, &imageCreateInfo, nullptr, &m_depthImage);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateImage failed to create depth image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // memory requirements for depth image to get alignement and region size
        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_vkDevice, m_depthImage, &memoryRequirements);

        // find suitable memory type
        auto memoryTypeIndex = m_device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!memoryTypeIndex.has_value())
        {
            VK_LOG_ERROR("findMemoryType failed to find suitable memory type for depth image");
            vkDestroyImage(m_vkDevice, m_depthImage, nullptr);
            return false;
        }

        // initialize memory allocation info
        VkMemoryAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocationInfo.pNext = nullptr;
        allocationInfo.allocationSize = memoryRequirements.size;
        allocationInfo.memoryTypeIndex = *memoryTypeIndex;

        // allocate memory for depth image from vulkan heap
        vkResult = vkAllocateMemory(m_vkDevice, &allocationInfo, nullptr, &m_depthMemory);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateMemory failed to allocate memory for depth image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkDestroyImage(m_vkDevice, m_depthImage, nullptr);
            return false;
        }

        // bind depth image to device memory
        vkResult = vkBindImageMemory(m_vkDevice, m_depthImage, m_depthMemory, 0);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkBindImageMemory failed to bind depth image to device memory : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, m_depthMemory, nullptr);
            vkDestroyImage(m_vkDevice, m_depthImage, nullptr);
            return false;
        }

        // initialize image view create info for depth image
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.format = m_depthFormat;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.image = m_depthImage;

        // add stencil aspect for depth formats that include a stencil component
        if (m_depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            m_depthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
            m_depthFormat == VK_FORMAT_D16_UNORM_S8_UINT)
        {
            imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        // create depth image view
        vkResult = vkCreateImageView(m_vkDevice, &imageViewCreateInfo, nullptr, &m_depthImageView);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateImageView failed to create view for depth image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, m_depthMemory, nullptr);
            vkDestroyImage(m_vkDevice, m_depthImage, nullptr);
            return false;
        }

        VK_LOG_DEBUG("swapchain depth image and view are created successfully");
        return true;
    }

    void VulkanSwapchain::destroyAttachments() noexcept
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
        for (size_t i = 0; i < m_imageCount; ++i)
        {
            if (m_colorImageViews[i] != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_vkDevice, m_colorImageViews[i], nullptr);
                m_colorImageViews[i] = VK_NULL_HANDLE;
            }
        }

        // clear vectors
        m_colorImages.clear();
        m_colorImageViews.clear();
    }
}   // namespace keplar
