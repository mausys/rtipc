cmake_minimum_required(VERSION 3.5)


project(rtipc LANGUAGES C)



set(CMAKE_C_STANDARD 11)

option(RTIPC_BUILD_EXAMPLES "Build examples" OFF)

include(GNUInstallDirs)

add_subdirectory(lib)

if (RTIPC_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif ()
