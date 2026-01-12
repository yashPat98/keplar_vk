// ────────────────────────────────────────────
//  File: texture.cpp · Created by Yash Patel · 9-14-2025
// ────────────────────────────────────────────

#include "texture.hpp"
#include <algorithm>

#include "core/keplar_config.hpp"
#include "vulkan/vulkan_device.hpp"
#include "vulkan/vulkan_command_pool.hpp"
#include "vulkan/vulkan_command_buffer.hpp"
#include "vulkan/vulkan_fence.hpp"
#include "utils/logger.hpp"

namespace
{
    // cross-platform safe fopen wrapper
    FILE* fopenSafe(const std::string& filepath, const char* mode) noexcept
    {
    #ifdef _WIN32
        FILE* file = nullptr;
        if (fopen_s(&file, filepath.c_str(), mode) != 0)
            return nullptr;
        return file;
    #else
        return std::fopen(path.string().c_str(), mode);
    #endif
    }
}

namespace keplar
{
    struct ImageData
    {
        void* pixels    = nullptr;
        int   width     = 0;
        int   height    = 0;
        int   channels  = 0;
    };

    Texture::Texture() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkImage(VK_NULL_HANDLE)
        , m_vkImageMemory(VK_NULL_HANDLE)
        , m_vkImageView(VK_NULL_HANDLE)
        , m_width(0)
        , m_height(0)
        , m_channels(0)
        , m_mipLevels(0)
        , m_format(VK_FORMAT_UNDEFINED)
    {
    }

    Texture::~Texture()
    {
        destroy();
    }

    Texture::Texture(Texture&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkImage(other.m_vkImage)
        , m_vkImageMemory(other.m_vkImageMemory)
        , m_vkImageView(other.m_vkImageView)
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_channels(other.m_channels)
        , m_mipLevels(other.m_mipLevels)
        , m_format(other.m_format)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkImage = VK_NULL_HANDLE;
        other.m_vkImageMemory = VK_NULL_HANDLE;
        other.m_vkImageView = VK_NULL_HANDLE;
        other.m_width = 0;
        other.m_height = 0;
        other.m_channels = 0;
        other.m_mipLevels = 0;
        other.m_format = VK_FORMAT_UNDEFINED;
    }

    Texture& Texture::operator=(Texture&& other) noexcept
    {
        // avoid self-move
        if (this != &other)
        {
            // release current resources
            destroy();

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkImage = other.m_vkImage;
            m_vkImageMemory = other.m_vkImageMemory;
            m_vkImageView = other.m_vkImageView;
            m_width = other.m_width;
            m_height = other.m_height;
            m_channels = other.m_channels;
            m_mipLevels = other.m_mipLevels;
            m_format = other.m_format;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkImage = VK_NULL_HANDLE;
            other.m_vkImageMemory = VK_NULL_HANDLE;
            other.m_vkImageView = VK_NULL_HANDLE;
            other.m_width = 0;
            other.m_height = 0;
            other.m_channels = 0;
            other.m_mipLevels = 0;
            other.m_format = VK_FORMAT_UNDEFINED;
        }
        
        return *this;
    }

    bool Texture::load(const VulkanDevice& device, const VulkanCommandPool& commandPool, const std::string& filepath, const VkFormat& format, bool flipY, bool genMips) noexcept
    {
        // image info container
        ImageData imageData{};
        const std::filesystem::path texturePath = keplar::config::kTextureDir / filepath;

        // load image data from file
        if (!loadImageData(texturePath.string(), imageData, flipY))
        {
            VK_LOG_ERROR("Texture::load :: failed to load image data from file: %s", filepath.c_str());
            return false;
        }

        // create vulkan image from loaded image info
        if (!createImage(device, commandPool, imageData, format, genMips))
        {
            VK_LOG_DEBUG("Texture::load :: failed to create vulkan image from loaded image info: %s", filepath.c_str());
            stbi_image_free(imageData.pixels);
            return false;
        }

        // create vulkan image view so that shaders can access and sample
        if (!createImageView())
        {
            VK_LOG_ERROR("Texture::load :: failed to create image view for texture: %s", filepath.c_str());
            stbi_image_free(imageData.pixels);
            return false;
        }

        // cleanup and return success
        stbi_image_free(imageData.pixels);
        return true;
    }

