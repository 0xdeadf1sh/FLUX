#################### FLUX COMPILER FLAGS ####################
include_guard(GLOBAL)

function(flux_target_compiler_flags target)

    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        return()
    endif()

    set(is_linux $<PLATFORM_ID:Linux>)

    set(is_x86 FALSE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        set(is_x86 TRUE)
    endif()

    #################### ALL BUILDS ####################
    set(common_flags
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
        -Wnull-dereference -Wdouble-promotion
        -Wformat=2 -Wimplicit-fallthrough
        -fstack-protector-strong)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND common_flags -fstack-clash-protection)
        if(is_x86)
            list(APPEND common_flags -fcf-protection=full)
        endif()
    endif()

    #################### DEBUG ####################
    set(debug_flags
        -fno-omit-frame-pointer)

    set(debug_defines
        _GLIBCXX_ASSERTIONS
        _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)

    set(sanitizers)
    if(NOT WIN32)
        set(sanitizers -fsanitize=address -fsanitize=undefined)
    endif()

    #################### RELEASE ####################
    set(release_flags)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND release_flags -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
    endif()

    #################### APPLY ####################
    set(is_debug    $<CONFIG:Debug>)
    set(is_release  $<CONFIG:Release,RelWithDebInfo,MinSizeRel>)

    target_compile_options(${target} PRIVATE
        ${common_flags}
        "$<${is_debug}:${debug_flags};${sanitizers}>"
        "$<${is_release}:${release_flags}>")

    target_compile_definitions(${target} PRIVATE
        "$<${is_debug}:${debug_defines}>")

    target_link_options(${target} PRIVATE
        "$<${is_debug}:${sanitizers}>"
        "$<${is_linux}:LINKER:-z,relro,-z,now,-z,noexecstack>")

endfunction()
