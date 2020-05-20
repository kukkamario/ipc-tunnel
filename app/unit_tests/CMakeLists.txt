add_executable(latency latency.cpp)

target_link_libraries(latency PRIVATE util)
target_include_directories(latency PRIVATE ../../unit_tests/common)

add_executable(throughput throughput.cpp)

target_link_libraries(throughput PRIVATE util)
target_include_directories(throughput PRIVATE ../../unit_tests/common)