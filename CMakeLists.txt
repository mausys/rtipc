cmake_minimum_required(VERSION 3.5)


project(rtipc LANGUAGES C)

set(CMAKE_C_STANDARD 11)

include(GNUInstallDirs)

add_subdirectory(lib)
add_subdirectory(examples)

