cmake_minimum_required(VERSION 3.5)

project(rtipc LANGUAGES C)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/src/abx.c ${PROJECT_SOURCE_DIR}/src/rtipc.c)

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic  -Wsign-compare)

add_executable(sample  sample/main.c)
target_compile_options(sample PRIVATE -Wall -Wextra -Wpedantic  -Wsign-compare)

target_link_libraries(sample PRIVATE ${PROJECT_NAME})

