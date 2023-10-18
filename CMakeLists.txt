cmake_minimum_required(VERSION 3.5)

project(rtipc LANGUAGES C)


set(RTIPC_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

set (RTIPC_SRC
  ${RTIPC_SRC_DIR}/mem_utils.c
  ${RTIPC_SRC_DIR}/log.c
  ${RTIPC_SRC_DIR}/sys.c
  ${RTIPC_SRC_DIR}/shm.c
  ${RTIPC_SRC_DIR}/channel.c
  ${RTIPC_SRC_DIR}/object.c
  ${RTIPC_SRC_DIR}/server.c
  ${RTIPC_SRC_DIR}/client.c)

set (RTIPC_INC ${PROJECT_SOURCE_DIR}/include)


add_library(${PROJECT_NAME} ${RTIPC_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${RTIPC_INC})
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra  -Wsign-compare)

add_executable(named_shm  sample/named_shm.c)
target_compile_options(named_shm PRIVATE -Wall -Wextra  -Wsign-compare)
target_link_libraries(named_shm PRIVATE ${PROJECT_NAME})

add_executable(anon_shm  sample/anon_shm.c)
target_compile_options(anon_shm PRIVATE -Wall -Wextra  -Wsign-compare)
target_link_libraries(anon_shm PRIVATE ${PROJECT_NAME})


