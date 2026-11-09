#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <functional>
#include <type_traits>
#include <ranges>

namespace flux
{
    ///////////////////////////////////////////////////////////////////////////
    template <typename                          T,
              vk::BufferUsageFlags              UsageFlags,
              vma::AllocationCreateFlags        AllocFlags  = {},
              vma::MemoryUsage                  MemoryUsage = vma::MemoryUsage::eAuto>
    class buffer
    {
    public:
        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////////// CONSTANTS /////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        static constexpr vk::BufferUsageFlags m_usageFlags          { UsageFlags };
        static constexpr vma::AllocationCreateFlags m_allocFlags    { AllocFlags };
        static constexpr vma::MemoryUsage m_memoryUsage             { MemoryUsage };

        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////////// CTOR ////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        template <std::ranges::input_range      InputRange>
        buffer(vma::raii::Allocator&            allocator,
               const vk::raii::CommandPool&     cmdPool,
               const vk::raii::Device&          device,
               const vk::raii::Queue&           queue,
               const InputRange&                data)

            : m_bufferSizeInBytes               { sizeof(T) * data.size() }
            , m_allocator                       { allocator }
            , m_cmdPool                         { cmdPool }
            , m_device                          { device }
            , m_queue                           { queue }
        {
            fill(data);
        }

        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////////// DTOR ////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        ~buffer()                               {};

        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////// DELETED MOVE ////////////////////////////////
        /////////////////////////////////////////////////////////////////////////// 
        buffer(buffer&&)                        = delete;
        buffer& operator=(buffer&&)             = delete;

        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////// CUSTOM COPY ////////////////////////////////
        /////////////////////////////////////////////////////////////////////////// 

        ///////////////////////////////////////////////////////////////////////////
        template <vk::BufferUsageFlags          OtherUsageFlags,
                  vma::AllocationCreateFlags    OtherAllocFlags,
                  vma::MemoryUsage              OtherMemoryUsage>

        buffer(const buffer<T,
                            OtherUsageFlags,
                            OtherAllocFlags,
                            OtherMemoryUsage>&  other)

            : m_bufferSizeInBytes               { other.m_bufferSizeInBytes }
            , m_allocator                       { other.m_allocator }
            , m_cmdPool                         { other.m_cmdPool }
            , m_device                          { other.m_device }
            , m_queue                           { other.m_queue }
        {
            static_assert(isTransferDestinationBuffer(),
                          "copied-to buffer must be destination buffer!");

            static_assert(other.isTransferSourceBuffer(),
                          "copied-from buffer must be source buffer!");

            m_buffer = create();
            copy(m_buffer, other.getBuffer());
        }

        ///////////////////////////////////////////////////////////////////////////
        template <vk::BufferUsageFlags          OtherUsageFlags,
                  vma::AllocationCreateFlags    OtherAllocFlags,
                  vma::MemoryUsage              OtherMemoryUsage>

        buffer& operator=(const buffer<T,
                                       OtherUsageFlags,
                                       OtherAllocFlags,
                                       OtherMemoryUsage>& other)
        {
            static_assert(isTransferDestinationBuffer(),
                          "copied-to buffer must be destination buffer!");

            static_assert(other.isTransferSourceBuffer(),
                          "copied-from buffer must be source buffer!");

            assert(m_bufferSizeInBytes == other.getBufferSizeInBytes());

            m_buffer = create();
            copy(m_buffer, other.getBuffer());

            return *this;
        }

        ///////////////////////////////////////////////////////////////////////////
        //////////////////////////////// BINDING //////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        void bind(const vk::raii::CommandBuffer& cmdBuffer) const
        {
            if constexpr (isVertexBuffer())
            {
                cmdBuffer.bindVertexBuffers(0, *m_buffer, { 0 });
            }
            else if constexpr (isIndexBuffer())
            {
                cmdBuffer.bindIndexBuffer(*m_buffer, 0, getIndexType());
            }
            else
            {
                throw std::runtime_error{"bind is not implemented for this buffer type!"};
            }
        }

        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////////// GETTERS /////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////
        constexpr bool isVertexBuffer() const
        {
            return (m_usageFlags & vk::BufferUsageFlagBits::eVertexBuffer) ==
                                   vk::BufferUsageFlagBits::eVertexBuffer;
        }

        ///////////////////////////////////////////////////////////////////////////
        constexpr bool isIndexBuffer() const
        {
            return (m_usageFlags & vk::BufferUsageFlagBits::eIndexBuffer) ==
                                   vk::BufferUsageFlagBits::eIndexBuffer;
        }

        ///////////////////////////////////////////////////////////////////////////
        constexpr bool isUniformBuffer() const
        {
            return (m_usageFlags & vk::BufferUsageFlagBits::eUniformBuffer) ==
                                   vk::BufferUsageFlagBits::eUniformBuffer;
        }

        ///////////////////////////////////////////////////////////////////////////
        constexpr bool isTransferDestinationBuffer() const
        {
            return (m_usageFlags & vk::BufferUsageFlagBits::eTransferDst) ==
                                   vk::BufferUsageFlagBits::eTransferDst;
        }

        ///////////////////////////////////////////////////////////////////////////
        constexpr bool isTransferSourceBuffer() const
        {
            return (m_usageFlags & vk::BufferUsageFlagBits::eTransferSrc) ==
                                   vk::BufferUsageFlagBits::eTransferSrc;
        }

