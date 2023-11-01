cmake_minimum_required(VERSION 3.5)

project(rtipc LANGUAGES C)

include(GNUInstallDirs)

set(RTIPC_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(RTIPC_PUBLIC_INCLUDE_DIR include)
set(RTIPC_PRIVATE_INCLUDE_DIR src)

set(RTIPC_SOURCE_FILES
  ${RTIPC_SOURCE_DIR}/mem_utils.c
  ${RTIPC_SOURCE_DIR}/log.c
  ${RTIPC_SOURCE_DIR}/sys.c
  ${RTIPC_SOURCE_DIR}/shm.c
  ${RTIPC_SOURCE_DIR}/channel.c
  ${RTIPC_SOURCE_DIR}/om.c
  ${RTIPC_SOURCE_DIR}/server.c
  ${RTIPC_SOURCE_DIR}/client.c)


set(RTIPC_INCLUDE_FILES
  ${RTIPC_PUBLIC_INCLUDE_DIR}/rtipc/log.h
  ${RTIPC_PUBLIC_INCLUDE_DIR}/rtipc/channel.h
  ${RTIPC_PUBLIC_INCLUDE_DIR}/rtipc/shm.h
  ${RTIPC_PUBLIC_INCLUDE_DIR}/rtipc/om.h
  ${RTIPC_PUBLIC_INCLUDE_DIR}/rtipc/server.h
  ${RTIPC_PUBLIC_INCLUDE_DIR}/rtipc/client.h
)

set(RTIPC_COMPILER_OPTIONS -Wall -Wextra  -Wsign-compare)

add_library(${PROJECT_NAME} ${RTIPC_SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${RTIPC_PUBLIC_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${RTIPC_PRIVATE_INCLUDE_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES
  C_STANDARD 11
  PUBLIC_HEADER "${RTIPC_INCLUDE_FILES}")

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra  -Wsign-compare)

add_executable(named_shm  sample/named_shm.c)
target_compile_options(named_shm PRIVATE ${RTIPC_COMPILER_OPTIONS})
target_link_libraries(named_shm PRIVATE ${PROJECT_NAME})

add_executable(anon_shm  sample/anon_shm.c)
target_compile_options(anon_shm PRIVATE ${RTIPC_COMPILER_OPTIONS})
target_link_libraries(anon_shm PRIVATE ${PROJECT_NAME})

add_executable(simple  sample/simple.c)
target_compile_options(simple PRIVATE ${RTIPC_COMPILER_OPTIONS})
target_link_libraries(simple PRIVATE ${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rtipc)
