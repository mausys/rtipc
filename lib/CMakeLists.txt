cmake_minimum_required(VERSION 3.5)


project(rtipc LANGUAGES C)

set(CMAKE_C_STANDARD 11)

include(GNUInstallDirs)


set(RTIPC_SRCS
  log.c
  log.h
  param.c
  param.h
  shm.c
  shm.h
  event.c
  event.h
  rtipc.h
  queue.c
  queue.h
  producer.c
  producer.h
  consumer.c
  consumer.h
  mem_utils.c
  mem_utils.h
  index.h
  vector.c
  vector.h
  channel.c
  channel.h
  header.c
  header.h
  unix_message.c
  unix_message.h
  fd.c
  fd.h
  protocol.c
  protocol.h
  server.c
  client.c
)


set(RTIPC_INCLUDE_FILES rtipc.h
)

set(RTIPC_COMPILER_OPTIONS -Wall -Wextra  -Wsign-compare)

add_library(${PROJECT_NAME} ${RTIPC_SRCS})

#target_include_directories(${PROJECT_NAME} PUBLIC ${RTIPC_PUBLIC_INCLUDE_DIR})
#target_include_directories(${PROJECT_NAME} PRIVATE ${RTIPC_PRIVATE_INCLUDE_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES
  PUBLIC_HEADER "${RTIPC_INCLUDE_FILES}")

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra  -Wsign-compare)

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})