        ///////////////////////////////////////////////////////////////////////////
        constexpr bool isMappableBuffer() const
        {
            return (m_allocFlags & vma::AllocationCreateFlagBits::eHostAccessSequentialWrite) ==
                                   vma::AllocationCreateFlagBits::eHostAccessSequentialWrite;
        }

        ///////////////////////////////////////////////////////////////////////////
        constexpr vk::IndexType getIndexType() const
        {
            static_assert(isIndexBuffer(),
                          "getIndexType() is only defined for index buffers!");

            if constexpr (std::is_same_v<T, uint16_t>)
            {
                return vk::IndexType::eUint16;
            }
            else if constexpr (std::is_same_v<T, uint32_t>)
            {
                return vk::IndexType::eUint32;
            }

            return vk::IndexType::eNoneKHR;
        }

        ///////////////////////////////////////////////////////////////////////////
        constexpr bool isDeviceLocal() const
        {
            return isVertexBuffer() || isIndexBuffer();
        }

        ///////////////////////////////////////////////////////////////////////////
        vk::DeviceSize getBufferSizeInBytes() const
        {
            return m_bufferSizeInBytes;
        }

        ///////////////////////////////////////////////////////////////////////////
        const vma::raii::Buffer& getBuffer() const
        {
            return m_buffer;
        }

    private:
        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////////// FILLING /////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////
        vma::raii::Buffer create();

        ///////////////////////////////////////////////////////////////////////////
        template <std::ranges::input_range InputRange>
        void fill(const InputRange& data)
        {
            m_buffer = create();

            if constexpr (isDeviceLocal())
            {
                static_assert(isTransferDestinationBuffer(),
                              "copied-to buffer must be a destination buffer!");

                buffer<T,
                       vk::BufferUsageFlagBits::eTransferSrc,
                       vma::AllocationCreateFlagBits::eHostAccessSequentialWrite>
                stagingBuffer
                {
                    m_allocator,
                    m_cmdPool,
                    m_device,
                    m_queue,
                    data
                };

                *this = stagingBuffer;

                spdlog::info("copied {} bytes from CPU to GPU buffer", m_bufferSizeInBytes);
            }
            else
            {
                static_assert(isTransferSourceBuffer(),
                              "copied-from buffer must be a source buffer!");

                static_assert(isMappableBuffer(),
                              "copied-from buffer must be a mappable buffer!");

                const vma::raii::Allocation& alloc{ m_buffer.getAllocation() };
                alloc.copyFromMemory(&data[0], {}, m_bufferSizeInBytes);

                spdlog::info("copied {} bytes from RAM to CPU buffer", m_bufferSizeInBytes);
            }
        }

        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////////// COPYING DATA //////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        void copy(const vma::raii::Buffer& dstBuffer,
                  const vma::raii::Buffer& srcBuffer) const;

        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////////// VMA ////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        const vk::DeviceSize m_bufferSizeInBytes                {};
        vma::raii::Buffer m_buffer                              { nullptr };

        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////////// REFERENCES ////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        std::reference_wrapper<vma::raii::Allocator>            m_allocator;
        std::reference_wrapper<const vk::raii::CommandPool>     m_cmdPool;
        std::reference_wrapper<const vk::raii::Device>          m_device;
        std::reference_wrapper<const vk::raii::Queue>           m_queue;
    };

    ///////////////////////////////////////////////////////////////////////////
    ////////////////////////////// FILL WITH DATA /////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    template <typename                      T,
              vk::BufferUsageFlags          UsageFlags,
              vma::AllocationCreateFlags    AllocFlags,
              vma::MemoryUsage              MemoryUsage>

    vma::raii::Buffer buffer<T, UsageFlags, AllocFlags, MemoryUsage>::create()
    {
        const vk::BufferCreateInfo bufferInfo
        {
            .size               = m_bufferSizeInBytes,
            .usage              = UsageFlags,
            .sharingMode        = vk::SharingMode::eExclusive,
        };

        constexpr vma::AllocationCreateInfo allocInfo
        {
            AllocFlags,
            vma::MemoryUsage::eAuto,
        };

        return m_allocator.get().createBuffer(bufferInfo, allocInfo);
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////// COPYING DATA //////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    template <typename                          T,
              vk::BufferUsageFlags              UsageFlags,
              vma::AllocationCreateFlags        AllocFlags,
              vma::MemoryUsage                  MemoryUsage>

    void buffer<T, UsageFlags, AllocFlags, MemoryUsage>::copy
    (
        const vma::raii::Buffer& dstBuffer,
        const vma::raii::Buffer& srcBuffer
    )
    const
    {
        const vk::CommandBufferAllocateInfo allocInfo
        {
            .commandPool            = m_cmdPool.get(),
            .level                  = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount     = 1,
        };

        const vk::raii::CommandBuffer cmdBuffer
        {
            std::move(m_device.get().allocateCommandBuffers(allocInfo).front() )
        };

        cmdBuffer.begin
        ({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        });

        cmdBuffer.copyBuffer(*srcBuffer,
                             *dstBuffer,
                             vk::BufferCopy(0, 0, m_bufferSizeInBytes));

        cmdBuffer.end();

        const vk::SubmitInfo submitInfo
        {
            .commandBufferCount         = 1,
            .pCommandBuffers            = &*cmdBuffer,
        };

        m_queue.get().submit(submitInfo, nullptr);
        m_queue.get().waitIdle();
    }
}
