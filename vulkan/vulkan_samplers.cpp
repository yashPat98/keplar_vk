// ────────────────────────────────────────────
//  File: vulkan_samplers.cpp · Created by Yash Patel · 10-04-2025
// ────────────────────────────────────────────

#include "vulkan_samplers.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanSamplers::VulkanSamplers() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_samplers{}
    {
    }

    VulkanSamplers::~VulkanSamplers()
    {
        destroy();
    }

    VulkanSamplers::VulkanSamplers(VulkanSamplers&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_samplers(other.m_samplers)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_samplers.fill(VK_NULL_HANDLE);
    }

    VulkanSamplers& VulkanSamplers::operator=(VulkanSamplers&& other) noexcept
    {
        if (this != &other)
        {
            // release current resources
            destroy();

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_samplers = other.m_samplers;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            m_samplers.fill(VK_NULL_HANDLE);
        }
        
        return *this;
    }

    bool VulkanSamplers::initialize(const VulkanDevice& device) noexcept
    {
        // store device handle
        m_vkDevice = device.getDevice();

        // query max supported anisotropy 
        const auto deviceProperties = device.getPhysicalDeviceProperties();
        const auto enabledFeatures = device.getEnabledFeatures();
        bool enableAnisotropy = enabledFeatures.samplerAnisotropy;
        float maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;

        // sampler config 
        struct SamplerConfig
        {
            VkFilter magFilter;
            VkFilter minFilter;
            VkSamplerAddressMode addressMode;
            bool requestAnisotropy;
        };

        // predefined sampler configurations for each Type.
        constexpr SamplerConfig samplerConfigs[static_cast<size_t>(Type::Count)] = 
        {
            { VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false },   // NearestClamp
            { VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false },   // LinearClamp
            { VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT,        false },   // NearestRepeat
            { VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT,        false },   // LinearRepeat
            { VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT,        true  }    // AnisotropicRepeat
        };

        for (size_t i = 0; i < static_cast<size_t>(Type::Count); i++)
        {
            const auto& config = samplerConfigs[i];

            // skip if anisotropy feature is not enabled
            if (config.requestAnisotropy && !enableAnisotropy)
            {
                VK_LOG_WARN("sampler creation skipped : anisotropy is not enabled for the vulkan device");
                continue;
            }

            // sampler creation info structure
            VkSamplerCreateInfo samplerCreateInfo{};
            samplerCreateInfo.sType                     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.pNext                     = nullptr;
            samplerCreateInfo.flags                     = 0;
            samplerCreateInfo.magFilter                 = config.magFilter;
            samplerCreateInfo.minFilter                 = config.minFilter;
            samplerCreateInfo.mipmapMode                = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCreateInfo.addressModeU              = config.addressMode;
            samplerCreateInfo.addressModeV              = config.addressMode;
            samplerCreateInfo.addressModeW              = config.addressMode;
            samplerCreateInfo.mipLodBias                = 0.0f;
            samplerCreateInfo.anisotropyEnable          = (config.requestAnisotropy && enableAnisotropy) ? VK_TRUE : VK_FALSE;
            samplerCreateInfo.maxAnisotropy             = (config.requestAnisotropy && enableAnisotropy) ? maxAnisotropy : 1.0f;
            samplerCreateInfo.compareEnable             = VK_FALSE;
            samplerCreateInfo.compareOp                 = VK_COMPARE_OP_ALWAYS;
            samplerCreateInfo.minLod                    = 0.0f;
            samplerCreateInfo.maxLod                    = VK_LOD_CLAMP_NONE;
            samplerCreateInfo.borderColor               = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerCreateInfo.unnormalizedCoordinates   = VK_FALSE;

            // create vulkan sampler object
            VkResult vkResult = vkCreateSampler(m_vkDevice, &samplerCreateInfo, nullptr, &m_samplers[i]);
            if (vkResult != VK_SUCCESS)
            {
                VK_LOG_FATAL("vkCreateSampler failed to create sampler for type %d : %s (code: %d)", i, string_VkResult(vkResult), vkResult);
                return false;
            }
        }

        VK_LOG_DEBUG("texture samplers created successfully");
        return true;
    }

    void VulkanSamplers::destroy() noexcept
    {
        // destroy all samplers
        for (auto& sampler : m_samplers)
        {
            if (sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(m_vkDevice, sampler, nullptr);
                sampler = VK_NULL_HANDLE;
            }
        }
    }
}   // namespace keplar
