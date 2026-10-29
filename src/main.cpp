#include "renderer/renderer.hpp"

#include <print>

int main([[maybe_unused]] int argc, char** argv)
{
    try {
        flux::renderer renderer{};
    }
    catch (const std::exception& e) {
        std::println(stderr, "{} raised unhandled exception: {}", argv[0], e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
