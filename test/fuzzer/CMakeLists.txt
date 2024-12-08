add_executable(driver driver.cpp)
target_link_libraries(driver PRIVATE big_num_core)

# Create a custom command to copy fuzzer.py and track it as a dependency
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fuzzer.py"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/fuzzer.py"
            "${CMAKE_CURRENT_BINARY_DIR}/fuzzer.py"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fuzzer.py"
    COMMENT "Copying fuzzer.py to build directory"
)

# Create a custom target that depends on the copied fuzzer.py
add_custom_target(fuzzer_dependency ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/fuzzer.py"
)

# Make the driver target depend on the fuzzer
add_dependencies(driver fuzzer_dependency)
