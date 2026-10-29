#include "renderer.hpp"
#include "../common/utils.hpp"

#include "vulkan/vulkan.hpp"
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstring>
#include <vector>
#include <optional>

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
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
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
            throw std::runtime_error{"glfwInit() failed!"};
        }

        spdlog::info("initialized GLFW: {}", glfwGetVersionString());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        if (!(m_window = glfwCreateWindow(m_windowWidth,
                                          m_windowHeight,
                                          m_windowTitle,
                                          nullptr, nullptr)))
        {
            throw std::runtime_error{"glfwCreateWindow() failed!"};
        }

        spdlog::info("opened a GLFW window: w = {}, h = {}", m_windowWidth,
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
            throw std::runtime_error{"no GLFW extensions found!"};
        }

        spdlog::info("number of required GLFW extensions: {}", glfwExtensionCount);
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

        spdlog::info("total number of required extensions: {}", requiredExtensions.size());
        for (uint32_t i{}; i < requiredExtensions.size(); ++i)
        {
            spdlog::info("\t{}", requiredExtensions[i]);
        }

        uint32_t extensionBits{};
        const auto extensionProperties{ m_context.enumerateInstanceExtensionProperties() };

        spdlog::info("total number of extensions: {}", extensionProperties.size());
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
                    break;
                }
            }
        }

        if (utils::countBits(extensionBits) != requiredExtensions.size())
        {
            throw std::runtime_error{"required extensions not found!"};
        }

        uint32_t validationLayersBits{};
        const std::vector<char const*> validationLayers
        {
#ifndef NDEBUG
            "VK_LAYER_KHRONOS_validation",
#endif
        };

        spdlog::info("number of required validation layers: {}", validationLayers.size());
        for (const auto& layerName : validationLayers)
        {
            spdlog::info("\t{}", layerName);
        }

        const auto layerProperties{ m_context.enumerateInstanceLayerProperties() };
        spdlog::info("number of available validation layers: {}", layerProperties.size());

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
                    break;
                }
            }
        }

        if (utils::countBits(validationLayersBits) != validationLayers.size())
        {
            throw std::runtime_error{"required validation layers not found!"};
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

        spdlog::info("created vulkan instance");
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

        spdlog::info("created debug messenger for validation layers");
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createSurface()
    {
        VkSurfaceKHR surface{};

        if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &surface))
        {
            throw std::runtime_error{"failed to create window surface!"};
        }

        m_surface = vk::raii::SurfaceKHR(m_instance, surface);

        spdlog::info("created window surface");
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::pickPhysicalDevice()
    {
        const auto physicalDevices{ m_instance.enumeratePhysicalDevices() };

        if (physicalDevices.empty())
        {
            throw std::runtime_error{"no physical vulkan device found!"};
        }

        for (auto physicalDevice : physicalDevices)
        {
            const auto deviceProperties{ physicalDevice.getProperties() };

            if (deviceProperties.apiVersion < vk::ApiVersion14 ||
                deviceProperties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu)
            {
                continue;
            }

            const auto queueFamilies{ physicalDevice.getQueueFamilyProperties() };

            bool foundGraphicsFamily{};

            for (const auto family : queueFamilies)
            {
                if (family.queueFlags & vk::QueueFlagBits::eGraphics)
                {
                    foundGraphicsFamily = true;
                    break;
                }
            }

            if (!foundGraphicsFamily)
            {
                continue;
            }

            const std::vector<const char*> requiredExtensions
            {
                vk::KHRSwapchainExtensionName,
            };

            const auto availableExtensions{ physicalDevice.enumerateDeviceExtensionProperties() };
            if (availableExtensions.empty())
            {
                continue;
            }

            uint32_t requiredExtensionsBits{};

            for (size_t i{}; i < requiredExtensions.size(); ++i)
            {
                for (const auto& extension : availableExtensions)
                {
                    if (!std::strncmp(requiredExtensions[i], extension.extensionName, m_stringMaxLen))
                    {
                        requiredExtensionsBits |= (1U << i);
                        break;
                    }
                }
            }

            if (utils::countBits(requiredExtensionsBits) != requiredExtensions.size())
            {
                continue;
            }

            const auto features
            {
                physicalDevice.template getFeatures2
                <
                    vk::PhysicalDeviceFeatures2,
                    vk::PhysicalDeviceVulkan11Features,
                    vk::PhysicalDeviceVulkan13Features,
                    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
                >
                ()
            };

            const bool supportsRequiredFeatures
            {
                features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters    &&
                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering        &&
                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState
            };

            if (!supportsRequiredFeatures)
            {
                continue;
            }

            m_physicalDevice = physicalDevice;
        }

        if (m_physicalDevice == nullptr)
        {
            throw std::runtime_error{"couldn't find suitable physical device!"};
        }
        else
        {
            spdlog::info("found suitable logical device");
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createLogicalDevice()
    {
        const std::vector<vk::QueueFamilyProperties> queueFamilyProperties
        {
            m_physicalDevice.getQueueFamilyProperties()
        };

        uint32_t graphicsFamilyInd{ ~0U };

        for (uint32_t i{}; i < queueFamilyProperties.size(); ++i)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                m_physicalDevice.getSurfaceSupportKHR(i, *m_surface))
            {
                graphicsFamilyInd = i;
                break;
            }
        }

        if (graphicsFamilyInd == ~0U)
        {
            throw std::runtime_error{"couldn't find queue family for graphics!"};
        }

        constexpr float priority{ 0.5f };

        const vk::DeviceQueueCreateInfo deviceQueueCreateInfo
        {
            .queueFamilyIndex   = graphicsFamilyInd,
            .queueCount         = 1,
            .pQueuePriorities   = &priority,
        };

        // constexpr vk::PhysicalDeviceFeatures deviceFeatures{};

        const vk::StructureChain
        <
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        >
        featureChain
        {
            {},                         // vk::PhysicalDeviceFeatures2
            { .shaderDrawParameters     = true },
            { .dynamicRendering         = true },
            { .extendedDynamicState     = true },
        };

        const std::vector<const char*> requiredDeviceExtensions
        {
            vk::KHRSwapchainExtensionName,
        };

        const vk::DeviceCreateInfo createInfo
        {
            .pNext                      = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount       = 1,
            .pQueueCreateInfos          = &deviceQueueCreateInfo,
            .enabledExtensionCount      = static_cast<uint32_t>(requiredDeviceExtensions.size()),
            .ppEnabledExtensionNames    = &requiredDeviceExtensions[0],
        };

        m_device = vk::raii::Device(m_physicalDevice, createInfo);
        spdlog::info("logical device is created");

        m_graphicsQueue = vk::raii::Queue(m_device, graphicsFamilyInd, 0);
        spdlog::info("graphics queue is created");
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
