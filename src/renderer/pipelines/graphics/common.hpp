#pragma once

#include "../../buffer.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <string_view>
#include <vector>
#include <string>
#include <memory>

///////////////////////////////////////////////////////////////////////////
/////////////////////////////// SHADERS DIR ///////////////////////////////
///////////////////////////////////////////////////////////////////////////
#ifndef NDEBUG
#define FLUX_SHADERS_PATH "build/bin/shaders/graphics/"
#else
#define FLUX_SHADERS_PATH "shaders/graphics/"
#endif

namespace flux::pipelines::graphics
{
    ///////////////////////////////////////////////////////////////////////////
    class common
    {
    public:
        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////// UNIFORM BUFFER TYPES //////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        using ubo_float_type = vk_buffer<float,
                                         vk::BufferUsageFlagBits::eUniformBuffer,
                                         vma::AllocationCreateFlagBits::eHostAccessSequentialWrite>;

        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////// VIRTUAL DTOR ///////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        virtual ~common() {};

        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////////// DELETED COPY //////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        common(const common&)                   = delete;
        common& operator=(const common&)        = delete;

        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////////// DEFAULTED MOVE ////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        common(common&&)                        = default;
        common& operator=(common&&)             = default;

        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////////// BINDING ////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////
        void bind(const vk::raii::CommandBuffer& cmdBuffer) const
        {
            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   *m_pipeline);
        }

