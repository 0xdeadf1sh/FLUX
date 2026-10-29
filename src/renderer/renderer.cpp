#include "renderer.hpp"
#include "../common/utils.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstring>
#include <print>

namespace flux
{
    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////////// CTOR //////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    renderer::renderer([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
    {
        createWindow();
        createInstance();
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////////// DTOR //////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    renderer::~renderer()
    {
        destroyWindow();
    }

    ///////////////////////////////////////////////////////////////////////////
    //////////////////////////////// WINDOWING ////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createWindow()
    {
        if (!glfwInit())
        {
            throw std::runtime_error{"renderer::createWindow(): glfwInit() failed!"};
        }

        spdlog::info("Initialized GLFW: {}", glfwGetVersionString());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        if (!(m_window = glfwCreateWindow(m_windowWidth,
                                          m_windowHeight,
                                          m_windowTitle,
                                          nullptr, nullptr)))
        {
            throw std::runtime_error{"renderer::createWindow(): glfwCreateWindow() failed!"};
        }

        spdlog::info("Opened a GLFW window: w = {}, h = {}", m_windowWidth,
                                                             m_windowHeight);
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////// INITIALIZATION //////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createInstance()
    {
        constexpr vk::ApplicationInfo appInfo
        {
            .pApplicationName   = m_rendererName,
            .applicationVersion = m_rendererVersion,
            .pEngineName        = m_rendererName,
            .engineVersion      = m_rendererVersion,
            .apiVersion         = vk::ApiVersion14,
        };

        uint32_t glfwExtensionCount{};
        const auto glfwExtensions{ glfwGetRequiredInstanceExtensions(&glfwExtensionCount) };

        if (!glfwExtensionCount)
        {
            throw std::runtime_error{"renderer::createInstance(): no GLFW extensions found!"};
        }

        spdlog::info("Number of required GLFW extensions: {}", glfwExtensionCount);
        for (uint32_t i{}; i < glfwExtensionCount; ++i)
        {
            spdlog::info("\tGLFW-required extension: {}", glfwExtensions[i]);
        }

        uint32_t glfwExtensionBits{};
        const auto extensionProperties{ m_context.enumerateInstanceExtensionProperties() };

        spdlog::info("Total number of Vulkan instance extensions: {}", extensionProperties.size());
        for (const auto& ext : extensionProperties)
        {
            spdlog::info("\tInstance extension: {}", ext.extensionName.cbegin());
        }

        for (uint32_t i{}; i < glfwExtensionCount; ++i)
        {
            for (const auto& prop : extensionProperties)
            {
                if (!std::strncmp(glfwExtensions[i], prop.extensionName, m_stringMaxLen))
                {
                    glfwExtensionBits |= (1U << i);
                }
            }
        }

        if (!utils::isPowerOf2(glfwExtensionBits + 1))
        {
            throw std::runtime_error{"renderer::createInstance(): some GLFW extensions not found!"};
        }

        const vk::InstanceCreateInfo createInfo
        {
            .pApplicationInfo           = &appInfo,
            .enabledExtensionCount      = glfwExtensionCount,
            .ppEnabledExtensionNames    = glfwExtensions,
        };

        m_instance = vk::raii::Instance(m_context, createInfo);

        spdlog::info("Created vulkan instance");
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////// RENDER LOOP ///////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::loop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////////// CLEANUP /////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::destroyWindow()
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}
