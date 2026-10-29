#include "renderer.hpp"
#include "../common/utils.hpp"
#include "vulkan/vulkan.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstring>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace flux
{
    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////////// CTOR //////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    renderer::renderer([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
    {
        createWindow();
        createInstance();
        createDebugMessenger();
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
    constexpr const char* validationTypeToString(vk::DebugUtilsMessageTypeFlagsEXT type)
    {
        if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral)
        {
            return "general";
        }
        else if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
        {
            return "validation";
        }
        else if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        {
            return "performance";
        }
        else if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding)
        {
            return "device-address-binding";
        }

        return "unknown";
    }

    ///////////////////////////////////////////////////////////////////////////
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback
    (
        vk::DebugUtilsMessageSeverityFlagBitsEXT        severity,
        vk::DebugUtilsMessageTypeFlagsEXT               type,
        const vk::DebugUtilsMessengerCallbackDataEXT*   pCallbackData,
        [[maybe_unused]] void*                          pUserData
    )
    {
        switch (severity)
        {

        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
            spdlog::trace("vk-validation [{}]: {}", validationTypeToString(type),
                                                    pCallbackData->pMessage);
            break;

        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
            spdlog::info("vk-validation [{}]: {}", validationTypeToString(type),
                                                   pCallbackData->pMessage);
            break;

        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            spdlog::warn("vk-validation [{}]: {}", validationTypeToString(type),
                                                   pCallbackData->pMessage);
            break;

        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
            spdlog::error("vk-validation [{}]: {}", validationTypeToString(type),
                                                    pCallbackData->pMessage);
            break;

        }

        spdlog::warn("vulkan validation layer: message = {}", pCallbackData->pMessage);

        return vk::False;
    }

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
            spdlog::info("\t{}", glfwExtensions[i]);
        }

        std::vector<char const*> requiredExtensions{};
        requiredExtensions.reserve(glfwExtensionCount + 1);

        for (uint32_t i{}; i < glfwExtensionCount; ++i)
        {
            requiredExtensions.push_back(glfwExtensions[i]);
        }

#ifndef NDEBUG
        requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif

        spdlog::info("Total number of required extensions: {}", requiredExtensions.size());
        for (uint32_t i{}; i < requiredExtensions.size(); ++i)
        {
            spdlog::info("\t{}", requiredExtensions[i]);
        }

        uint32_t extensionBits{};
        const auto extensionProperties{ m_context.enumerateInstanceExtensionProperties() };

        spdlog::info("Total number of extensions: {}", extensionProperties.size());
        for (const auto& ext : extensionProperties)
        {
            spdlog::info("\t{}", ext.extensionName.cbegin());
        }

        for (uint32_t i{}; i < requiredExtensions.size(); ++i)
        {
            for (const auto& prop : extensionProperties)
            {
                if (!std::strncmp(requiredExtensions[i], prop.extensionName, m_stringMaxLen))
                {
                    extensionBits |= (1U << i);
                }
            }
        }

        if (!utils::isPowerOf2(extensionBits + 1))
        {
            throw std::runtime_error{"renderer::createInstance(): required GLFW extensions not found!"};
        }

        uint32_t validationLayersBits{};
        const std::vector<char const*> validationLayers
        {
#ifndef NDEBUG
            "VK_LAYER_KHRONOS_validation",
#endif
        };

        spdlog::info("Number of required validation layers: {}", validationLayers.size());
        for (const auto& layerName : validationLayers)
        {
            spdlog::info("\t{}", layerName);
        }

        const auto layerProperties{ m_context.enumerateInstanceLayerProperties() };
        spdlog::info("Number of available validation layers: {}", layerProperties.size());

        for (const auto& layerProp : layerProperties)
        {
            spdlog::info("\t{}", layerProp.layerName.cbegin());
        }

        for (size_t i{}; i < validationLayers.size(); ++i)
        {
            for (const auto& layerProp : layerProperties)
            {
                if (!std::strncmp(validationLayers[i], layerProp.layerName, m_stringMaxLen))
                {
                    validationLayersBits |= (1U << i);
                }
            }
        }

        if (validationLayers.size() && !utils::isPowerOf2(validationLayersBits + 1))
        {
            throw std::runtime_error{"renderer::createInstance(): required validation layers not found!"};
        }

        const vk::InstanceCreateInfo createInfo
        {
            .pApplicationInfo           = &appInfo,
            .enabledLayerCount          = static_cast<uint32_t>(validationLayers.size()),
            .ppEnabledLayerNames        = &validationLayers[0],
            .enabledExtensionCount      = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames    = &requiredExtensions[0],
        };

        m_instance = vk::raii::Instance(m_context, createInfo);

        spdlog::info("Created vulkan instance");
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createDebugMessenger()
    {
#ifdef NDEBUG
        return;
#endif

        constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags
        {
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        };

        constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags
        {
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral      |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance  |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation   |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding
        };

        constexpr vk::DebugUtilsMessengerCreateInfoEXT createInfo
        {
            .messageSeverity    = severityFlags,
            .messageType        = messageTypeFlags,
            .pfnUserCallback    = &debugCallback,
        };

        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(createInfo);
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
