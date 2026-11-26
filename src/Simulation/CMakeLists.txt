file(GLOB_RECURSE SIMULATION_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER SIMULATION_SOURCES EXCLUDE REGEX "\\.Test\\.cpp$")

add_library(Simulation STATIC ${SIMULATION_SOURCES})
target_include_directories(Simulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(Simulation PUBLIC galaxies_compile_options)

if(BUILD_TESTING)
    file(GLOB_RECURSE SIMULATION_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.Test.cpp")

    add_executable(Simulation.Test ${SIMULATION_TEST_SOURCES})
    target_link_libraries(Simulation.Test PRIVATE Simulation Catch2::Catch2WithMain galaxies_compile_options)
    add_test(NAME Simulation.Unit COMMAND Simulation.Test)
endif()
