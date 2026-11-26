find_program(
    GLSLANG_VALIDATOR
    NAMES glslangValidator
    HINTS "$ENV{VULKAN_SDK}/Bin"
)
if(NOT GLSLANG_VALIDATOR)
    foreach(CONFIGURATION IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOLOWER "${CONFIGURATION}" CONFIGURATION_LOWER)
        file(GLOB CONAN_BUILD_ENVIRONMENTS
            "${CMAKE_BINARY_DIR}/conan/conanbuildenv-${CONFIGURATION_LOWER}-*.bat"
        )
        foreach(CONAN_BUILD_ENVIRONMENT IN LISTS CONAN_BUILD_ENVIRONMENTS)
            file(READ "${CONAN_BUILD_ENVIRONMENT}" CONAN_BUILD_ENVIRONMENT_CONTENT)
            string(REGEX MATCH "set \"PATH=([^%]*)%PATH%" CONAN_PATH_MATCH "${CONAN_BUILD_ENVIRONMENT_CONTENT}")
            if(CMAKE_MATCH_1)
                set(CONAN_TOOL_PATHS "${CMAKE_MATCH_1}")
                find_program(
                    GLSLANG_VALIDATOR
                    NAMES glslangValidator
                    PATHS ${CONAN_TOOL_PATHS}
                    NO_DEFAULT_PATH
                )
            endif()
            if(GLSLANG_VALIDATOR)
                break()
            endif()
        endforeach()
        if(GLSLANG_VALIDATOR)
            break()
        endif()
    endforeach()
endif()
if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator was not found in PATH, the Vulkan SDK, or Conan's tool environment.")
endif()

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(SHADER_HEADER_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/GenerateSpirvHeader.cmake")

function(compile_shader source_name shader_stage output_header output_variable)
    set(SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${source_name}")
    set(SHADER_SPIRV "${SHADER_OUTPUT_DIR}/${source_name}.spv")
    set(SHADER_HEADER "${SHADER_OUTPUT_DIR}/${output_header}")
    add_custom_command(
        OUTPUT "${SHADER_SPIRV}" "${SHADER_HEADER}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLANG_VALIDATOR}" --quiet -V --target-env vulkan1.0 -S "${shader_stage}"
            -o "${SHADER_SPIRV}" "${SHADER_SOURCE}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${SHADER_SPIRV}"
            "-DOUTPUT=${SHADER_HEADER}"
            "-DVARIABLE=${output_variable}"
            -P "${SHADER_HEADER_GENERATOR}"
        DEPENDS "${SHADER_SOURCE}" "${SHADER_HEADER_GENERATOR}"
        VERBATIM
    )
    set(${output_variable}_HEADER "${SHADER_HEADER}" PARENT_SCOPE)
    set(${output_variable}_SPIRV "${SHADER_SPIRV}" PARENT_SCOPE)
endfunction()

compile_shader(circle.vert vert CircleVertexShader.h CIRCLE_VERTEX_SPIRV)
compile_shader(circle.frag frag CircleFragmentShader.h CIRCLE_FRAGMENT_SPIRV)
add_custom_target(
    ClientShaders
    DEPENDS
        "${CIRCLE_VERTEX_SPIRV}"
        "${CIRCLE_VERTEX_SPIRV_HEADER}"
        "${CIRCLE_FRAGMENT_SPIRV}"
        "${CIRCLE_FRAGMENT_SPIRV_HEADER}"
)

add_library(Client STATIC
    Camera.cpp
    Client.cpp
    EconomyChart.cpp
    GameWindow.cpp
    ImGuiVulkanBackend.cpp
    Selection.cpp
    Ui.cpp
    VulkanRenderer.cpp
)
add_dependencies(Client ClientShaders)
set(IMGUI_BACKEND_DIR
    "$<IF:$<CONFIG:Debug>,${imgui_PACKAGE_FOLDER_DEBUG},${imgui_PACKAGE_FOLDER_RELEASE}>/res/bindings")

target_include_directories(Client
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${SHADER_OUTPUT_DIR}"
        "${IMGUI_BACKEND_DIR}"
)
target_link_libraries(
    Client
    PUBLIC
        Server
        SDL3::SDL3
        imgui::imgui
        Vulkan::Headers
        Vulkan::Loader
        galaxies_compile_options
)

if(BUILD_TESTING)
    add_executable(
        Client.Test
        Camera.Test.cpp
        EconomyChart.Test.cpp
        EconomyHistory.Test.cpp
        Selection.Test.cpp
        VulkanRenderer.Test.cpp
    )
    target_link_libraries(Client.Test PRIVATE Client Catch2::Catch2WithMain galaxies_compile_options)
    copy_conan_runtime_dlls(Client.Test)
    add_test(NAME Client.Unit COMMAND Client.Test)
endif()
