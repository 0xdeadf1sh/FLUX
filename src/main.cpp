#include "renderer/renderer.hpp"

#include <spdlog/spdlog.h>
#include <print>

int main(int argc, char** argv)
{
    spdlog::flush_on(spdlog::level::info);

    try
    {
        flux::renderer renderer{argc, argv};

        renderer.loop();
    }
    catch (const vk::SystemError& e)
    {
        std::println(stderr, "{} raised vulkan exception: {}", argv[0], e.what());
        return EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        std::println(stderr, "{} raised unhandled exception: {}", argv[0], e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
