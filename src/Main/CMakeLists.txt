add_executable(Galaxies Main.cpp)
target_link_libraries(Galaxies PRIVATE galaxies_compile_options)
set_target_properties(Galaxies PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

if(BUILD_TESTING)
    add_test(NAME Galaxies.Startup COMMAND Galaxies)
    set_tests_properties(Galaxies.Startup PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()
