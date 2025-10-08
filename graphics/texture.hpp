// ────────────────────────────────────────────
//  File: texture.hpp · Created by Yash Patel · 9-14-2025
// ────────────────────────────────────────────

#pragma once

#include <string>

#include "asset_io.hpp"
#include "vulkan/vulkan_config.hpp"

namespace keplar
{
    // forward declarations
    class VulkanDevice;
    class VulkanCommandPool;
    struct ImageData;

    class Texture
    {
        public:
            // creation and destruction
            Texture() noexcept;
            ~Texture();

            // disable copy semantics to enforce unique ownership
            Texture(const Texture&) = delete;
            Texture& operator=(const Texture&) = delete;

            // move semantics
            Texture(Texture&&) noexcept;
            Texture& operator=(Texture&&) noexcept;

            // usage
            bool load(const VulkanDevice& device, const VulkanCommandPool& commandPool, const std::string& filepath, bool flipY = false, bool genMips = true) noexcept;
            bool load(const VulkanDevice& device, const VulkanCommandPool& commandPool, const tinygltf::Image& gltfImage, bool genMips = true) noexcept;
            void destroy() noexcept;

            // accessors
            VkImage     getImage() const noexcept       { return m_vkImage; }
            VkImageView getImageView() const noexcept   { return m_vkImageView; }
            uint32_t    getMipLevels() const noexcept   { return m_mipLevels; }
            uint32_t    getWidth() const noexcept       { return m_width; }
            uint32_t    getHeight() const noexcept      { return m_height; }
            uint32_t    getChannels() const noexcept    { return m_channels; }

        private:
            bool loadImageData(const std::string& filepath, ImageData& imageData, bool flipY = true) noexcept;
            bool createImage(const VulkanDevice& device, const VulkanCommandPool& commandPool, const ImageData& imageData, bool genMips) noexcept;
            bool createImageView() noexcept;
            void generateMipmaps(VkCommandBuffer commandBuffer) noexcept;
            void transitionImageLayout(VkCommandBuffer commandBuffer, 
                    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, 
                    VkImageLayout oldLayout, VkImageLayout newLayout, 
                    VkAccessFlags srcAccess, VkAccessFlags dstAccess, 
                    uint32_t baseMipLevel, uint32_t levelCount) noexcept;

        private:
            // vulkan handles
            VkDevice        m_vkDevice;
            VkImage         m_vkImage;
            VkDeviceMemory  m_vkImageMemory;
            VkImageView     m_vkImageView;

            // image data
            uint32_t m_width;
            uint32_t m_height;
            uint32_t m_channels;
            uint32_t m_mipLevels;
            VkFormat m_format;
    };  
}   // namespace keplar

