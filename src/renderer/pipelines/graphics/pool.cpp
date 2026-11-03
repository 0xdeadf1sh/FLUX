#include "pool.hpp"

#include <spdlog/spdlog.h>

namespace flux::pipelines::graphics
{
    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////// INITIALIZATION //////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    void pool::init(const vk::raii::Device&                 device,
                    const vk::PipelineRenderingCreateInfo&  renderInfo)
    {
        m_pipelines.reserve(1);

        m_pipelines.push_back
        ({
            create(name::basic, device, renderInfo)
        });

        spdlog::info("graphics pipeline pool: created {} pipelines", m_pipelines.size());
    }

    ///////////////////////////////////////////////////////////////////////////
    ////////////////////////////////// BINDING ////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    void pool::bind(name pipelineName, const vk::raii::CommandBuffer& cmdBuffer) const
    {
        switch (pipelineName)
        {
        case name::basic:
            m_pipelines[0]->bind(cmdBuffer);
            break;
        default:
            throw std::runtime_error{"unspecified graphics pipeline!"};
        }
    }
}
