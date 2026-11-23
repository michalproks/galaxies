add_library(Server STATIC Server.cpp)
target_include_directories(Server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(Server PUBLIC Simulation galaxies_compile_options)

if(BUILD_TESTING)
    add_executable(Server.Test Server.Test.cpp)
    target_link_libraries(Server.Test PRIVATE Server Catch2::Catch2WithMain galaxies_compile_options)
    add_test(NAME Server.Unit COMMAND Server.Test)
endif()
