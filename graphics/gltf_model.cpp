// ────────────────────────────────────────────
//  File: gltf_model.cpp · Created by Yash Patel · 9-28-2025
// ────────────────────────────────────────────

#include "gltf_model.hpp"
#include "core/keplar_config.hpp"
#include "utils/logger.hpp"

namespace
{
    // descriptor set layout bindings
    static constexpr uint32_t kBaseColorBinding          = 1; 
    static constexpr uint32_t kMetallicRoughnessBinding  = 2; 
    static constexpr uint32_t kNormalBinding             = 3; 
    static constexpr uint32_t kOcclusionBinding          = 4; 
    static constexpr uint32_t kEmissiveBinding           = 5; 

    // push constants: model matrix and pbr material properties
    struct alignas(16) PushConstants
    {
        glm::mat4 model;
        glm::vec4 baseColor;
        glm::vec4 pbrFactors;
        glm::vec4 emissiveColor;
    }; 
}

namespace keplar
{
    // ─────────────────────────────────────────────
    // GLTF model structures for meshes, nodes, primitives, and materials
    // ─────────────────────────────────────────────
    struct GLTFModel::Vertex
    {
        glm::vec4 mPosition;
        glm::vec3 mNormal;
        glm::vec2 mUV;
        glm::vec4 mTangent;
    };

    // range of indices with a single material
    struct GLTFModel::Primitive 
    {
        uint32_t mFirstIndex;
        uint32_t mIndexCount;
        int32_t  mMaterialIndex;
    };

    // mesh containing multiple primitives
    struct GLTFModel::Mesh
    {
        std::string mName;
        std::vector<Primitive> mPrimitives;
    };

    // scene node with transform hierarchy
    struct GLTFModel::Node 
    {
        std::string           mName;
        int32_t               mMeshIndex;
        int32_t               mParentIndex;
        glm::mat4             mLocalTransform;
        glm::mat4             mWorldTransform;
        std::vector<int32_t>  mChildren;
    };

    // scene with root node indices
    struct GLTFModel::Scene
    {
        std::string mName;
        std::vector<int32_t> mRootNodes; 
    };

    // pbr material data and descriptor
    struct GLTFModel::Material 
    {
        glm::vec4 mBaseColor;
        float     mMetallic;
        float     mRoughness;
        float     mSpecular;
        glm::vec3 mEmissive;

        std::optional<uint32_t> mBaseColorTex; 
        std::optional<uint32_t> mMetallicRoughnessTex;
        std::optional<uint32_t> mNormalTex;
        std::optional<uint32_t> mOcclusionTex;
        std::optional<uint32_t> mEmissiveTex;

        VkDescriptorSet mDescriptorSet;
    };

    // ─────────────────────────────────────────────
    // GLTFModel implementation
    // ─────────────────────────────────────────────
    GLTFModel::GLTFModel() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vertexCount(0)
        , m_indexCount(0)
    {
    }

    GLTFModel::~GLTFModel()
    {
        // clear model resources
        m_textures.clear();
        m_materials.clear();
        m_scenes.clear();
        m_nodes.clear();
        m_meshes.clear();
    }

