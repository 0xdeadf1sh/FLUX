#include "basic.hpp"
#include "common.hpp"

#include "../../../common/utils.hpp"

#include <spdlog/spdlog.h>

namespace flux::pipelines::graphics
{
    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////////// CTOR ////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    common::common(std::string_view shaderURL)
        : m_bytecodeURL     { shaderURL }
        , m_bytecode        { utils::readBinaryFile(shaderURL) }
    {
        spdlog::info("read {} bytes of spirv from '{}'", m_bytecode.size(),
                                                         shaderURL);
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////// PIPELINE CREATION /////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    void common::create(const vk::raii::Device&                 device,
                        const vk::PipelineRenderingCreateInfo&  renderInfo)
    {
        m_layout = vk::raii::PipelineLayout(device, m_layoutInfo);

        const vk::ShaderModuleCreateInfo shaderModuleInfo
        {
            .codeSize       = m_bytecode.size(),
            .pCode          = reinterpret_cast<const uint32_t*>(&m_bytecode[0]),
        };

        const auto shaderModule = vk::raii::ShaderModule(device, shaderModuleInfo);

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{};

        if (m_vertexShaderStageInfo.has_value())
        {
            m_vertexShaderStageInfo->module = shaderModule;
            shaderStages.push_back(*m_vertexShaderStageInfo);
        }

        if (m_fragmentShaderStageInfo.has_value())
        {
            m_fragmentShaderStageInfo->module = shaderModule;
            shaderStages.push_back(*m_fragmentShaderStageInfo);
        }

        const vk::PipelineDynamicStateCreateInfo dynamicStateInfo
        {
            .dynamicStateCount          = static_cast<uint32_t>(m_dynamicStates.size()),
            .pDynamicStates             = &m_dynamicStates[0],
        };

        const vk::PipelineVertexInputStateCreateInfo vertexInput
        {
            .vertexBindingDescriptionCount      = static_cast<uint32_t>(m_vertexInputBindings.size()),
            .pVertexBindingDescriptions         = &m_vertexInputBindings[0],
            .vertexAttributeDescriptionCount    = static_cast<uint32_t>(m_vertexAttribDescriptions.size()),
            .pVertexAttributeDescriptions       = &m_vertexAttribDescriptions[0],
        };

        const vk::StructureChain
        <
            vk::GraphicsPipelineCreateInfo,
            vk::PipelineRenderingCreateInfo
        >
        createInfo
        {
            // graphics pipeline
            {
                // programmable stages
                .stageCount             = static_cast<uint32_t>(shaderStages.size()),
                .pStages                = &shaderStages[0],

                // fixed-function stages
                .pVertexInputState      = &vertexInput,
                .pInputAssemblyState    = &m_inputAssembly,
                .pViewportState         = &m_viewportState,
                .pRasterizationState    = &m_rasterizer,
                .pMultisampleState      = &m_multisampling,
                .pColorBlendState       = &m_colorBlending,
                .pDynamicState          = &dynamicStateInfo,

                // pipeline layout
                .layout                 = m_layout,

                // no renderpass needed
                .renderPass             = nullptr,

                // parent pipeline
                .basePipelineHandle     = nullptr,
                .basePipelineIndex      = 0,
                // .flags               = vk::PipelineCreateFlagBits::eDerivative,
            },

            // dynamic rendering
            renderInfo,
        };

        const vk::raii::PipelineCache cache{ nullptr };

        m_pipeline = vk::raii::Pipeline(device,
                                        cache,
                                        createInfo.get<vk::GraphicsPipelineCreateInfo>());

        spdlog::info("created a graphics pipeline for '{}'", m_bytecodeURL);
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////// FACTORY METHOD //////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    std::unique_ptr<common> create
    (
        name                                    pipelineName,
        const vk::raii::Device&                 device,
        const vk::PipelineRenderingCreateInfo&  renderInfo
    )
    {
        switch (pipelineName)
        {
        case name::basic:
            return std::unique_ptr<common>{new basic{device, renderInfo}};
        default:
            throw std::runtime_error{"unspecified graphics pipeline!"};
        }
    }
}
