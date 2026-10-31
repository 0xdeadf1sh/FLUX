#include "renderer.hpp"
#include "../common/utils.hpp"

#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_raii.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstring>
#include <vector>
#include <cassert>
#include <limits>

#ifndef NDEBUG
#define FLUX_SHADERS_PATH "build/bin/shaders/"
#else
#define FLUX_SHADERS_PATH "shaders/"
#endif

namespace flux
{
    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////////// CTOR //////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    renderer::renderer([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
    {
        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////// INITIALIZATION /////////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        createWindow();
        createInstance();
        createDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createSwapchainImageViews();

        ///////////////////////////////////////////////////////////////////////////
        //////////////////////////// GRAPHICS PIPELINES ///////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        m_basicPipelineLayout   = createPipelineLayout();
        m_basicPipeline         = createPipeline(FLUX_SHADERS_PATH "basic.slang.spv",
                                                 m_basicPipelineLayout);

        ///////////////////////////////////////////////////////////////////////////
        /////////////////////////// VULKAN COMMAND OBJECTS ////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        createCommandPool();
        createCommandBuffers();

        ///////////////////////////////////////////////////////////////////////////
        ///////////////////////// SYNCHRONIZATION OBJECTS /////////////////////////
        ///////////////////////////////////////////////////////////////////////////
        createSyncObjects();
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////////// DTOR //////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    renderer::~renderer()
    { }

    ///////////////////////////////////////////////////////////////////////////
    //////////////////////////////// WINDOWING ////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createWindow()
    {
#ifndef NDEBUG
        spdlog::warn("running in DEBUG mode");
#endif

        if (!glfwInit())
        {
            throw std::runtime_error{"glfwInit() failed!"};
        }

        spdlog::info("initialized GLFW: {}", glfwGetVersionString());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        if (!(m_window.m_window = glfwCreateWindow(m_windowWidth,
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

        if (glfwCreateWindowSurface(*m_instance,
                                    m_window.m_window,
                                    nullptr,
                                    &surface))
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
                features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
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
            { .synchronization2         = true, .dynamicRendering = true },
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

        m_graphicsQueueFamilyIndex = graphicsFamilyInd;
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
        glfwGetFramebufferSize(m_window.m_window, &width, &height);

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
    vk::raii::ShaderModule renderer::createShaderModule(const std::vector<char>& bytecode)
    {
        const vk::ShaderModuleCreateInfo createInfo
        {
            .codeSize   = bytecode.size(),
            .pCode      = reinterpret_cast<const uint32_t*>(&bytecode[0])
        };

        return vk::raii::ShaderModule(m_device, createInfo);
    }

    ///////////////////////////////////////////////////////////////////////////
    vk::raii::PipelineLayout renderer::createPipelineLayout()
    {
        constexpr vk::PipelineLayoutCreateInfo pipelineLayoutInfo
        {
            .setLayoutCount             = 0,
            .pushConstantRangeCount     = 0,
        };

        vk::raii::PipelineLayout pipelineLayout
        { vk::raii::PipelineLayout(m_device, pipelineLayoutInfo) };

        return pipelineLayout;
    }

    ///////////////////////////////////////////////////////////////////////////
    vk::raii::Pipeline renderer::createPipeline(std::string_view shaderURL,
                                                const vk::raii::PipelineLayout& layout)
    {
        const std::vector<char> shaderCode{ utils::readBinaryFile(shaderURL) };
        spdlog::info("loaded shader {} with {} bytes", shaderURL, shaderCode.size());

        const vk::raii::ShaderModule shaderModule{ createShaderModule(shaderCode) };

        const vk::PipelineShaderStageCreateInfo vertexShaderStageInfo
        {
            .stage                  = vk::ShaderStageFlagBits::eVertex,
            .module                 = shaderModule,
            .pName                  = "vmain",
            .pSpecializationInfo    = nullptr,
        };

        const vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo
        {
            .stage                  = vk::ShaderStageFlagBits::eFragment,
            .module                 = shaderModule,
            .pName                  = "fmain",
            .pSpecializationInfo    = nullptr,
        };

        const vk::PipelineShaderStageCreateInfo shaderStages[]
        {
            vertexShaderStageInfo,
            fragmentShaderStageInfo,
        };

        const std::vector<vk::DynamicState> dynamicStates
        {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };

        const vk::PipelineDynamicStateCreateInfo dynamicStateInfo
        {
            .dynamicStateCount  = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates     = &dynamicStates[0],
        };

        constexpr vk::PipelineViewportStateCreateInfo viewportState
        {
            .viewportCount  = 1,
            .scissorCount   = 1,
        };

        constexpr vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

        constexpr vk::PipelineInputAssemblyStateCreateInfo inputAssembly
        {
            .topology = vk::PrimitiveTopology::eTriangleList,
        };

        constexpr vk::PipelineRasterizationStateCreateInfo rasterizer
        {
            .depthClampEnable           = vk::False,
            .rasterizerDiscardEnable    = vk::False,
            .polygonMode                = vk::PolygonMode::eFill,
            .cullMode                   = vk::CullModeFlagBits::eBack,
            .frontFace                  = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable            = vk::False,
            .lineWidth                  = 1.0f,
        };

        constexpr vk::PipelineMultisampleStateCreateInfo multisampling
        {
            .rasterizationSamples       = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable        = vk::False,
        };

        constexpr vk::PipelineColorBlendAttachmentState colorBlendAttachment
        {
            .blendEnable                = vk::False,
            .colorWriteMask             = vk::ColorComponentFlagBits::eR |
                                          vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |
                                          vk::ColorComponentFlagBits::eA,
        };

        const vk::PipelineColorBlendStateCreateInfo colorBlending
        {
            .logicOpEnable              = vk::False, // when True, blendEnable becomes False
            .logicOp                    = vk::LogicOp::eCopy,
            .attachmentCount            = 1,
            .pAttachments               = &colorBlendAttachment,
        };

        const vk::StructureChain
        <
            vk::GraphicsPipelineCreateInfo,
            vk::PipelineRenderingCreateInfo
        >
        createInfo
        {
            // graphics pipeline
            {
                // programmable stages
                .stageCount             = 2,
                .pStages                = shaderStages,

                // fixed-function stages
                .pVertexInputState      = &vertexInputInfo,
                .pInputAssemblyState    = &inputAssembly,
                .pViewportState         = &viewportState,
                .pRasterizationState    = &rasterizer,
                .pMultisampleState      = &multisampling,
                .pColorBlendState       = &colorBlending,
                .pDynamicState          = &dynamicStateInfo,

                // pipeline layout
                .layout                 = layout,

                // no renderpass needed
                .renderPass             = nullptr,

                // parent pipeline
                .basePipelineHandle     = nullptr,
                .basePipelineIndex      = 0,
                // .flags               = vk::PipelineCreateFlagBits::eDerivative,
            },

            // dynamic rendering
            {
                .colorAttachmentCount   = 1,
                .pColorAttachmentFormats = &m_swapchainSurfaceFormat.format,
            }
        };

        const vk::raii::PipelineCache cache{ nullptr };

        vk::raii::Pipeline pipeline(m_device,
                                    cache,
                                    createInfo.get<vk::GraphicsPipelineCreateInfo>());

        spdlog::info("created a pipeline for '{}'", shaderURL);

        return pipeline;
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////// VULKAN COMMANDS /////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createCommandPool()
    {
        const vk::CommandPoolCreateInfo poolInfo
        {
            .flags              = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex   = m_graphicsQueueFamilyIndex,
        };

        m_cmdPool = vk::raii::CommandPool(m_device, poolInfo);

        spdlog::info("created a command pool");
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createCommandBuffers()
    {
        const vk::CommandBufferAllocateInfo allocInfo
        {
            .commandPool            = m_cmdPool,
            .level                  = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount     = m_framesInFlight,
        };

        m_cmdBuffers = vk::raii::CommandBuffers(m_device, allocInfo);
    }

    ///////////////////////////////////////////////////////////////////////////
    static void transitionImageLayout
    (
        vk::ImageLayout             oldLayout,
        vk::ImageLayout             newLayout,
        vk::AccessFlags2            srcAccessMask,
        vk::AccessFlags2            dstAccessMask,
        vk::PipelineStageFlags2     srcStageMask,
        vk::PipelineStageFlags2     dstStageMask,
        const vk::Image&            image,
        const vk::CommandBuffer     cmdBuffer
    )
    {
        const vk::ImageMemoryBarrier2 barrier
        {
            .srcStageMask           = srcStageMask,
            .srcAccessMask          = srcAccessMask,
            .dstStageMask           = dstStageMask,
            .dstAccessMask          = dstAccessMask,
            .oldLayout              = oldLayout,
            .newLayout              = newLayout,
            .srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
            .image                  = image,
            .subresourceRange       =
            {
                .aspectMask         = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel       = 0,
                .levelCount         = 1,
                .baseArrayLayer     = 0,
                .layerCount         = 1,
            },
        };

        const vk::DependencyInfo dependency
        {
            .dependencyFlags            = {},
            .imageMemoryBarrierCount    = 1,
            .pImageMemoryBarriers       = &barrier,
        };

        cmdBuffer.pipelineBarrier2(dependency);
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::recordCommandBuffers(uint32_t imageIndex)
    {
        assert(m_cmdBuffers.size() > 0);

        const vk::raii::CommandBuffer& cmdBuffer{ m_cmdBuffers[m_frameIndex] };

        cmdBuffer.begin({});

        transitionImageLayout(vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eColorAttachmentOptimal,
                              {},
                              vk::AccessFlagBits2::eColorAttachmentWrite,
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                              m_swapchainImages[imageIndex],
                              cmdBuffer);

        constexpr vk::ClearValue clearColor
        { vk::ClearColorValue({{0.01f, 0.02f, 0.02f, 1.0f}}) };

        const vk::RenderingAttachmentInfo attachmentInfo
        {
            .imageView      = m_swapchainImageViews[imageIndex],
            .imageLayout    = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp         = vk::AttachmentLoadOp::eClear,
            .storeOp        = vk::AttachmentStoreOp::eStore,
            .clearValue     = clearColor,
        };

        const vk::RenderingInfo renderInfo
        {
            .renderArea             =
            {
                .offset             = {},
                .extent             = m_swapchainExtent,
            },
            .layerCount             = 1,
            .colorAttachmentCount   = 1,
            .pColorAttachments      = &attachmentInfo,
        };

        cmdBuffer.beginRendering(renderInfo);
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_basicPipeline);
        cmdBuffer.setViewport(0, vk::Viewport(0.0f,
                                              0.0f,
                                              static_cast<float>(m_swapchainExtent.width),
                                              static_cast<float>(m_swapchainExtent.height)));
        cmdBuffer.setScissor(0, vk::Rect2D{ {0, 0}, m_swapchainExtent });
        cmdBuffer.draw(3, 1, 0, 0);
        cmdBuffer.endRendering();

        transitionImageLayout(vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::ePresentSrcKHR,
                              vk::AccessFlagBits2::eColorAttachmentWrite,
                              {},
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                              vk::PipelineStageFlagBits2::eBottomOfPipe,
                              m_swapchainImages[imageIndex],
                              cmdBuffer);

        cmdBuffer.end();
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////// SYNCHRONIZATION /////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::createSyncObjects()
    {
        for (uint32_t i{}; i < m_swapchainImages.size(); ++i)
        {
            m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());
        }

        constexpr vk::FenceCreateInfo fenceCreateInfo
        {
            .flags = vk::FenceCreateFlagBits::eSignaled,
        };

        for (uint32_t i{}; i < m_framesInFlight; ++i)
        {
            m_presentCompleteSemaphores.emplace_back(m_device,
                                                     vk::SemaphoreCreateInfo());

            m_inFlightFences.emplace_back(m_device, fenceCreateInfo);
        }

        spdlog::info("created vulkan synchronization objects");
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////////////////////////// RENDER LOOP ///////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    void renderer::loop()
    {
        while (!glfwWindowShouldClose(m_window.m_window))
        {
            glfwPollEvents();
            render();
            stepToNextFrame();
        }

        m_device.waitIdle();
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::stepToNextFrame()
    {
        m_frameIndex = (m_frameIndex + 1) % m_framesInFlight;
    }

    ///////////////////////////////////////////////////////////////////////////
    void renderer::render()
    {
        const vk::Result fenceResult{ m_device.waitForFences(*m_inFlightFences[m_frameIndex],
                                                             vk::True,
                                                             UINT64_MAX) };

        if (fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error{"failed to wait for fence!"};
        }

        m_device.resetFences(*m_inFlightFences[m_frameIndex]);

        auto [result, imageIndex] = m_swapchain.acquireNextImage(UINT64_MAX,
                                                                 *m_presentCompleteSemaphores[m_frameIndex],
                                                                 nullptr);

        m_cmdBuffers[m_frameIndex].reset();
        recordCommandBuffers(imageIndex);

        constexpr vk::PipelineStageFlags waitDestinationStageFlag
        { vk::PipelineStageFlagBits::eColorAttachmentOutput };

        const vk::SubmitInfo submitInfo
        {
            .waitSemaphoreCount         = 1,
            .pWaitSemaphores            = &*m_presentCompleteSemaphores[m_frameIndex],
            .pWaitDstStageMask          = &waitDestinationStageFlag,
            .commandBufferCount         = 1,
            .pCommandBuffers            = &*m_cmdBuffers[m_frameIndex],
            .signalSemaphoreCount       = 1,
            .pSignalSemaphores          = &*m_renderFinishedSemaphores[imageIndex],
        };

        m_graphicsQueue.submit(submitInfo, *m_inFlightFences[m_frameIndex]);

        const vk::PresentInfoKHR presentInfoKHR
        {
            .waitSemaphoreCount         = 1,
            .pWaitSemaphores            = &*m_renderFinishedSemaphores[imageIndex],
            .swapchainCount             = 1,
            .pSwapchains                = &*m_swapchain,
            .pImageIndices              = &imageIndex,
        };

        result = m_graphicsQueue.presentKHR(presentInfoKHR);
    }
}
