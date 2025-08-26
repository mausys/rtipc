cmake_minimum_required(VERSION 3.5)


project(rtipc LANGUAGES C)

set(CMAKE_C_STANDARD 11)

include(GNUInstallDirs)

add_subdirectory(lib)

add_executable(example_rpc  examples/rpc.c)
target_compile_options(example_rpc PRIVATE ${RTIPC_COMPILER_OPTIONS})
target_link_libraries(example_rpc PRIVATE ${PROJECT_NAME})


