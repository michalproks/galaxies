add_library(Simulation STATIC Simulation.cpp)
target_include_directories(Simulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(Simulation PUBLIC galaxies_compile_options)

if(BUILD_TESTING)
    add_executable(Simulation.Test Simulation.Test.cpp)
    target_link_libraries(Simulation.Test PRIVATE Simulation Catch2::Catch2WithMain galaxies_compile_options)
    add_test(NAME Simulation.Unit COMMAND Simulation.Test)
endif()