    bool GLTFModel::load(const VulkanDevice& device, const VulkanCommandPool& commandPool, const std::string& filename) noexcept
    {
        // ─────────────────────────────────────────
        // load glTF model using TinyGLTF (binary .glb or ASCII .gltf)
        // ─────────────────────────────────────────
        tinygltf::TinyGLTF tinygltf;
        tinygltf::Model model;
        std::string error;
        std::string warning;
        bool status = false;

        // construct full file path
        const std::filesystem::path filepath = keplar::config::kModelDir / filename;
        std::string extension = filepath.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // load model based on extension (binary: glb; ascii: gltf)
        if (extension == ".glb")
        {
            status = tinygltf.LoadBinaryFromFile(&model, &error, &warning, filepath.string());
        }
        else if (extension == ".gltf")
        {
            status = tinygltf.LoadASCIIFromFile(&model, &error, &warning, filepath.string());
        }
        else 
        {
            VK_LOG_ERROR("GLTFModel::load :: unsupported model format: %s", filename.c_str());
            return false;
        }

        // log warnings
        if (!warning.empty())
        {
            VK_LOG_WARN("GLTFModel::load :: tinygltf - %s: %s", filepath.c_str(), warning.c_str());
        }

        // check for errors
        if (!status || !error.empty())
        {
            VK_LOG_ERROR("GLTFModel::load :: tinygltf failed to load model: %s (error: %s)", filepath.c_str(), error.c_str());
            return false;
        }

        // ─────────────────────────────────────────
        // load individual model components: meshes, nodes, scenes, textures, materials
        // ─────────────────────────────────────────
        if (!loadMeshes(model, device, commandPool))
        {
            VK_LOG_ERROR("GLTFModel::load :: failed to load meshes");
            return false;
        }

        if (!loadSceneGraph(model))
        {
            VK_LOG_ERROR("GLTFModel::load :: failed to load nodes and scenes");
            return false;
        }

        if (!loadTextures(model, device, commandPool))
        {
            VK_LOG_ERROR("GLTFModel::load :: failed to load textures");
            return false;
        }

        if (!loadMaterials(model))
        {
            VK_LOG_ERROR("GLTFModel::load :: failed to load materials");
            return false;
        }

        // store device for later use
        m_vkDevice = device.getDevice();
        VK_LOG_DEBUG("GLTFModel::load :: model loaded successfully: %s", filename.c_str());
        return true;
    }

    void GLTFModel::render(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) noexcept
    {
        // skip if no meshes or scenes
        if (m_meshes.empty() || m_scenes.empty())
        {
            VK_LOG_DEBUG("GLTFModel::render :: no meshes or scenes loaded");
            return;
        }

        // render default scene
        const Scene& scene = m_scenes[0];

        // iterative stack-based traversal 
        std::vector<int> stack;
        stack.reserve(m_nodes.size());

        // push root nodes
        for (int rootIdx : scene.mRootNodes)
        {
            stack.push_back(rootIdx);
        }

        // bind vertex and index buffers 
        VkBuffer vertexBuffers[] = { m_vertexBuffer.get() };
        VkDeviceSize offsets[]   = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.get(), 0, VK_INDEX_TYPE_UINT32);

