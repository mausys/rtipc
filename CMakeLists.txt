cmake_minimum_required(VERSION 3.5)


project(rtipc LANGUAGES C)



set(CMAKE_C_STANDARD 11)

option(RTIPC_BUILD_EXAMPLES "Build examples" OFF)

include(GNUInstallDirs)

set(RTIPC_SRCS
  src/log.c
  src/shm.c
  src/shm.h
  src/queue.c
  src/queue.h
  src/producer.c
  src/producer.h
  src/consumer.c
  src/consumer.h
  src/mem_utils.c
  src/mem_utils.h
  src/index.h
  src/vector.c
  src/vector.h
  src/channel.c
  src/channel.h
  src/header.c
  src/header.h
  src/unix.c
  src/unix.h
  src/request.c
  src/server.c
  src/client.c)




set(RTIPC_COMPILER_OPTIONS -Wall -Wextra  -Wsign-compare)

add_library(${PROJECT_NAME} SHARED ${RTIPC_SRCS})

target_include_directories(rtipc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra  -Wsign-compare)
target_compile_definitions(${PROJECT_NAME} PUBLIC -DRTIPC_LOG_API=1)


install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


if (RTIPC_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif ()
