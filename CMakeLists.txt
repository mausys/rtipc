cmake_minimum_required(VERSION 3.5)

project(rtipc LANGUAGES C)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/src/sys.c ${PROJECT_SOURCE_DIR}/src/object.c ${PROJECT_SOURCE_DIR}/src/shm.c ${PROJECT_SOURCE_DIR}/src/mem_utils.c ${PROJECT_SOURCE_DIR}/src/channel.c ${PROJECT_SOURCE_DIR}/src/log.c ${PROJECT_SOURCE_DIR}/src/server.c ${PROJECT_SOURCE_DIR}/src/client.c)

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra  -Wsign-compare)

add_executable(sample  sample/main.c)
target_compile_options(sample PRIVATE -Wall -Wextra  -Wsign-compare)

target_link_libraries(sample PRIVATE ${PROJECT_NAME})

