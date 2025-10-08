function (add_exec source output)
  add_executable(${output} ${source})
  target_link_libraries(${output} PRIVATE project_options project_warnings big_num_core)
endfunction()

target_compile_definitions(big_num_core INTERFACE ENABLE_BIG_NUM_TRACE)
 set(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
# add_exec("example_1.cpp" example_1)
add_exec("main.cpp" main)
