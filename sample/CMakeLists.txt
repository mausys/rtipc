
add_executable(anon_shm  anon_shm.c server.c client.c ipc.c timestamp.c myobjects.c)
target_compile_options(anon_shm PRIVATE ${RTIPC_COMPILER_OPTIONS})
target_link_libraries(anon_shm PRIVATE ${PROJECT_NAME})

add_executable(simple  simple.c)
target_compile_options(simple PRIVATE ${RTIPC_COMPILER_OPTIONS})
target_link_libraries(simple PRIVATE ${PROJECT_NAME})
