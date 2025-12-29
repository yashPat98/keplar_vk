// ────────────────────────────────────────────
//  File: gltf_model.hpp · Created by Yash Patel · 9-28-2025
// ────────────────────────────────────────────

#pragma once

#include <optional>

#include "asset_io.hpp"
#include "math3d.hpp"

#include "vulkan/vulkan_config.hpp"
#include "vulkan/vulkan_buffer.hpp"
#include "vulkan/vulkan_descriptor_pool.hpp"
#include "vulkan/vulkan_samplers.hpp"
#include "graphics/texture.hpp"

namespace keplar
{
    class GLTFModel
    {
        public:
            // creation and destruction
            GLTFModel() noexcept;
            ~GLTFModel();

            // model lifecycle: load, render and update
            bool load(const VulkanDevice& device, const VulkanCommandPool& commandPool, const std::string& filename) noexcept;
            void render(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) noexcept;
            void update(float dt) noexcept;

            // descriptor management: allocation and updates
            bool allocateDescriptorSets(VkDescriptorPool descriptorPool) noexcept;
            void updateDescriptorSets(const VulkanSamplers& sampler) noexcept;
            DescriptorRequirements getDescriptorRequirements() const noexcept;

            // manage shared vulkan resources: descriptor set layout, push constants
            static void initSharedResources(VkDevice vkDevice) noexcept;
            static void destroySharedResources(VkDevice vkDevice) noexcept;
            static const std::vector<VkVertexInputBindingDescription>& getBindings() noexcept { return s_vertexBindings; }
            static const std::vector<VkVertexInputAttributeDescription>& getAttributes() noexcept { return s_vertexAttributes; }
            static VkDescriptorSetLayout getDescriptorSetLayout() noexcept { return s_descriptorSetLayout; }
            static VkPushConstantRange getPushConstantRange() noexcept { return s_pushConstantRange; }

        private:
            // forward declarations
            struct Vertex;
            struct Primitive;
            struct Mesh;
            struct Scene;
            struct Node;
            struct Material;

            // internal helpers for loading model
            bool loadMeshes(const tinygltf::Model& model, const VulkanDevice& device, const VulkanCommandPool& commandPool) noexcept;
            bool loadSceneGraph(const tinygltf::Model& model) noexcept;
            bool loadTextures(const tinygltf::Model& model, const VulkanDevice& device, const VulkanCommandPool& commandPool) noexcept;
            bool loadMaterials(const tinygltf::Model& model) noexcept;
            void generateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) noexcept;

        private:
            // vulkan resources
            VkDevice              m_vkDevice;
            VulkanBuffer          m_vertexBuffer;       
            VulkanBuffer          m_indexBuffer;
            uint32_t              m_vertexCount;
            uint32_t              m_indexCount;

            // scene data
            std::vector<Mesh>     m_meshes;
            std::vector<Node>     m_nodes;
            std::vector<Scene>    m_scenes;
            std::vector<Texture>  m_textures;
            std::vector<Material> m_materials;

            // shared vulkan resources: bindings, attributes, descriptor set layout and push constants
            inline static std::vector<VkVertexInputBindingDescription>   s_vertexBindings;
            inline static std::vector<VkVertexInputAttributeDescription> s_vertexAttributes;
            inline static VkDescriptorSetLayout                          s_descriptorSetLayout  = VK_NULL_HANDLE;
            inline static VkPushConstantRange                            s_pushConstantRange{};
    };
}   // namespace keplar