    bool Texture::load(const VulkanDevice& device, const VulkanCommandPool& commandPool, const tinygltf::Image& gltfImage, const VkFormat& format, bool genMips) noexcept
    {
        // image info container
        ImageData imageData{};
        std::string imageName;
        bool isEmbedded = false;

        if (!gltfImage.image.empty())
        {
            // embedded image: pixels already loaded in tinygltf::Image
            imageData.pixels   = const_cast<void*>(static_cast<const void*>(gltfImage.image.data()));
            imageData.width    = gltfImage.width;
            imageData.height   = gltfImage.height;
            imageData.channels = gltfImage.component;
            imageName          = !gltfImage.name.empty() ? gltfImage.name : "embedded_image";
            isEmbedded         = true;

            if (!imageData.pixels || imageData.width <= 0 || imageData.height <= 0)
            {
                VK_LOG_ERROR("Texture::load failed :: invalid embedded glTF image: %s (%dx%d)", imageName.c_str(), imageData.width, imageData.height);
                return false;
            }
        }
        else if (!gltfImage.uri.empty())
        {
            // non-embedded image: load from external file
            imageName = gltfImage.uri;
            const std::filesystem::path texturePath = keplar::config::kModelDir / imageName;
            if (!loadImageData(texturePath.string(), imageData))
            {
                VK_LOG_ERROR("Texture::load :: failed to load image data: %s", imageName.c_str());
                return false;
            }
        }
        else 
        {
            VK_LOG_ERROR("Texture::load :: glTF image has no embedded data or URI!");
            return false;
        }

        // create vulkan image from loaded image info
        if (!createImage(device, commandPool, imageData, format, genMips))
        {
            VK_LOG_DEBUG("Texture::load :: failed to create vulkan image for loaded glTF image: %s", imageName.c_str());
            if (!isEmbedded) { stbi_image_free(imageData.pixels); }
            return false;
        }

        // create vulkan image view for sampling
        if (!createImageView())
        {
            VK_LOG_ERROR("Texture::load :: failed to create image view for glTF image: %s", imageName.c_str());
            if (!isEmbedded) { stbi_image_free(imageData.pixels); }
            return false;
        }

        // cleanup for non-embedded image
        if (!isEmbedded)
        {
            stbi_image_free(imageData.pixels);
        }

        return true;
    }

    bool Texture::loadImageData(const std::string& filepath, ImageData& imageData, bool flipY) noexcept
    {
        // open file in binary mode
        FILE* file = fopenSafe(filepath, "rb");
        if (!file)
        {
            VK_LOG_ERROR("Texture::loadImageData :: failed to open texture file: %s", filepath.c_str());
            return false;
        }

        // set vertical flip
        stbi_set_flip_vertically_on_load(flipY); 

        // load image data as unsigned byte 
        imageData.pixels = stbi_load_from_file(file, &imageData.width, &imageData.height, &imageData.channels, STBI_rgb_alpha);

        // close file 
        fclose(file);

        // validate image data
        if (!imageData.pixels || imageData.width <= 0 || imageData.height <= 0)
        {
            VK_LOG_ERROR("Texture::loadImageData :: stbi failed to load pixel data or invalid dimensions: %s (%dx%d)", filepath.c_str(), imageData.width, imageData.height);
            if (imageData.pixels) { stbi_image_free(imageData.pixels); }
            return false;
        }

        return true;
    }

    void Texture::destroy() noexcept
    {
        if (m_vkImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_vkDevice, m_vkImageView, nullptr);
            m_vkImageView = VK_NULL_HANDLE;
        }

