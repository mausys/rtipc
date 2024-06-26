cmake_minimum_required(VERSION 3.5)


project(rtipc LANGUAGES C)

set(CMAKE_C_STANDARD 11)

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
  ${RTIPC_SOURCE_DIR}/mapper.c
  ${RTIPC_SOURCE_DIR}/object.c
  ${RTIPC_SOURCE_DIR}/odb.c)


set(RTIPC_INCLUDE_FILES
  ${RTIPC_PUBLIC_INCLUDE_DIR}/rtipc.h
)

set(RTIPC_COMPILER_OPTIONS -Wall -Wextra  -Wsign-compare)

add_library(${PROJECT_NAME} ${RTIPC_SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${RTIPC_PUBLIC_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${RTIPC_PRIVATE_INCLUDE_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES
  PUBLIC_HEADER "${RTIPC_INCLUDE_FILES}")

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra  -Wsign-compare)

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_subdirectory(test)
#add_subdirectory(sample)


