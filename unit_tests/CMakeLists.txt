#add_subdirectory(openamp)

set(variants openamp ipc-tunnel-ocm ipc-tunnel-ddr ipc-tunnel-ocm-cached ipc-tunnel-ddr-cached)

foreach(var ${variants})
	add_executable(latency-${var} latency.c)
	target_link_libraries(latency-${var} PRIVATE bsp openamp_support variant-${var})
	target_include_directories(latency-${var} PRIVATE common)
	
	add_executable(throughput-${var} throughput.c)
	target_link_libraries(throughput-${var} PRIVATE bsp openamp_support variant-${var})
	target_include_directories(throughput-${var} PRIVATE common)
endforeach()