        if (m_vkImageMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_vkDevice, m_vkImageMemory, nullptr);
            m_vkImageMemory = VK_NULL_HANDLE;
        }

        if (m_vkImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(m_vkDevice, m_vkImage, nullptr);
            m_vkImage = VK_NULL_HANDLE;
        }
    }

    bool Texture::createImage(const VulkanDevice& device, const VulkanCommandPool& commandPool, const ImageData& imageData, const VkFormat& format, bool genMips) noexcept
    {
        // set device and image metadata
        m_vkDevice  = device.getDevice();
        m_width     = imageData.width;
        m_height    = imageData.height;
        m_channels  = imageData.channels;
        m_format    = format;
        m_mipLevels = 1;

        // compute mip levels
        if (genMips)
        {
            const auto maxDim = max(imageData.width, imageData.height);
            m_mipLevels += static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(maxDim))));
        }

        // compute image size
        VkDeviceSize imageSize = m_width * m_height * m_channels * sizeof(uint8_t);
  
        // ------------------------------------------
        // ▶ step 1: create host-visible staging buffer and copy image data
        // ------------------------------------------

        // staging buffer creation info
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext = nullptr;
        bufferCreateInfo.flags = 0;
        bufferCreateInfo.size = imageSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // staging buffer for host-to-device copy
        VkBuffer vkBufferStaging = VK_NULL_HANDLE;
        VkResult vkResult = vkCreateBuffer(m_vkDevice, &bufferCreateInfo, nullptr, &vkBufferStaging);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_ERROR("vkCreateBuffer failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // query memory requirements for the staging buffer
        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(m_vkDevice, vkBufferStaging, &memoryRequirements);

        VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        auto memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, propertyFlags);
        if (!memoryTypeIndex.has_value())
        {
            return false;
        }

        // allocate device memory for the staging buffer
        VkMemoryAllocateInfo memoryAllocationInfo{};
        memoryAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;                  
        memoryAllocationInfo.pNext = nullptr;                                                    
        memoryAllocationInfo.allocationSize = memoryRequirements.size;                      
        memoryAllocationInfo.memoryTypeIndex = *memoryTypeIndex;

        VkDeviceMemory deviceMemoryStaging = VK_NULL_HANDLE;
        vkResult = vkAllocateMemory(m_vkDevice, &memoryAllocationInfo, nullptr, &deviceMemoryStaging);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateMemory failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // bind memory to the staging buffer
        vkResult = vkBindBufferMemory(m_vkDevice, vkBufferStaging, deviceMemoryStaging, 0);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkBindBufferMemory failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // map staging buffer memory
        void* mappedData = nullptr;
        vkResult = vkMapMemory(m_vkDevice, deviceMemoryStaging, 0, imageSize, 0, &mappedData);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkMapMemory failed for staging buffer : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // copy pixel data to staging buffer
        std::memcpy(mappedData, imageData.pixels, imageSize);
        vkUnmapMemory(m_vkDevice, deviceMemoryStaging);

        // ------------------------------------------
        // ▶ step 2: create device-local vulkan image
        // ------------------------------------------

        // image creation info
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext = nullptr;
        imageCreateInfo.flags = 0;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = m_format;
        imageCreateInfo.extent.width = m_width;
        imageCreateInfo.extent.height = m_height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = m_mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // create image 
        vkResult = vkCreateImage(m_vkDevice, &imageCreateInfo, nullptr, &m_vkImage);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateImage failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // query memory requirements
        VkMemoryRequirements vkMemoryRequirements{};
        vkGetImageMemoryRequirements(m_vkDevice, m_vkImage, &vkMemoryRequirements);

        // find suitable memory type
        memoryTypeIndex = device.findMemoryType(vkMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!memoryTypeIndex.has_value())
        {
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // allocate device memory from vulkan heap
        VkMemoryAllocateInfo vkMemoryAllocateInfo{};
        vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vkMemoryAllocateInfo.pNext = nullptr;
        vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
        vkMemoryAllocateInfo.memoryTypeIndex = *memoryTypeIndex;

        vkResult = vkAllocateMemory(m_vkDevice, &vkMemoryAllocateInfo, nullptr, &m_vkImageMemory);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkAllocateMemory failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // bind memory to image
        vkResult = vkBindImageMemory(m_vkDevice, m_vkImage, m_vkImageMemory, 0);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkBindImageMemory failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // ------------------------------------------
        // ▶ step 3: allocate and begin one time submit command buffer
        // ------------------------------------------

        // allocate command buffer from pool 
        auto commandBuffer = commandPool.allocatePrimary();
        if (!commandBuffer.isValid())
        {
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // begin recording commands 
        if (!commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
        {
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // ------------------------------------------
        // ▶ step 4: pipeline barrier to transition image layout for transfer 
        // ------------------------------------------

        // transition image from undefined to transfer-dst for staging copy
        auto srcStage       = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        auto dstStage       = VK_PIPELINE_STAGE_TRANSFER_BIT;
        auto oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED;
        auto newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        auto srcAccessMask  = 0;
        auto dstAccessMask  = VK_ACCESS_TRANSFER_WRITE_BIT;
        auto baseMipLevel   = 0;
        auto levelCount     = m_mipLevels;

        // execute image layout transition
        transitionImageLayout(commandBuffer.get(), srcStage, dstStage, oldLayout, newLayout, srcAccessMask, dstAccessMask, baseMipLevel, levelCount);

        // ------------------------------------------
        // ▶ step 5: copy data from staging buffer to device-local image 
        // ------------------------------------------

        // buffer image copy info
        VkBufferImageCopy bufferImageCopy{};
        bufferImageCopy.bufferOffset = 0;
        bufferImageCopy.bufferRowLength = 0;
        bufferImageCopy.bufferImageHeight = 0;
        bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferImageCopy.imageSubresource.mipLevel = 0;
        bufferImageCopy.imageSubresource.baseArrayLayer = 0;
        bufferImageCopy.imageSubresource.layerCount = 1;
        bufferImageCopy.imageOffset.x = 0;
        bufferImageCopy.imageOffset.y = 0;
        bufferImageCopy.imageOffset.z = 0;
        bufferImageCopy.imageExtent.width = m_width;
        bufferImageCopy.imageExtent.height = m_height;
        bufferImageCopy.imageExtent.depth = 1;

        // copy buffer to image
        vkCmdCopyBufferToImage(commandBuffer.get(), vkBufferStaging, m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

        // ------------------------------------------
        // ▶ step 6: pipeline barrier to transition image layout to shader-read (generate mipmaps if enabled)
        // ------------------------------------------

        if (genMips) 
        { 
            // generate all mip levels and transition them to shader-read layout
            generateMipmaps(commandBuffer.get());
        }
        else 
        {
            // transition image from transfer-dst to shader-read for sampling
            srcStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            // execute image layout transition
            transitionImageLayout(commandBuffer.get(), srcStage, dstStage, oldLayout, newLayout, srcAccessMask, dstAccessMask, baseMipLevel, levelCount);
        }

        // ------------------------------------------
        // ▶ step 7: submit command buffer and wait for completion
        // ------------------------------------------

        // end recording commands
        if (!commandBuffer.end())
        {
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // create a fence 
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;
        fenceCreateInfo.flags = 0;  

        VulkanFence copyFence;
        if (!copyFence.initialize(m_vkDevice, fenceCreateInfo))
        {
            VK_LOG_FATAL("failed for create fence");
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // submit info for executing the command buffer
        VkCommandBuffer vkCommandBuffer = commandBuffer.get();
        VkSubmitInfo vkSubmitInfo{};
        vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        vkSubmitInfo.pNext = nullptr;
        vkSubmitInfo.commandBufferCount = 1;
        vkSubmitInfo.pCommandBuffers = &vkCommandBuffer;

        // submit the copy command and signal the fence on completion
        vkResult = vkQueueSubmit(device.getGraphicsQueue(), 1, &vkSubmitInfo, copyFence.get());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkQueueSubmit failed for buffer copy from host to device : %s (code: %d)", string_VkResult(vkResult), vkResult);
            vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
            vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
            return false;
        }

        // cleanup staging resources 
        copyFence.wait();
        commandPool.deallocate(commandBuffer);
        vkFreeMemory(m_vkDevice, deviceMemoryStaging, nullptr);
        vkDestroyBuffer(m_vkDevice, vkBufferStaging, nullptr);
        return true;
    }

    bool Texture::createImageView() noexcept
    {
        // image view creation info
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = m_vkImage;
        imageViewCreateInfo.format = m_format;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = m_mipLevels;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        // create texture image view
        VkResult vkResult = vkCreateImageView(m_vkDevice, &imageViewCreateInfo, nullptr, &m_vkImageView);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateImageView failed to create view for texture image : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        return true;
    }

    void Texture::generateMipmaps(VkCommandBuffer commandBuffer) noexcept
    {
        // validate mip levels
        if (m_mipLevels == 1) 
        {
            VK_LOG_WARN("texture has only 1 mip level; mipmap generation skipped");
            return;
        }

        int32_t mipWidth  = m_width;
        int32_t mipHeight = m_height;

        // local variables for pipeline stages and layout transitions during mipmap generation
        VkPipelineStageFlags  srcStage     = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkPipelineStageFlags  dstStage     = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkImageLayout         oldLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout         newLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags         srcAccess    = 0;
        VkAccessFlags         dstAccess    = 0;
        uint32_t              baseMipLevel = 0;
        uint32_t              levelCount   = 1;

        for (uint32_t i = 1; i < m_mipLevels; ++i)
        {
            // transition previous mip level from transfer-dst -> transfer-src
            srcStage     = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage     = VK_PIPELINE_STAGE_TRANSFER_BIT;
            oldLayout    = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            newLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            srcAccess    = VK_ACCESS_TRANSFER_WRITE_BIT;
            dstAccess    = VK_ACCESS_TRANSFER_READ_BIT;
            baseMipLevel = i - 1;

            transitionImageLayout(commandBuffer, srcStage, dstStage, oldLayout, newLayout, srcAccess, dstAccess, baseMipLevel, levelCount);

            // transition current mip level from undefined -> transfer-dst
            oldLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
            newLayout    = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            srcAccess    = 0;
            dstAccess    = VK_ACCESS_TRANSFER_WRITE_BIT;
            baseMipLevel = i;

            transitionImageLayout(commandBuffer, srcStage, dstStage, oldLayout, newLayout, srcAccess, dstAccess, baseMipLevel, levelCount);

            // blit previous mip level into current mip level
            VkImageBlit blit{};
            // source
            blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel       = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount     = 1;
            blit.srcOffsets[0]                 = {0, 0, 0};
            blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
            // destination 
            blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel       = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount     = 1;
            blit.dstOffsets[0]                 = {0, 0, 0};
            blit.dstOffsets[1]                 = { max(1, mipWidth / 2), max(1, mipHeight / 2), 1 };

            vkCmdBlitImage(commandBuffer, m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            // transition previous mip level from transfer-src -> shader-read
            srcStage     = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage     = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            oldLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            newLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcAccess    = VK_ACCESS_TRANSFER_READ_BIT;
            dstAccess    = VK_ACCESS_SHADER_READ_BIT;
            baseMipLevel = i - 1;

            transitionImageLayout(commandBuffer, srcStage, dstStage, oldLayout, newLayout, srcAccess, dstAccess, baseMipLevel, levelCount);

            // halve the dimensions for the next mip level
            mipWidth  = max(1, mipWidth / 2);
            mipHeight = max(1, mipHeight / 2);
        }

        // transition last mip level from transfer-dst -> shader-read
        srcStage     = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage     = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        oldLayout    = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        newLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        srcAccess    = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstAccess    = VK_ACCESS_SHADER_READ_BIT;
        baseMipLevel = m_mipLevels - 1;

        transitionImageLayout(commandBuffer, srcStage, dstStage, oldLayout, newLayout, srcAccess, dstAccess, baseMipLevel, levelCount);
    }

    void Texture::transitionImageLayout(VkCommandBuffer commandBuffer, 
                                        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, 
                                        VkImageLayout oldLayout, VkImageLayout newLayout, 
                                        VkAccessFlags srcAccess, VkAccessFlags dstAccess, 
                                        uint32_t baseMipLevel, uint32_t levelCount) noexcept
    {
        // image memory barrier setup
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_vkImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = baseMipLevel;
        barrier.subresourceRange.levelCount = levelCount; 
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        // execute pipeline barrier
        vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
}   // namespace keplar
