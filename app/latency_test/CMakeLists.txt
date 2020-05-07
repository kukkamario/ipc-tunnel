add_executable(latency_test_openamp main_openamp.cpp)

target_link_libraries(latency_test_openamp PRIVATE util)
target_include_directories(latency_test_openamp PRIVATE ../../latency_test/common)
