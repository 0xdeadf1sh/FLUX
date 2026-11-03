#include "basic.hpp"

namespace flux::pipelines::graphics
{
    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////////// CTOR //////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    basic::basic(const vk::raii::Device&                device,
                 const vk::PipelineRenderingCreateInfo& renderInfo)
        : common(FLUX_SHADERS_PATH "basic.slang.spv")
    {
        change();
        create(device, renderInfo);
    }

    ///////////////////////////////////////////////////////////////////////////
    //////////////////////////// CHANGES TO DEFAULTS //////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    void basic::change()
    {
        m_vertexShaderStageInfo =
        {
            .stage                  = vk::ShaderStageFlagBits::eVertex,
            .pName                  = "vmain",
            .pSpecializationInfo    = nullptr,
        };

        m_fragmentShaderStageInfo =
        {
            .stage                  = vk::ShaderStageFlagBits::eFragment,
            .pName                  = "fmain",
            .pSpecializationInfo    = nullptr,
        };

        m_dynamicStates.push_back(vk::DynamicState::eViewport);
        m_dynamicStates.push_back(vk::DynamicState::eScissor);

        m_vertexInputBindings.push_back
        ({
            .binding        = 0,
            .stride         = 6 * sizeof(float),
            .inputRate      = vk::VertexInputRate::eVertex,
        });

        m_vertexAttribDescriptions.push_back
        ({
            .location       = 0,
            .binding        = 0,
            .format         = vk::Format::eR32G32B32Sfloat,
            .offset         = 0,
        });

        m_vertexAttribDescriptions.push_back
        ({
            .location       = 1,
            .binding        = 0,
            .format         = vk::Format::eR32G32B32Sfloat,
            .offset         = 3 * sizeof(float),
        });
    }
}
