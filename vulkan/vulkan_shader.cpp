// ────────────────────────────────────────────
//  File: vulkan_shader.cpp · Created by Yash Patel · 8-18-2025
// ────────────────────────────────────────────

#include "vulkan_shader.hpp"

#include <fstream>
#include "core/config.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanShader::VulkanShader() noexcept
        : m_vkDevice(VK_NULL_HANDLE)
        , m_vkShaderModule(VK_NULL_HANDLE)
        , m_vkPipelineShaderStageCreateInfo{}
    {
    }

    VulkanShader::~VulkanShader()
    {
        // destroy shader module
        if (m_vkShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_vkDevice, m_vkShaderModule, nullptr);
            m_vkShaderModule = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
            VK_LOG_INFO("shader module destroyed successfully");
        }
    }

    VulkanShader::VulkanShader(VulkanShader&& other) noexcept
        : m_vkDevice(other.m_vkDevice)
        , m_vkShaderModule(other.m_vkShaderModule)
        , m_vkPipelineShaderStageCreateInfo(other.m_vkPipelineShaderStageCreateInfo)
    {
        // reset the other
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkShaderModule = VK_NULL_HANDLE;
        other.m_vkPipelineShaderStageCreateInfo = {};
    }

    VulkanShader& VulkanShader::operator=(VulkanShader&& other) noexcept
    {
        if (this != &other)
        {
            // release current resources
            if (m_vkShaderModule != VK_NULL_HANDLE)
            {
                vkDestroyShaderModule(m_vkDevice, m_vkShaderModule, nullptr);
            }

            // transfer ownership
            m_vkDevice = other.m_vkDevice;
            m_vkShaderModule = other.m_vkShaderModule;
            m_vkPipelineShaderStageCreateInfo = other.m_vkPipelineShaderStageCreateInfo;

            // reset the other
            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkShaderModule = VK_NULL_HANDLE;
            other.m_vkPipelineShaderStageCreateInfo = {};
        }

        return *this;
    }

    bool VulkanShader::initialize(VkDevice vkDevice, VkShaderStageFlagBits stage, const std::string& spirvFile) noexcept
    {
        // validate device handle
        if (vkDevice == VK_NULL_HANDLE)
        {
            VK_LOG_ERROR("VulkanShader::initialize failed: VkDevice is VK_NULL_HANDLE");
            return false;
        }

        // build full path to SPIR-V file
        std::filesystem::path filepath = keplar::config::kShaderDir / spirvFile;

        // load the SPIR-V binary from disk
        auto spirvCode = loadSPIRVFile(filepath.string());
        if (spirvCode.empty())
        {
            VK_LOG_WARN("VulkanShader::initialize failed: SPIR-V file '%s' not found or empty", filepath.string().c_str());
            return false;
        }

        // shader module creation info
        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.pNext = nullptr;
        shaderModuleCreateInfo.flags = 0;
        shaderModuleCreateInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
        shaderModuleCreateInfo.pCode = spirvCode.data();

        // create shader module
        VkResult vkResult = vkCreateShaderModule(vkDevice, &shaderModuleCreateInfo, nullptr, &m_vkShaderModule);
        if (vkResult != VK_SUCCESS)
        {
            VK_LOG_FATAL("vkCreateShaderModule failed to create shader module : %s (code: %d)", string_VkResult(vkResult), vkResult);
            return false;
        }

        // setup shader stage info for pipeline
        m_vkPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_vkPipelineShaderStageCreateInfo.pNext = nullptr;
        m_vkPipelineShaderStageCreateInfo.flags = 0;
        m_vkPipelineShaderStageCreateInfo.stage = stage;
        m_vkPipelineShaderStageCreateInfo.module = m_vkShaderModule;
        m_vkPipelineShaderStageCreateInfo.pName = "main";
        m_vkPipelineShaderStageCreateInfo.pSpecializationInfo = nullptr;

        // store device handle for destruction
        m_vkDevice = vkDevice;
        VK_LOG_INFO("shader module initialized successfully using SPIR-V file: '%s'.", spirvFile.c_str());
        return true;
    }

    std::vector<uint32_t> VulkanShader::loadSPIRVFile(const std::string& filepath) const noexcept
    {
        // open file and seek to end to determine file size quickly
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            return {};
        }

        // get file size and allocate buffer
        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        // read entire file into buffer
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        // return SPIR-V binary data
        return buffer;
    }
}   // namespace keplar