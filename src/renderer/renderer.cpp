#include "renderer.hpp"
#include "../common/utils.hpp"

#include "vulkan/vulkan.hpp"
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstring>
#include <vector>
#include <cassert>
#include <limits>
#include <vulkan/vulkan_raii.hpp>

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
        createSwapchain();
        createSwapchainImageViews();
        createBasicGraphicsPipeline();
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
#ifndef NDEBUG
        spdlog::warn("Running in DEBUG mode");
#endif

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
    static bool willRenderingBeSlow(const vk::PhysicalDeviceProperties& props)
    {
        return props.deviceType != vk::PhysicalDeviceType::eDiscreteGpu &&
               props.deviceType != vk::PhysicalDeviceType::eIntegratedGpu;
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::pickPhysicalDevice()
    {
        const auto physicalDevices{ m_instance.enumeratePhysicalDevices() };

        if (physicalDevices.empty())
        {
            throw std::runtime_error{"no physical vulkan device found!"};
        }

        vk::raii::PhysicalDevice selectedDevice   { nullptr };
        vk::raii::PhysicalDevice preferredDevice  { nullptr };

        for (auto physicalDevice : physicalDevices)
        {
            const auto deviceProperties{ physicalDevice.getProperties() };

            if (deviceProperties.apiVersion < vk::ApiVersion14 ||
                willRenderingBeSlow(deviceProperties))
            {
                continue;
            }

            const auto queueFamilies{ physicalDevice.getQueueFamilyProperties() };

            bool foundGraphicsFamily{};

            for (uint32_t i{}; i < queueFamilies.size(); ++i)
            {
                if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
                    && physicalDevice.getSurfaceSupportKHR(i, *m_surface))
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

            if (!supportsRequiredFeatures                                               ||
                physicalDevice.getSurfaceCapabilitiesKHR(*m_surface).maxImageCount < 1  ||
                physicalDevice.getSurfaceFormatsKHR(*m_surface).empty()                 ||
                physicalDevice.getSurfacePresentModesKHR(*m_surface).empty())
            {
                continue;
            }

            selectedDevice = physicalDevice;

            if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                preferredDevice = physicalDevice;
            }
        }

        if (preferredDevice != nullptr)
        {
            m_physicalDevice = preferredDevice;
            spdlog::info("running on discrete gpu");
        }
        else
        {
            m_physicalDevice = selectedDevice;
            spdlog::warn("running on integrated gpu");
        }

        if (m_physicalDevice == nullptr)
        {
            throw std::runtime_error{"couldn't find suitable physical device!"};
        }

        spdlog::info("found suitable logical device");
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
    vk::SurfaceFormatKHR renderer::chooseSwapSurfaceFormat
    (
        const std::vector<vk::SurfaceFormatKHR>& formats
    )
    {
        assert(formats.size() > 0);

        for (const auto& fmt : formats)
        {
            if (fmt.format      == vk::Format::eB8G8R8A8Srgb &&
                fmt.colorSpace  == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return fmt;
            }
        }

        spdlog::warn("couldn't find SRGB surface");
        return formats[0];
    }

    ///////////////////////////////////////////////////////////////////////////
    vk::PresentModeKHR renderer::chooseSwapPresentMode
    (
        const std::vector<vk::PresentModeKHR>& modes
    )
    {
        assert(modes.size() > 0);

        for (const auto& mode : modes)
        {
            if (mode == vk::PresentModeKHR::eMailbox)
            {
                return mode;
            }
        }

        spdlog::warn("mailbox for present not found");
        return vk::PresentModeKHR::eFifo;
    }

    ///////////////////////////////////////////////////////////////////////////
    vk::Extent2D renderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps)
    {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return caps.currentExtent;
        }

        int32_t width{}, height{};
        glfwGetFramebufferSize(m_window, &width, &height);

        const uint32_t uwidth { static_cast<uint32_t>(width) };
        const uint32_t uheight{ static_cast<uint32_t>(height) };

        return
        {
            std::clamp(uwidth, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp(uheight, caps.minImageExtent.height, caps.maxImageExtent.height),
        };
    }

    ///////////////////////////////////////////////////////////////////////////
    uint32_t renderer::chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& caps)
    {
        uint32_t minImageCount{ std::max(m_swapchainMinImages,
                                         caps.minImageCount) };

        if (caps.maxImageCount > 0 && caps.maxImageCount < minImageCount)
        {
            minImageCount = caps.maxImageCount;
        }

        return minImageCount;
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createSwapchain()
    {
        const vk::SurfaceCapabilitiesKHR surfaceCaps
        { m_physicalDevice.getSurfaceCapabilitiesKHR(*m_surface) };

        const std::vector<vk::PresentModeKHR> presentModes
        { m_physicalDevice.getSurfacePresentModesKHR(*m_surface) };

        m_swapchainExtent = chooseSwapExtent(surfaceCaps);

        const uint32_t minImageCount{ chooseSwapMinImageCount(surfaceCaps) };

        std::vector<vk::SurfaceFormatKHR> availableFormats
        { m_physicalDevice.getSurfaceFormatsKHR(*m_surface) };

        m_swapchainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

        const vk::SwapchainCreateInfoKHR createInfo
        {
            .surface            = *m_surface,
            .minImageCount      = minImageCount,
            .imageFormat        = m_swapchainSurfaceFormat.format,
            .imageColorSpace    = m_swapchainSurfaceFormat.colorSpace,
            .imageExtent        = m_swapchainExtent,
            .imageArrayLayers   = 1, // more than 1 for VR
            .imageUsage         = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode   = vk::SharingMode::eExclusive,
            .preTransform       = surfaceCaps.currentTransform,
            .compositeAlpha     = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode        = chooseSwapPresentMode(presentModes),
            .clipped            = true,
            .oldSwapchain       = nullptr,
        };

        m_swapchain             = vk::raii::SwapchainKHR(m_device, createInfo);
        m_swapchainImages       = m_swapchain.getImages();

        spdlog::info("created swapchain with {} images", m_swapchainImages.size());
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createSwapchainImageViews()
    {
        assert(m_swapchainImageViews.empty());

        vk::ImageViewCreateInfo createInfo
        {
            .viewType           = vk::ImageViewType::e2D,
            .format             = m_swapchainSurfaceFormat.format,
            .subresourceRange   =
            {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .levelCount     = 1,
                .layerCount     = 1, // needs to be more than 1 for VR
            }
        };

        m_swapchainImageViews.reserve(m_swapchainImages.size());

        for (const auto& image : m_swapchainImages)
        {
            createInfo.image = image;
            m_swapchainImageViews.emplace_back(m_device, createInfo);
        }

        spdlog::info("created {} image views for the swapchain", m_swapchainImageViews.size());
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////// GRAPHICS PIPELINES ////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createBasicGraphicsPipeline()
    {

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
