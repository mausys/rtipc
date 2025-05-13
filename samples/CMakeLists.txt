
add_executable(simple simple.c)
target_compile_options(simple PRIVATE ${RTIPC_COMPILER_OPTIONS})
target_link_libraries(simple PRIVATE ${PROJECT_NAME})
target_include_directories(simple PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
