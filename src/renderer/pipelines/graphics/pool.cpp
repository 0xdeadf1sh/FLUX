#include "pool.hpp"

#include <spdlog/spdlog.h>

namespace flux::pipelines::graphics
{
    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////// INITIALIZATION //////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    void pool::init
    (
        const vk::raii::Device&                                     device,
        const vk::PipelineRenderingCreateInfo&                      renderInfo,
        const vk::raii::DescriptorPool&                             descriptorPool,
        const std::vector<std::reference_wrapper<ubo_float_type>>&  ubos,
        uint32_t                                                    framesInFlight
    )
    {
        m_pipelines.reserve(1);

        m_pipelines.push_back
        ({
            create(name::basic,
                   device,
                   renderInfo,
                   descriptorPool,
                   ubos,
                   framesInFlight)
        });

        spdlog::info("graphics pipeline pool: created {} pipelines", m_pipelines.size());
    }

    ///////////////////////////////////////////////////////////////////////////
    ////////////////////////////////// BINDING ////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void pool::bind(name                            pipelineName,
                    [[maybe_unused]]
                    const vk::raii::CommandBuffer&  cmdBuffer) const
    {
        switch (pipelineName)
        {
        case name::basic:
            throw std::runtime_error{"'basic' pipeline requires frameIndex"};
        default:
            throw std::runtime_error{"unspecified graphics pipeline!"};
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    void pool::bind(name                            pipelineName,
                    const vk::raii::CommandBuffer&  cmdBuffer,
                    uint32_t                        frameIndex) const
    {
        switch (pipelineName)
        {
        case name::basic:
            m_pipelines[0]->bind(cmdBuffer, frameIndex);
            break;
        default:
            throw std::runtime_error{"unspecified graphics pipeline!"};
        }
    }
}
