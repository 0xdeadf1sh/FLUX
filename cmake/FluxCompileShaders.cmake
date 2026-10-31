#################### FLUX SHADER COMPILATION ####################
include_guard(GLOBAL)

find_program(SLANGC_EXECUTABLE slangc REQUIRED)

function(add_slang_shader_target TARGET)

    cmake_parse_arguments(PARSE_ARGV 1 ARG
        ""
        "OUTPUT_DIR"
        "SOURCES;ENTRY_POINTS;INCLUDE_DIRS")

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "add_slang_shader_target(${TARGET}): no SOURCES given")
    endif()

    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    endif()

    if (NOT ARG_ENTRY_POINTS)
        set(ARG_ENTRY_POINTS vmain fmain)
    endif()

    set(entry_flags)
    foreach(ep IN LISTS ARG_ENTRY_POINTS)
        list(APPEND entry_flags -entry ${ep})
    endforeach()

    set(include_flags)
    foreach(dir IN LISTS ARG_INCLUDE_DIRS)

        cmake_path(ABSOLUTE_PATH dir
            BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            NORMALIZE OUTPUT_VARIABLE dir_abs)

        list(APPEND include_flags -I ${dir_abs})

    endforeach()

    set(dep_dir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${TARGET}.dir)

    set(spv_files)
    foreach(src IN LISTS ARG_SOURCES)

        cmake_path(ABSOLUTE_PATH src
            BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            NORMALIZE OUTPUT_VARIABLE src_abs)

        cmake_path(GET src_abs FILENAME name)

        set(spv ${ARG_OUTPUT_DIR}/${name}.spv) 
        set(dep ${dep_dir}/${name}.d)

        add_custom_command(
            OUTPUT ${spv}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ARG_OUTPUT_DIR} ${dep_dir}
            COMMAND ${SLANGC_EXECUTABLE} ${src_abs}
                -target spirv
                -profile spirv_1_4
                -emit-spirv-directly
                -fvk-invert-y
                -fvk-use-entrypoint-name ${entry_flags} ${include_flags}
                -depfile ${dep}
                -o ${spv}
            DEPENDS ${src_abs}
            DEPFILE ${dep}
            COMMENT "Compiling shader ${name}"
            VERBATIM
        )

        list(APPEND spv_files ${spv})

    endforeach()

    add_custom_target(${TARGET} DEPENDS ${spv_files})

endfunction()
