cmake_minimum_required(VERSION 3.25)

if(NOT WIN32)
    message(FATAL_ERROR "Galaxies is supported on Windows 11 only.")
endif()

option(GALAXIES_USE_CONAN "Resolve dependencies using Conan 2 and cmake-conan." ON)

if(GALAXIES_USE_CONAN)
    set(CONAN_PROVIDER "${CMAKE_BINARY_DIR}/conan_provider.cmake")
    if(NOT EXISTS "${CONAN_PROVIDER}")
        file(
            DOWNLOAD
            "https://raw.githubusercontent.com/conan-io/cmake-conan/develop2/conan_provider.cmake"
            "${CONAN_PROVIDER}"
            TLS_VERIFY ON
            STATUS CONAN_PROVIDER_DOWNLOAD_STATUS
        )
        list(GET CONAN_PROVIDER_DOWNLOAD_STATUS 0 CONAN_PROVIDER_DOWNLOAD_CODE)
        if(NOT CONAN_PROVIDER_DOWNLOAD_CODE EQUAL 0)
            list(GET CONAN_PROVIDER_DOWNLOAD_STATUS 1 CONAN_PROVIDER_DOWNLOAD_MESSAGE)
            file(REMOVE "${CONAN_PROVIDER}")
            message(FATAL_ERROR "Could not download the cmake-conan provider: ${CONAN_PROVIDER_DOWNLOAD_MESSAGE}")
        endif()
    endif()
    set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${CONAN_PROVIDER}")
endif()

project(Galaxies LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "Galaxies must be built with the MSVC compiler.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)

add_library(galaxies_compile_options INTERFACE)
target_compile_features(galaxies_compile_options INTERFACE cxx_std_23)
target_compile_options(
    galaxies_compile_options
    INTERFACE
        /W4
        /WX
        /external:W0
        /external:anglebrackets
        /permissive-
        /EHsc
        /Zc:__cplusplus
)

include(CTest)
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanLoader CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

if(BUILD_TESTING)
    find_package(Catch2 3.16.1 CONFIG REQUIRED)
    find_package(trompeloeil CONFIG REQUIRED)
endif()

function(copy_conan_runtime_dlls target)
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target}>
            $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endfunction()

add_subdirectory(src/Simulation)
add_subdirectory(src/Server)
add_subdirectory(src/Client)
add_subdirectory(src/Main)
