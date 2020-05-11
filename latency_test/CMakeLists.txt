#add_subdirectory(openamp)

add_executable(latency_test-openamp main.c)
target_link_libraries(latency_test-openamp PRIVATE bsp openamp_support variant-openamp)
target_include_directories(latency_test-openamp PRIVATE common)

add_executable(latency_test-ipc-tunnel-v3 main.c)
target_link_libraries(latency_test-ipc-tunnel-v3 PRIVATE bsp openamp_support  variant-ipc-tunnel-v3)
target_include_directories(latency_test-ipc-tunnel-v3 PRIVATE common)
