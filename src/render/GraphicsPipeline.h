#pragma once

#include <Geometry.h>
#include <Result.h>
#include <filesystem>
#include <volk.h>

namespace spurv {

struct GraphicsPipelineCreateInfo
{
    std::filesystem::path vertexShader;
    std::filesystem::path fragmentShader;

    VkRenderPass renderPass;
    VkPipelineLayout layout;
    VkPrimitiveTopology topology;

    uint32_t vertexBindingDescriptionCount = 0;
    VkVertexInputBindingDescription* pVertexBindingDescriptions = nullptr;

    uint32_t vertexAttributeDescriptionCount = 0;
    VkVertexInputAttributeDescription* pVertexAttributeDescriptions = nullptr;
};

Result<VkPipeline> createGraphicsPipeline(VkDevice device, const GraphicsPipelineCreateInfo& info);

} // namespace spurv
