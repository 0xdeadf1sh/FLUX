#################### FLUX ####################
cmake_minimum_required(VERSION 3.30...4.4)

project(FLUX
    VERSION 0.1.0
    DESCRIPTION "3D Game Engine"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(FluxCompilerFlags)

#################### GLOBAL DEFAULTS ####################
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed, use cmake -B build.")
endif()

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            Debug Release RelWithDebInfo MinSizeRel)
    endif()

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()

#################### DEPENDENCIES ####################
find_package(Vulkan 1.4 REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)

#################### EXECUTABLE ####################
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/renderer/pipelines/graphics/common.cpp
    src/renderer/pipelines/graphics/basic.cpp
    src/renderer/renderer.cpp
    src/main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_26)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLFW_INCLUDE_NONE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    VULKAN_HPP_NO_CONSTRUCTORS
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    spdlog::spdlog)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON)

flux_target_compiler_flags(${PROJECT_NAME})

#################### SECURITY ####################
include(CheckPIESupported)
check_pie_supported(OUTPUT_VARIABLE pie_msg LANGUAGES CXX)
if(CMAKE_CXX_LINK_PIE_SUPPORTED)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        POSITION_INDEPENDENT_CODE ON)
else()
    message(WARNING "PIE not supported: ${pie_msg}")
endif()

#################### PERFORMANCE ####################
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg LANGUAGES CXX)
if(ipo_ok)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
        INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
else()
    message(WARNING "IPO/LTO not supported: ${ipo_msg}")
endif()

#################### SHADERS ####################
include(FluxCompileShaders)

add_slang_shader_target(graphics_shaders
    OUTPUT_DIR  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/graphics
    INCLUDE_DIR shaders/common
    SOURCES     shaders/graphics/basic.slang)

add_dependencies(${PROJECT_NAME} graphics_shaders)
