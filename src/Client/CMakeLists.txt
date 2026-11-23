add_library(Client STATIC
    Client.cpp
    GameWindow.cpp
    VulkanRenderer.cpp
)
target_include_directories(Client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
    Client
    PUBLIC
        SDL3::SDL3
        Vulkan::Headers
        Vulkan::Loader
        galaxies_compile_options
)

if(BUILD_TESTING)
    add_executable(Client.Test VulkanRenderer.Test.cpp)
    target_link_libraries(Client.Test PRIVATE Client Catch2::Catch2WithMain galaxies_compile_options)
    copy_conan_runtime_dlls(Client.Test)
    add_test(NAME Client.Unit COMMAND Client.Test)
endif()
