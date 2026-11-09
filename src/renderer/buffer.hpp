#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <functional>
#include <type_traits>
#include <span>
#include <cassert>

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

        /// NOTE: different flags mean a different type, so we need a friend
        /// declaration to grant access across instantiations
        template
        <
            typename,
            vk::BufferUsageFlags,
            vma::AllocationCreateFlags,
            vma::MemoryUsage
        >
        friend class buffer;

        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////////// CONSTANTS /////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        static constexpr vk::BufferUsageFlags m_usageFlags          { UsageFlags };
        static constexpr vma::AllocationCreateFlags m_allocFlags    { AllocFlags };
        static constexpr vma::MemoryUsage m_memoryUsage             { MemoryUsage };

        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////////// CTOR ////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        buffer(vma::raii::Allocator&            allocator,
               const vk::raii::CommandPool&     cmdPool,
               const vk::raii::Device&          device,
               const vk::raii::Queue&           queue,
               std::span<const T>               data)

            : m_bufferSizeInBytes               { data.size_bytes() }
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
        ///////////////////////////// DEFAULTED MOVE //////////////////////////////
        /////////////////////////////////////////////////////////////////////////// 
        buffer(buffer&&)                        = default;
        buffer& operator=(buffer&&)             = default;

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
            assert(*other.m_buffer);

            static_assert(isTransferDestinationBuffer(),
                          "copied-to buffer must be destination buffer!");

            static_assert(other.isTransferSourceBuffer(),
                          "copied-from buffer must be source buffer!");

            m_buffer = create();
            copy(m_buffer, other.m_buffer);
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
            assert(*other.m_buffer);
            assert(m_bufferSizeInBytes == other.m_bufferSizeInBytes);

            static_assert(isTransferDestinationBuffer(),
                          "copied-to buffer must be destination buffer!");

            static_assert(other.isTransferSourceBuffer(),
                          "copied-from buffer must be source buffer!");

            if (!*m_buffer)
            {
                m_buffer = create();
            }

            copy(m_buffer, other.m_buffer);

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
                static_assert(false, "bind isn't defined for this type of buffer!");
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
            else
            {
                static_assert(false, "index buffers aren't defined for this type!");
                return {};
            }
        }

        ///////////////////////////////////////////////////////////////////////////
        constexpr bool isDeviceLocal() const
        {
            return isVertexBuffer() || isIndexBuffer();
        }

    private:
        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////////// FILLING /////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////
        vma::raii::Buffer create();

        ///////////////////////////////////////////////////////////////////////////
        void fill(std::span<const T> data)
        {
            assert(!data.empty());

            m_buffer = create();

            if constexpr (isDeviceLocal())
            {
                static_assert(isTransferDestinationBuffer(),
                              "copied-to buffer must be a destination buffer!");

                buffer
                <
                    T,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vma::AllocationCreateFlagBits::eHostAccessSequentialWrite
                >
                stagingBuffer
                {
                    m_allocator,
                    m_cmdPool,
                    m_device,
                    m_queue,
                    data
                };

                copy(m_buffer, stagingBuffer.m_buffer);

                spdlog::info("copied {} bytes from CPU to GPU buffer", m_bufferSizeInBytes);
            }
            else
            {
                static_assert(isTransferSourceBuffer(),
                              "copied-from buffer must be a source buffer!");

                static_assert(isMappableBuffer(),
                              "copied-from buffer must be a mappable buffer!");

                const vma::raii::Allocation& alloc{ m_buffer.getAllocation() };
                alloc.copyFromMemory(data.data(), {}, m_bufferSizeInBytes);

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
        vk::DeviceSize m_bufferSizeInBytes                      {};
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
            MemoryUsage,
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