        ///////////////////////////////////////////////////////////////////////////
        void bind(const vk::raii::CommandBuffer&    cmdBuffer,
                  uint32_t                          frameIndex) const
        {
            if (!m_descriptorSets.empty())
            {
                cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                             m_layout,
                                             0,
                                             *m_descriptorSets[frameIndex],
                                             nullptr);
            }

            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   *m_pipeline);
        }

    protected:
        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////////// CTOR ///////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        explicit common(std::string_view shaderURL);

        ///////////////////////////////////////////////////////////////////////////
        /////////////////// CHILDREN OVERRIDE BELOW FUNCTION //////////////////////
        ///////////////////////////////////////////////////////////////////////////
        virtual void change() = 0;

        ///////////////////////////////////////////////////////////////////////////
        ////////////// CHILDREN CALL ONE OF THESE TO CREATE PIPELINE //////////////
        ///////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////
        void create(const vk::raii::Device&                 device,
                    const vk::PipelineRenderingCreateInfo&  renderInfo);

        ///////////////////////////////////////////////////////////////////////////
        void create(const vk::raii::Device&                 device,
                    const vk::PipelineRenderingCreateInfo&  renderInfo,
                    const vk::raii::DescriptorPool&         descriptorPool,
                    const std::vector<std::reference_wrapper<ubo_float_type>>&,
                    uint32_t                                framesInFlight);

        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////////// DEFAULTS ///////////////////////////////
        ///////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////
        std::optional<vk::PipelineShaderStageCreateInfo> m_vertexShaderStageInfo{};
        std::optional<vk::PipelineShaderStageCreateInfo> m_fragmentShaderStageInfo{};

        ///////////////////////////////////////////////////////////////////////////
        std::vector<vk::DynamicState> m_dynamicStates       {};

        ///////////////////////////////////////////////////////////////////////////
        vk::PipelineViewportStateCreateInfo m_viewportState
        {
            .viewportCount  = 1,
            .scissorCount   = 1,
        };

        ///////////////////////////////////////////////////////////////////////////
        std::vector<vk::VertexInputBindingDescription> m_vertexInputBindings        {};
        std::vector<vk::VertexInputAttributeDescription> m_vertexAttribDescriptions {};

        ///////////////////////////////////////////////////////////////////////////
        std::vector<vk::DescriptorSetLayoutBinding> m_descriptorBindings {};

        ///////////////////////////////////////////////////////////////////////////
        vk::PipelineInputAssemblyStateCreateInfo m_inputAssembly
        {
            .topology = vk::PrimitiveTopology::eTriangleList,
        };

        ///////////////////////////////////////////////////////////////////////////
        vk::PipelineRasterizationStateCreateInfo m_rasterizer
        {
            .depthClampEnable           = vk::False,
            .rasterizerDiscardEnable    = vk::False,
            .polygonMode                = vk::PolygonMode::eFill,
            .cullMode                   = vk::CullModeFlagBits::eBack,
            .frontFace                  = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable            = vk::False,
            .lineWidth                  = 1.0f,
        };

        ///////////////////////////////////////////////////////////////////////////
        vk::PipelineMultisampleStateCreateInfo m_multisampling
        {
            .rasterizationSamples       = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable        = vk::False,
        };

        ///////////////////////////////////////////////////////////////////////////
        vk::PipelineColorBlendAttachmentState m_colorBlendAttachment
        {
            .blendEnable                = vk::False,
            .colorWriteMask             = vk::ColorComponentFlagBits::eR |
                                          vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |
                                          vk::ColorComponentFlagBits::eA,
        };

        ///////////////////////////////////////////////////////////////////////////
        vk::PipelineColorBlendStateCreateInfo m_colorBlending
        {
            .logicOpEnable              = vk::False, // when True, blendEnable becomes False
            .logicOp                    = vk::LogicOp::eCopy,
            .attachmentCount            = 1,
            .pAttachments               = &m_colorBlendAttachment,
        };

    private:
        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////////// HELPERS ////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////
        void createDescriptorSetLayout(const vk::raii::Device&);

        ///////////////////////////////////////////////////////////////////////////
        void createDescriptorSets(const vk::raii::Device&,
                                  const vk::raii::DescriptorPool&,
                                  uint32_t);

        ///////////////////////////////////////////////////////////////////////////
        void configureDescriptorSets(const vk::raii::Device& device,
                                     const std::vector<std::reference_wrapper<ubo_float_type>>&,
                                     uint32_t);

        ///////////////////////////////////////////////////////////////////////////
        void createPipelineLayout(const vk::raii::Device& device);

        ///////////////////////////////////////////////////////////////////////////
        std::pair<vk::raii::ShaderModule,
                  std::vector<vk::PipelineShaderStageCreateInfo>>
        createShaderStages(const vk::raii::Device&);

        ///////////////////////////////////////////////////////////////////////////
        vk::PipelineDynamicStateCreateInfo createDynamicStateInfo();

        ///////////////////////////////////////////////////////////////////////////
        vk::PipelineVertexInputStateCreateInfo createVertexInputState();

        ///////////////////////////////////////////////////////////////////////////
        void createPipeline(const std::vector<vk::PipelineShaderStageCreateInfo>&,
                            const vk::PipelineDynamicStateCreateInfo&,
                            const vk::PipelineVertexInputStateCreateInfo&,
                            const vk::PipelineRenderingCreateInfo&,
                            const vk::raii::Device&);

        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////// SPIRV BYTECODE /////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        std::string m_bytecodeURL                           {};
        std::vector<char> m_bytecode                        {};

        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////// PIPELINE OBJECTS ////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        vk::raii::DescriptorSetLayout m_descriptorSetLayout { nullptr };
        std::vector<vk::raii::DescriptorSet> m_descriptorSets{};
        vk::raii::PipelineLayout m_layout                   { nullptr };
        vk::raii::Pipeline m_pipeline                       { nullptr };
    };

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////// GRAPHICS SHADERS ARE HERE /////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    enum class name
    {
        basic,
    };

    ///////////////////////////////////////////////////////////////////////////
    std::unique_ptr<common> create(name,
                                   const vk::raii::Device&,
                                   const vk::PipelineRenderingCreateInfo&,
                                   const vk::raii::DescriptorPool&,
                                   const std::vector<std::reference_wrapper<common::ubo_float_type>>&,
                                   uint32_t);
}
