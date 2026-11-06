#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <functional>
#include <type_traits>

namespace flux
{
    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    class buffer
    {
    public:
        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////////// CTOR ////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        buffer(vma::raii::Allocator&            allocator,
               const vk::raii::CommandPool&     cmdPool,
               const vk::raii::Device&          device,
               const vk::raii::Queue&           queue,
               vk::BufferUsageFlags             usageFlags,
               vma::AllocationCreateFlags       allocFlags,
               const std::vector<T>&            data)

            : m_bufferSizeInBytes               { sizeof(T) * data.size() }
            , m_usageFlags                      { usageFlags }
            , m_allocFlags                      { allocFlags }
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
        buffer(const buffer&);
        buffer& operator=(const buffer&);

        ///////////////////////////////////////////////////////////////////////////
        //////////////////////////////// BINDING //////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        void bind(const vk::raii::CommandBuffer& cmdBuffer) const
        {
            if (isVertexBuffer())
            {
                cmdBuffer.bindVertexBuffers(0, *m_buffer, { 0 });
            }
            else if (isIndexBuffer())
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
        bool isVertexBuffer() const
        {
            return (m_usageFlags & vk::BufferUsageFlagBits::eVertexBuffer) ==
                                   vk::BufferUsageFlagBits::eVertexBuffer;
        }

        ///////////////////////////////////////////////////////////////////////////
        bool isIndexBuffer() const
        {
            return (m_usageFlags & vk::BufferUsageFlagBits::eIndexBuffer) ==
                                   vk::BufferUsageFlagBits::eIndexBuffer;
        }

        ///////////////////////////////////////////////////////////////////////////
        bool isUniformBuffer() const
        {
            return (m_usageFlags & vk::BufferUsageFlagBits::eUniformBuffer) ==
                                   vk::BufferUsageFlagBits::eUniformBuffer;
        }

        ///////////////////////////////////////////////////////////////////////////
        vk::IndexType getIndexType() const
        {
            assert(isIndexBuffer());

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
                // thanks to assert() above this line won't get executed,
                // but we still need it to satisfy the compiler
                return vk::IndexType::eUint32;
            }
        }

        ///////////////////////////////////////////////////////////////////////////
        bool isDeviceLocal() const
        {
            return isVertexBuffer() || isIndexBuffer();
        }

    private:
        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////////// FILL WITH DATA //////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        vma::raii::Buffer create(vk::BufferUsageFlags, vma::AllocationCreateFlags);
        void fill(const std::vector<T>&);

        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////////// COPYING DATA //////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        void copy(const vma::raii::Buffer& dstBuffer,
                  const vma::raii::Buffer& srcBuffer) const;

        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////////// VMA ////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        const vk::DeviceSize m_bufferSizeInBytes                {};
        const vk::BufferUsageFlags m_usageFlags                 {};
        const vma::AllocationCreateFlags m_allocFlags           {};
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
    //////////////////////////////////// COPY /////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    buffer<T>::buffer(const buffer<T>& other)
        : m_bufferSizeInBytes                   { other.m_bufferSizeInBytes }
        , m_usageFlags                          { (other.m_usageFlags & ~vk::BufferUsageFlagBits::eTransferSrc) |
                                                                         vk::BufferUsageFlagBits::eTransferDst }
        , m_allocFlags                          { other.m_allocFlags }
        , m_allocator                           { other.m_allocator }
        , m_cmdPool                             { other.m_cmdPool }
        , m_device                              { other.m_device }
        , m_queue                               { other.m_queue }
    {
        assert((m_usageFlags        & vk::BufferUsageFlagBits::eTransferDst) ==
                                      vk::BufferUsageFlagBits::eTransferDst);

        assert((other.m_usageFlags  & vk::BufferUsageFlagBits::eTransferSrc) ==
                                      vk::BufferUsageFlagBits::eTransferSrc);

        m_buffer = create(m_usageFlags, m_allocFlags);
        copy(m_buffer, other.m_buffer);
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    buffer<T>& buffer<T>::operator=(const buffer<T>& other)
    {
        assert((m_usageFlags        & vk::BufferUsageFlagBits::eTransferDst) ==
                                      vk::BufferUsageFlagBits::eTransferDst);

        assert((other.m_usageFlags  & vk::BufferUsageFlagBits::eTransferSrc) ==
                                      vk::BufferUsageFlagBits::eTransferSrc);

        assert(m_bufferSizeInBytes == other.m_bufferSizeInBytes);

        if (this == &other)
        {
            return *this;
        }

        m_buffer = create(m_usageFlags, m_allocFlags);
        copy(m_buffer, other.m_buffer);

        return *this;
    }

    ///////////////////////////////////////////////////////////////////////////
    ////////////////////////////// FILL WITH DATA /////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    vma::raii::Buffer buffer<T>::create(vk::BufferUsageFlags        usageFlags,
                                        vma::AllocationCreateFlags  allocFlags)
    {
        const vk::BufferCreateInfo bufferInfo
        {
            .size               = m_bufferSizeInBytes,
            .usage              = usageFlags,
            .sharingMode        = vk::SharingMode::eExclusive,
        };

        const vma::AllocationCreateInfo allocInfo
        {
            allocFlags,
            vma::MemoryUsage::eAuto,
        };

        return m_allocator.get().createBuffer(bufferInfo, allocInfo);
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    void buffer<T>::fill(const std::vector<T>& data)
    {
        m_buffer = create(m_usageFlags, m_allocFlags);

        if (isDeviceLocal())
        {
            assert((m_usageFlags & vk::BufferUsageFlagBits::eTransferDst) ==
                                   vk::BufferUsageFlagBits::eTransferDst);

            buffer<T> stagingBuffer{ m_allocator,
                                     m_cmdPool,
                                     m_device,
                                     m_queue,
                                     vk::BufferUsageFlagBits::eTransferSrc,
                                     vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                                     data };

            *this = stagingBuffer;

            spdlog::info("copied {} bytes from CPU to GPU buffer", m_bufferSizeInBytes);
        }
        else
        {
            assert((m_usageFlags & vk::BufferUsageFlagBits::eTransferSrc) ==
                                   vk::BufferUsageFlagBits::eTransferSrc);

            assert((m_allocFlags & vma::AllocationCreateFlagBits::eHostAccessSequentialWrite) ==
                                   vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

            const vma::raii::Allocation& alloc{ m_buffer.getAllocation() };
            alloc.copyFromMemory(&data[0], {}, m_bufferSizeInBytes);

            spdlog::info("copied {} bytes from RAM to CPU buffer", m_bufferSizeInBytes);
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////// COPYING DATA //////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    void buffer<T>::copy(const vma::raii::Buffer& dstBuffer,
                         const vma::raii::Buffer& srcBuffer) const
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
