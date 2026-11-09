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

    ///////////////////////////////////////////////////////////////////////////
    void common::create
    (
        const vk::raii::Device&                                     device,
        const vk::PipelineRenderingCreateInfo&                      renderInfo
    )
    {
        createDescriptorSetLayout(device);
        createPipelineLayout(device);

        const auto [ shaderModule, shaderStages ] = createShaderStages(device);
        const auto dynamicStateInfo   = createDynamicStateInfo();
        const auto vertexInput        = createVertexInputState();

        createPipeline(shaderStages,
                       dynamicStateInfo,
                       vertexInput,
                       renderInfo,
                       device);
    }

    ///////////////////////////////////////////////////////////////////////////
    void common::create
    (
        const vk::raii::Device&                                     device,
        const vk::PipelineRenderingCreateInfo&                      renderInfo,
        const vk::raii::DescriptorPool&                             descriptorPool,
        const std::vector<std::reference_wrapper<ubo_float_type>>&  ubos,
        uint32_t                                                    framesInFlight
    )
    {
        createDescriptorSetLayout(device);
        createDescriptorSets(device, descriptorPool, framesInFlight);
        configureDescriptorSets(device, ubos, framesInFlight);
        createPipelineLayout(device);

        const auto [ shaderModule, shaderStages ] = createShaderStages(device);
        const auto dynamicStateInfo   = createDynamicStateInfo();
        const auto vertexInput        = createVertexInputState();

        createPipeline(shaderStages,
                       dynamicStateInfo,
                       vertexInput,
                       renderInfo,
                       device);
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////////// HELPERS /////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void common::createDescriptorSetLayout(const vk::raii::Device& device)
    {
        const vk::DescriptorSetLayoutCreateInfo descInfo
        {
            .bindingCount   = static_cast<uint32_t>(m_descriptorBindings.size()),
            .pBindings      = m_descriptorBindings.data(),
        };

        m_descriptorSetLayout   = vk::raii::DescriptorSetLayout(device, descInfo);

        spdlog::info("created a descriptor set layout with {} bindings",
                     m_descriptorBindings.size());
    }

    ///////////////////////////////////////////////////////////////////////////
    void common::createDescriptorSets(const vk::raii::Device&           device,
                                      const vk::raii::DescriptorPool&   descriptorPool,
                                      uint32_t                          framesInFlight)
    {
        const std::vector<vk::DescriptorSetLayout> layouts(framesInFlight,
                                                           *m_descriptorSetLayout);

        const vk::DescriptorSetAllocateInfo descriptorSetAllocInfo
        {
            .descriptorPool             = descriptorPool,
            .descriptorSetCount         = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts                = layouts.data(),
        };

        m_descriptorSets = device.allocateDescriptorSets(descriptorSetAllocInfo);

        spdlog::info("created {} descriptor sets", m_descriptorSets.size());
    }

    ///////////////////////////////////////////////////////////////////////////
    void common::configureDescriptorSets
    (
        const vk::raii::Device&                                     device,
        const std::vector<std::reference_wrapper<ubo_float_type>>&  ubos,
        uint32_t                                                    framesInFlight)
    {
        assert(ubos.size() == framesInFlight);

        for (uint32_t i{}; i < framesInFlight; ++i)
        {
            const vk::DescriptorBufferInfo bufferInfo
            {
                .buffer             = ubos[i].get().getBuffer(),
                .offset             = 0,
                .range              = vk::WholeSize,
            };

            const vk::WriteDescriptorSet descriptorWrite
            {
                .dstSet             = m_descriptorSets[i],
                .dstBinding         = 0,
                .dstArrayElement    = 0,
                .descriptorCount    = 1,
                .descriptorType     = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo        = &bufferInfo,
            };

            device.updateDescriptorSets(descriptorWrite, {});
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    void common::createPipelineLayout(const vk::raii::Device& device)
    {
        const vk::PipelineLayoutCreateInfo layoutInfo
        {
            .setLayoutCount             = 1,
            .pSetLayouts                = &*m_descriptorSetLayout,
            .pushConstantRangeCount     = 0,
        };

        m_layout                = vk::raii::PipelineLayout(device, layoutInfo);

        spdlog::info("created a pipeline layout");
    }

    ///////////////////////////////////////////////////////////////////////////
    std::pair<vk::raii::ShaderModule,
              std::vector<vk::PipelineShaderStageCreateInfo>>
    common::createShaderStages
    (
        const vk::raii::Device& device
    )
    {
        const vk::ShaderModuleCreateInfo shaderModuleInfo
        {
            .codeSize       = m_bytecode.size(),
            .pCode          = reinterpret_cast<const uint32_t*>(&m_bytecode[0]),
        };

        auto shaderModule = vk::raii::ShaderModule(device, shaderModuleInfo);

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

        return { std::move(shaderModule), shaderStages };
    }

    ///////////////////////////////////////////////////////////////////////////
    vk::PipelineDynamicStateCreateInfo common::createDynamicStateInfo()
    {
        const vk::PipelineDynamicStateCreateInfo dynamicStateInfo
        {
            .dynamicStateCount          = static_cast<uint32_t>(m_dynamicStates.size()),
            .pDynamicStates             = m_dynamicStates.data(),
        };

        return dynamicStateInfo;
    }

    ///////////////////////////////////////////////////////////////////////////
    vk::PipelineVertexInputStateCreateInfo common::createVertexInputState()
    {
        const vk::PipelineVertexInputStateCreateInfo vertexInput
        {
            .vertexBindingDescriptionCount      = static_cast<uint32_t>(m_vertexInputBindings.size()),
            .pVertexBindingDescriptions         = m_vertexInputBindings.data(),
            .vertexAttributeDescriptionCount    = static_cast<uint32_t>(m_vertexAttribDescriptions.size()),
            .pVertexAttributeDescriptions       = m_vertexAttribDescriptions.data(),
        };

        return vertexInput;
    }

    ///////////////////////////////////////////////////////////////////////////
    void common::createPipeline
    (
        const std::vector<vk::PipelineShaderStageCreateInfo>&   shaderStages,
        const vk::PipelineDynamicStateCreateInfo&               dynamicStateInfo,
        const vk::PipelineVertexInputStateCreateInfo&           vertexInput,
        const vk::PipelineRenderingCreateInfo&                  renderInfo,
        const vk::raii::Device&                                 device
    )
    {
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
                .pStages                = shaderStages.data(),

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
        name                                                                pipelineName,
        const vk::raii::Device&                                             device,
        const vk::PipelineRenderingCreateInfo&                              renderInfo,
        const vk::raii::DescriptorPool&                                     descriptorPool,
        const std::vector<std::reference_wrapper<common::ubo_float_type>>&  ubos,
        uint32_t                                                            framesInFlight
    )
    {
        switch (pipelineName)
        {
        case name::basic:
            return std::unique_ptr<common>{new basic{device,
                                                     renderInfo,
                                                     descriptorPool,
                                                     ubos,
                                                     framesInFlight}};
        default:
            throw std::runtime_error{"unspecified graphics pipeline!"};
        }
    }
}
