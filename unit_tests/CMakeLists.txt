#add_subdirectory(openamp)

add_executable(latency-openamp latency.c)
target_link_libraries(latency-openamp PRIVATE bsp openamp_support variant-openamp)
target_include_directories(latency-openamp PRIVATE common)

add_executable(latency-ipc-tunnel-v3 latency.c)
target_link_libraries(latency-ipc-tunnel-v3 PRIVATE bsp openamp_support  variant-ipc-tunnel-v3)
target_include_directories(latency-ipc-tunnel-v3 PRIVATE common)


add_executable(throughput-openamp throughput.c)
target_link_libraries(throughput-openamp PRIVATE bsp openamp_support variant-openamp)
target_include_directories(throughput-openamp PRIVATE common)

add_executable(throughput-ipc-tunnel-v3 throughput.c)
target_link_libraries(throughput-ipc-tunnel-v3 PRIVATE bsp openamp_support  variant-ipc-tunnel-v3)
target_include_directories(throughput-ipc-tunnel-v3 PRIVATE common)
