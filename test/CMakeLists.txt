find_package(Check REQUIRED)

add_executable(tests tests.c test_rtipc.c test_mapper.c )

target_link_libraries(tests  rtipc Check::checkShared )

