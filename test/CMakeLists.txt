find_package(Check REQUIRED)

add_executable(test_rtipc test_rtipc.c)

target_link_libraries(test_rtipc rtipc Check::checkShared )

