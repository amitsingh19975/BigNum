function (add_exec source output)
  add_executable(${output} ${source})
  target_link_libraries(${output} PRIVATE project_options project_warnings diagnostics_core)
endfunction()

add_exec("example_1.cpp" example_1)