        while (!stack.empty())
        {
            auto nodeIdx = stack.back();
            stack.pop_back();

            // validate node index
            if (nodeIdx < 0 || nodeIdx >= static_cast<int>(m_nodes.size()))
            {
                continue;
            }

            // push children
            Node& node = m_nodes[nodeIdx];
            for (int childIdx : node.mChildren)
            {
                stack.push_back(childIdx);
            }

            // skip nodes with invalid mesh
            if (node.mMeshIndex < 0 || node.mMeshIndex >= static_cast<int>(m_meshes.size()))
            {
                continue;
            }

            // record rendering commands for the mesh
            VkDescriptorSet lastBoundMaterial = VK_NULL_HANDLE;
            for (const auto& primitive : m_meshes[node.mMeshIndex].mPrimitives)
            {
                // skip rendering primitive if it has no material
                if (primitive.mMaterialIndex < 0 || primitive.mMaterialIndex >= static_cast<int32_t>(m_materials.size()))
                {
                    VK_LOG_DEBUG("GLTFModel::render :: invalid material, primitive skipped");
                    continue;
                }         
                
                // avoid redundant binds when consecutive primitives share the same material
                const Material& material = m_materials[primitive.mMaterialIndex];
                if (lastBoundMaterial != material.mDescriptorSet)
                {
                    // bind material descriptor set (set: 1)
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.mDescriptorSet, 0, nullptr);
                    lastBoundMaterial = material.mDescriptorSet;
                }

                // prepare push constants
                PushConstants pushConstants{};
                pushConstants.model         = node.mWorldTransform;
                pushConstants.baseColor     = material.mBaseColor;
                pushConstants.pbrFactors    = glm::vec4(material.mMetallic, material.mRoughness, material.mSpecular, 0.0f);
                pushConstants.emissiveColor = glm::vec4(material.mEmissive, 0.0f);
        
                // record push constants and issue indexed draw call 
                vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
                vkCmdDrawIndexed(commandBuffer, primitive.mIndexCount, 1, primitive.mFirstIndex, 0, 0);
            }
        }
    }

    void GLTFModel::update(float /* dt */) noexcept
    {
    }

    bool GLTFModel::allocateDescriptorSets(VkDescriptorPool descriptorPool) noexcept
    {
        // validate materials 
        if (m_materials.empty())
        {
            VK_LOG_DEBUG("GLTFModel::allocateDescriptorSets :: no materials to allocate descriptor sets");
            return true;
        }

        // validate descriptor set layout
        if (s_descriptorSetLayout == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("GLTFModel::allocateDescriptorSets :: shared descriptor set layout not initialized");
            return false;
        }

        // descriptor set allocation info
        std::vector<VkDescriptorSetLayout> layouts(m_materials.size(), s_descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = descriptorPool;
        allocateInfo.descriptorSetCount = static_cast<uint32_t>(m_materials.size());
        allocateInfo.pSetLayouts = layouts.data();

        // allocate descriptor sets for each material
        std::vector<VkDescriptorSet> descriptorSets(m_materials.size());
        VkResult vkResult = vkAllocateDescriptorSets(m_vkDevice, &allocateInfo, descriptorSets.data());
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("GLTFModel::allocateDescriptorSets :: vkAllocateDescriptorSets failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // assign sets to materials
        for (size_t i = 0; i < m_materials.size(); i++)
        {
            m_materials[i].mDescriptorSet = descriptorSets[i];
        }

        VK_LOG_DEBUG("GLTFModel::allocateDescriptorSets :: descriptor sets allocation successful");
        return true;
    }

    void GLTFModel::updateDescriptorSets(const VulkanSamplers& sampler) noexcept
    {
        // skip if no materials or invalid layout
        if (m_materials.empty() || s_descriptorSetLayout == VK_NULL_HANDLE)
        {
            VK_LOG_DEBUG("GLTFModel::allocateDescriptorSets :: no materials or shared layout to update descriptor sets");
            return;
        }

        // temporary storage for descriptor writes and their image info
        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorImageInfo> imageInfos;

        // pre-allocate
        const size_t maxTexturesPerMaterial = 5;
        writes.reserve(m_materials.size() * maxTexturesPerMaterial);
        imageInfos.reserve(m_materials.size() * maxTexturesPerMaterial);

        // select the best available sampler
        VkSampler vkSampler = sampler.get(VulkanSamplers::Type::AnisotropicRepeat);
        if (vkSampler == VK_NULL_HANDLE)
        {
            vkSampler = sampler.get(VulkanSamplers::Type::LinearRepeat);
        }

        // prepare descriptor updates per material
        for (const auto& material : m_materials)
        {
            std::array<std::pair<uint32_t, std::optional<int>>, 5> textureBindings = 
            {{
                { kBaseColorBinding,          material.mBaseColorTex },
                { kNormalBinding,             material.mNormalTex },
                { kMetallicRoughnessBinding,  material.mMetallicRoughnessTex },
                { kOcclusionBinding,          material.mOcclusionTex },
                { kEmissiveBinding,           material.mEmissiveTex }
            }};
            
            for (const auto& [binding, texIndex] : textureBindings)
            {
                // validate texture index
                if (!texIndex.has_value()) 
                    continue;

                // image info
                imageInfos.emplace_back();
                VkDescriptorImageInfo& info = imageInfos.back();
                info.sampler     = vkSampler;
                info.imageView   = m_textures[texIndex.value()].getImageView();
                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                // write descriptor set
                VkWriteDescriptorSet write{};
                write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext            = nullptr;
                write.dstSet           = material.mDescriptorSet;
                write.dstBinding       = binding;
                write.dstArrayElement  = 0;
                write.descriptorCount  = 1;
                write.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pImageInfo       = &info;
                write.pBufferInfo      = nullptr;
                write.pTexelBufferView = nullptr;

                writes.emplace_back(write);
            }
        }

        // TODO: update skin matrices (single write for all skinned objects)

        // commit batch-update for all descriptor sets
        vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        VK_LOG_DEBUG("GLTFModel::updateDescriptorSets :: updated descriptor sets successful");
    }

    DescriptorRequirements GLTFModel::getDescriptorRequirements() const noexcept
    {
        // each material will have one descriptor set
        DescriptorRequirements requirements{};
        requirements.mMaxSets= static_cast<uint32_t>(m_materials.size());

        // count how many textures will be used across all materials
        for (const auto& material : m_materials)
        {
            if (material.mBaseColorTex.has_value())         { requirements.mSamplerCount++; }
            if (material.mNormalTex.has_value())            { requirements.mSamplerCount++; }
            if (material.mMetallicRoughnessTex.has_value()) { requirements.mSamplerCount++; }
            if (material.mEmissiveTex.has_value())          { requirements.mSamplerCount++; }
            if (material.mOcclusionTex.has_value())         { requirements.mSamplerCount++; }
        }

        // uniform buffer count for pbr factors (one per material)
        requirements.mUniformCount = static_cast<uint32_t>(m_materials.size());
        return requirements;
    }

    bool GLTFModel::loadMeshes(const tinygltf::Model& model, const VulkanDevice& device, const VulkanCommandPool& commandPool) noexcept
    {
        // clear previous data
        m_meshes.clear();
        m_vertexCount = 0;
        m_indexCount  = 0;

        // precompute total vertex and index counts
        for (const auto& mesh : model.meshes)
        {
            for (const auto& primitive : mesh.primitives)
            {
                // skip invalid primitive
                if (primitive.attributes.find("POSITION") == primitive.attributes.end() || primitive.indices < 0)
                    continue;

                const auto& posAccessor  = model.accessors.at(primitive.attributes.at("POSITION"));
                const auto& indexAccessor = model.accessors.at(primitive.indices);

                // update counts
                m_vertexCount += static_cast<uint32_t>(posAccessor.count);
                m_indexCount  += static_cast<uint32_t>(indexAccessor.count);
            }
        }

        // aggregated vertex/index data from all meshes
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        bool hasTangents = false;

        // pre-allocate vectors
        m_meshes.reserve(model.meshes.size());
        vertices.reserve(m_vertexCount);
        indices.reserve(m_indexCount);

        // process meshes and their primitives
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;

        for (const auto& gltfMesh : model.meshes)
        {
            Mesh mesh{};
            mesh.mName = gltfMesh.name;
            mesh.mPrimitives.reserve(gltfMesh.primitives.size());

            for (const auto& gltfPrimitive : gltfMesh.primitives)
            {
                // skip if position is missing
                const auto& attributes = gltfPrimitive.attributes;
                if (attributes.find("POSITION") == attributes.end() || gltfPrimitive.indices < 0)
                {
                    VK_LOG_DEBUG("GLTFModel::loadMeshes : primitive skipped; position or indices missing in mesh: %s", gltfMesh.name.c_str());
                    continue;
                }

                // pointers to vertex attribute data for current primitive
                const float* positionBuffer = nullptr;
                const float* normalBuffer   = nullptr;
                const float* uvBuffer       = nullptr;
                const float* tangentBuffer  = nullptr;

                // stride in floats for interleaved attributes
                size_t vertexCount    = 0;
                size_t positionStride = 0;
                size_t normalStride   = 0;
                size_t uvStride       = 0;
                size_t tangentStride  = 0;

                // position 
                if (const auto& itr = attributes.find("POSITION"); itr != attributes.end())
                {
                    const auto& accessor = model.accessors.at(itr->second);
                    const auto& view     = model.bufferViews[accessor.bufferView];
                    positionBuffer       = reinterpret_cast<const float*>(&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
                    positionStride       = view.byteStride ? view.byteStride / sizeof(float) : 3;
                    vertexCount          = accessor.count;
                }

                // normal
                if (const auto& itr = attributes.find("NORMAL"); itr != attributes.end())
                {
                    const auto& accessor = model.accessors.at(itr->second);
                    const auto& view     = model.bufferViews[accessor.bufferView];
                    normalBuffer         = reinterpret_cast<const float*>(&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
                    normalStride         = view.byteStride ? view.byteStride / sizeof(float) : 3;
                }

                // texcoords
                if (const auto& itr = attributes.find("TEXCOORD_0"); itr != attributes.end())
                {
                    const auto& accessor = model.accessors.at(itr->second);
                    const auto& view     = model.bufferViews[accessor.bufferView];
                    uvBuffer             = reinterpret_cast<const float*>(&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
                    uvStride             = view.byteStride ? view.byteStride / sizeof(float) : 2;
                }

                // tangent
                if (const auto& itr = attributes.find("TANGENT"); itr != attributes.end())
                {
                    const auto& accessor = model.accessors.at(itr->second);
                    const auto& view     = model.bufferViews[accessor.bufferView];
                    tangentBuffer        = reinterpret_cast<const float*>(&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
                    tangentStride        = view.byteStride ? view.byteStride / sizeof(float) : 4;
                    hasTangents          = true;
                }

                // append vertex data for current primitive
                for (size_t i = 0; i < vertexCount; i++)
                {
                    Vertex vertex{};
                    vertex.mPosition = glm::vec4(glm::make_vec3(&positionBuffer[i * positionStride]), 1.0f);
                    vertex.mNormal   = normalBuffer  ? glm::make_vec3(&normalBuffer[i * normalStride])    : glm::vec3(0.0f);
                    vertex.mUV       = uvBuffer      ? glm::make_vec2(&uvBuffer[i * uvStride])            : glm::vec2(0.0f);
                    vertex.mTangent  = tangentBuffer ? glm::make_vec4(&tangentBuffer[i * tangentStride])  : glm::vec4(0.0f);
                    vertices.emplace_back(std::move(vertex));
                }

                // indices
                const auto& indexAccessor = model.accessors[gltfPrimitive.indices];
                const auto& indexView     = model.bufferViews[indexAccessor.bufferView];
                const auto& indexBuffer   = model.buffers[indexView.buffer].data;
                const void* dataPtr       = &indexBuffer[indexAccessor.byteOffset + indexView.byteOffset];

                // append index data for current primitive
                switch (indexAccessor.componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: 
                    {
                        const uint32_t* buffer = static_cast<const uint32_t*>(dataPtr);
                        for (size_t i = 0; i < indexAccessor.count; i++)
                            indices.emplace_back(buffer[i] + vertexOffset);
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: 
                    {
                        const uint16_t* buffer = static_cast<const uint16_t*>(dataPtr);
                        for (size_t i = 0; i < indexAccessor.count; i++)
                            indices.emplace_back(static_cast<uint32_t>(buffer[i]) + vertexOffset);
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: 
                    {
                        const uint8_t* buffer = static_cast<const uint8_t*>(dataPtr);
                        for (size_t i = 0; i < indexAccessor.count; i++)
                            indices.emplace_back(static_cast<uint32_t>(buffer[i]) + vertexOffset);
                        break;
                    }
                    default:
                        VK_LOG_WARN("GLTFModel::loadMeshes : unsupported index component type");
                        break;
                }

                // append primitive info for current mesh
                Primitive primitive{};
                primitive.mFirstIndex = indexOffset;
                primitive.mIndexCount = static_cast<uint32_t>(indexAccessor.count);
                primitive.mMaterialIndex = (gltfPrimitive.material >= 0) ? gltfPrimitive.material : -1;
                mesh.mPrimitives.emplace_back(std::move(primitive));

                // update offsets for next primitive
                vertexOffset += static_cast<uint32_t>(vertexCount);
                indexOffset += static_cast<uint32_t>(indexAccessor.count);
            }
            m_meshes.emplace_back(std::move(mesh));
        }

        if (!hasTangents)
        {
            generateTangents(vertices, indices);
        }

        // create device-local vertex buffer: positions, normals, uvs, tangents
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferCreateInfo.size = sizeof(Vertex) * vertices.size();

        if (!m_vertexBuffer.createDeviceLocal(device, commandPool, bufferCreateInfo, vertices.data(), bufferCreateInfo.size))
        {
            VK_LOG_ERROR("GLTFModel::loadMeshes :: failed to create device-local buffer for vertex data");
            return false;
        }

        // create device-local index buffer: triangle indices
        bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferCreateInfo.size = sizeof(uint32_t) * indices.size();

        if (!m_indexBuffer.createDeviceLocal(device, commandPool, bufferCreateInfo, indices.data(), bufferCreateInfo.size))
        {
            VK_LOG_ERROR("GLTFModel::loadMeshes :: failed to create device-local buffer for index data");
            return false;
        }

        VK_LOG_DEBUG("GLTFModel::loadMeshes :: meshes uploaded to device buffer successfully (vertices:%u, indices:%u)", m_vertexCount, m_indexCount);
        return true;
    }

    bool GLTFModel::loadSceneGraph(const tinygltf::Model& model) noexcept
    {
        // clear previous data
        m_nodes.clear();
        m_scenes.clear();

        // pre-allocate vectors
        m_nodes.reserve(model.nodes.size());
        m_scenes.reserve(model.scenes.size());

        // load all nodes
        for (size_t i = 0; i < model.nodes.size(); i++)
        {
            const auto& gltfNode = model.nodes[i];
            Node node{};
            node.mName       = gltfNode.name;
            node.mMeshIndex  = gltfNode.mesh >= 0 ? gltfNode.mesh : -1;
            node.mChildren   = gltfNode.children;

            // compute local transform
            if (!gltfNode.matrix.empty())
            {
                node.mLocalTransform = glm::make_mat4(gltfNode.matrix.data());
            }
            else 
            {
                glm::vec3 translation(0.0f);
                glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3 scale(1.0f);

                if (!gltfNode.translation.empty())
                {
                    translation = glm::vec3(static_cast<float>(gltfNode.translation[0]),
                                            static_cast<float>(gltfNode.translation[1]),
                                            static_cast<float>(gltfNode.translation[2]));
                }

                if (!gltfNode.rotation.empty())
                {
                    rotation = glm::quat(static_cast<float>(gltfNode.rotation[3]),
                                         static_cast<float>(gltfNode.rotation[0]),
                                         static_cast<float>(gltfNode.rotation[1]),
                                         static_cast<float>(gltfNode.rotation[2]));
                }
                
                if (!gltfNode.scale.empty())
                {
                    scale = glm::vec3(static_cast<float>(gltfNode.scale[0]),
                                      static_cast<float>(gltfNode.scale[1]),
                                      static_cast<float>(gltfNode.scale[2]));
                }

                node.mLocalTransform = glm::translate(glm::mat4(1.0f), translation) *
                                       glm::mat4_cast(rotation) * 
                                       glm::scale(glm::mat4(1.0f), scale);
            }

            m_nodes.emplace_back(std::move(node));
        }

        // link child nodes to their parent for scene hierarchy
        for (size_t parentIdx = 0; parentIdx < m_nodes.size(); parentIdx++)
        {
            for (size_t childIdx : m_nodes[parentIdx].mChildren)
            {
                if (childIdx >= 0 && childIdx < static_cast<int>(m_nodes.size()))
                {
                    m_nodes[childIdx].mParentIndex = static_cast<int32_t>(parentIdx);
                }
            }
        }

        // track scene root nodes
        for (const auto& gltfScene : model.scenes)
        {
            Scene scene{};
            scene.mName       = gltfScene.name;
            scene.mRootNodes  = gltfScene.nodes;

            m_scenes.emplace_back(std::move(scene));
            VK_LOG_DEBUG("GLTFModel::loadSceneGraph :: scene loaded: '%s'", gltfScene.name.c_str());
        }

        // recursive lambda to compute world transforms for a node and its children
        std::function<void(int, const glm::mat4&)> computeWorldTransform;
        computeWorldTransform = [this, &computeWorldTransform](int nodeIdx, const glm::mat4& parentTransform)
        {
            // validate node index
            if (nodeIdx < 0 || nodeIdx >= static_cast<int>(m_nodes.size())) 
                return;

            // compute world transform
            Node& node = m_nodes[nodeIdx];
            node.mWorldTransform = parentTransform * node.mLocalTransform;

            // recursively update children
            for (int childIdx : node.mChildren)
            {
                computeWorldTransform(childIdx, node.mWorldTransform);
            }
        };

        // compute world transforms for all scenes
        for (const auto& scene : m_scenes)
        {
            for (int rootIdx : scene.mRootNodes)
            {
                computeWorldTransform(rootIdx, glm::mat4(1.0f));
            }
        }

        VK_LOG_DEBUG("GLTFModel::loadSceneGraph :: loaded %zu nodes across %zu scenes", m_nodes.size(), m_scenes.size());
        return true;
    }

    bool GLTFModel::loadTextures(const tinygltf::Model& model, const VulkanDevice& device, const VulkanCommandPool& commandPool) noexcept
    {
        // clear previous data
        m_textures.clear();
        m_textures.reserve(model.images.size());

        // prepare an array of VkFormat per image defaulting to UNORM
        std::vector<VkFormat> textureFormats(model.images.size(), VK_FORMAT_R8G8B8A8_UNORM);

        // assign sRGB to color textures based on materials
        for (const auto& gltfMaterial : model.materials)
        {
            auto setFormat = [&](int textureIndex, bool isColor)
            {
                if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
                    return;

                int imageIndex = model.textures[textureIndex].source;
                if (imageIndex < 0 || imageIndex >= static_cast<int>(textureFormats.size()))
                    return;

                if (isColor)
                {
                    textureFormats[imageIndex] = VK_FORMAT_R8G8B8A8_SRGB;
                }
                else if (textureFormats[imageIndex] != VK_FORMAT_R8G8B8A8_SRGB)
                {
                    textureFormats[imageIndex] = VK_FORMAT_R8G8B8A8_UNORM;
                }
            };

            setFormat(gltfMaterial.pbrMetallicRoughness.baseColorTexture.index, true);
            setFormat(gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index, false);
            setFormat(gltfMaterial.normalTexture.index, false);
            setFormat(gltfMaterial.occlusionTexture.index, false);
            setFormat(gltfMaterial.emissiveTexture.index, true);
        }

        // lambda to decide if mipmaps should be generated
        auto shouldGenerateMipmaps = [](const tinygltf::Image& image) noexcept -> bool 
        {
            std::string name = image.name.empty() ? image.uri : image.name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            // skip mipmaps for normal maps; generate for all others
            return name.find("normal") == std::string::npos;
        };

        // load each glTF image as a vulkan texture
        for (size_t i = 0; i < model.images.size(); ++i)
        {
            const auto& gltfImage = model.images[i];
            VkFormat& format = textureFormats[i];

            Texture texture;
            if (!texture.load(device, commandPool, gltfImage, format, shouldGenerateMipmaps(gltfImage)))
            {
                VK_LOG_ERROR("Model::loadTextures :: failed to load texture: %s", gltfImage.uri.c_str());
                return false;
            }

            m_textures.emplace_back(std::move(texture));
        }

        VK_LOG_DEBUG("Model::loadTextures :: textures loaded successfully : %zu", m_textures.size());
        return true;
    }

    bool GLTFModel::loadMaterials(const tinygltf::Model& model) noexcept
    {
        // clear previous data
        m_materials.clear();
        m_materials.reserve(model.materials.size());

        for (const auto& gltfMaterial : model.materials)
        {
            Material material{};

            // ─────────────────────────────────────────
            // base material properties
            // ─────────────────────────────────────────
            material.mBaseColor = glm::vec4(1.0f);
            material.mMetallic  = static_cast<float>(gltfMaterial.pbrMetallicRoughness.metallicFactor);
            material.mRoughness = static_cast<float>(gltfMaterial.pbrMetallicRoughness.roughnessFactor);
            material.mSpecular  = 1.0f; // KHR_materials_specular
            material.mEmissive = glm::vec3(0.0f);

            if (gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() == 4)
            {
                const auto& baseColor = gltfMaterial.pbrMetallicRoughness.baseColorFactor;
                material.mBaseColor = glm::vec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
            }

            if (gltfMaterial.emissiveFactor.size() == 3)
            {
                const auto& emissiveColor = gltfMaterial.emissiveFactor;
                material.mEmissive = glm::vec3(emissiveColor[0], emissiveColor[1], emissiveColor[2]);
            }

            // ─────────────────────────────────────────
            // texture indices
            // ─────────────────────────────────────────
            auto getTextureIndex = [&](int index) -> std::optional<uint32_t> 
            {
                if (index < 0 || index >= static_cast<int>(model.textures.size()))
                    return std::nullopt;

                int imageIndex = model.textures[index].source;
                if (imageIndex < 0 || imageIndex >= m_textures.size())
                    return std::nullopt;

                return imageIndex;
            };

            material.mBaseColorTex          = getTextureIndex(gltfMaterial.pbrMetallicRoughness.baseColorTexture.index);
            material.mMetallicRoughnessTex  = getTextureIndex(gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index);
            material.mNormalTex             = getTextureIndex(gltfMaterial.normalTexture.index);
            material.mOcclusionTex          = getTextureIndex(gltfMaterial.occlusionTexture.index);
            material.mEmissiveTex           = getTextureIndex(gltfMaterial.emissiveTexture.index);

            // descriptor set creation is deferred
            material.mDescriptorSet = VK_NULL_HANDLE;
            m_materials.emplace_back(std::move(material));
        }

        VK_LOG_DEBUG("GLTFModel::loadMaterials :: materials loaded successfullt: %zu", m_materials.size());
        return true;
    }

    void GLTFModel::generateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) noexcept
    {
        std::vector<glm::vec3> tan1(vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> tan2(vertices.size(), glm::vec3(0.0f));

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            glm::vec3 v0 = glm::vec3(vertices[i0].mPosition);
            glm::vec3 v1 = glm::vec3(vertices[i1].mPosition);
            glm::vec3 v2 = glm::vec3(vertices[i2].mPosition);

            glm::vec2 w0 = vertices[i0].mUV;
            glm::vec2 w1 = vertices[i1].mUV;
            glm::vec2 w2 = vertices[i2].mUV;

            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;

            glm::vec2 deltaUV1 = w1 - w0;
            glm::vec2 deltaUV2 = w2 - w0;

            float r = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
            if (fabs(r) < 1e-6f) {
                // degenerate UV, skip this triangle's tangent calculation
                continue;
            }
            float f = 1.0f / r;

            glm::vec3 sdir = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
            glm::vec3 tdir = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

            tan1[i0] += sdir;
            tan1[i1] += sdir;
            tan1[i2] += sdir;

            tan2[i0] += tdir;
            tan2[i1] += tdir;
            tan2[i2] += tdir;
        }

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            glm::vec3 n = glm::normalize(vertices[i].mNormal);
            glm::vec3 t = tan1[i];
            glm::vec3 tangent = glm::normalize(t - n * glm::dot(n, t));

            float handedness = (glm::dot(glm::cross(n, t), tan2[i]) < 0.0f) ? -1.0f : 1.0f;
            vertices[i].mTangent = glm::vec4(tangent, handedness);
        }
    }

    void GLTFModel::initSharedResources(VkDevice vkDevice) noexcept
    {
        // early exit if shared resources are already initialized
        if (s_descriptorSetLayout != VK_NULL_HANDLE)
        {
            return;
        } 

        // ─────────────────────────────────────────────
        // descriptor set layout bindings
        // ─────────────────────────────────────────────
        std::array<VkDescriptorSetLayoutBinding, 5> descriptorBindings
        {
           {{kBaseColorBinding,         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {kMetallicRoughnessBinding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {kNormalBinding,            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {kOcclusionBinding,         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {kEmissiveBinding,          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}
        };

        // descriptor set layout creation info
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;
        layoutInfo.flags = 0;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
        layoutInfo.pBindings = descriptorBindings.data();

        VkResult vkResult = vkCreateDescriptorSetLayout(vkDevice, &layoutInfo, nullptr, &s_descriptorSetLayout);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("GLTFModel::initSharedResources :: vkCreateDescriptorSetLayout failed : %s (code: %d)", string_VkResult(vkResult), vkResult);
        }

        // ─────────────────────────────────────────────
        // push constant range: model + material data
        // ─────────────────────────────────────────────
        s_pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        s_pushConstantRange.offset     = 0;
        s_pushConstantRange.size       = sizeof(PushConstants); 

        // ─────────────────────────────────────────────
        // vertex input binding (single interleaved buffer)
        // ─────────────────────────────────────────────
        s_vertexBindings = 
        {{
            {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}
        }};

        // ─────────────────────────────────────────────
        // vertex input attributes
        // ─────────────────────────────────────────────
        s_vertexAttributes = 
        {{
            {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT,  offsetof(Vertex, mPosition)},  // position
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT,     offsetof(Vertex, mNormal)},    // normal
            {2, 0, VK_FORMAT_R32G32_SFLOAT,        offsetof(Vertex, mUV)},        // uv
            {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT,  offsetof(Vertex, mTangent)}    // tangent
        }};
    }

    void GLTFModel::destroySharedResources(VkDevice vkDevice) noexcept
    {
        // destroy shared descriptor set layout
        if (s_descriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(vkDevice, s_descriptorSetLayout, nullptr);
            s_descriptorSetLayout = VK_NULL_HANDLE;
        }
    }
}   // namespace keplar
