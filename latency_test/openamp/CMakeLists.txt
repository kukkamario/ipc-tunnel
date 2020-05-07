add_executable(latency_test_openamp main.c)
target_link_libraries(latency_test_openamp PRIVATE openamp_support bsp)
target_include_directories(latency_test_openamp PRIVATE ../common)
