cmake_minimum_required(VERSION 3.13)
project(Dippa)

set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -fno-math-errno -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -fno-math-errno -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -march=armv7-a -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -fno-math-errno -fdata-sections -ffunction-sections")
set(CMAKE_C_STANDARD_LIBRARIES "-Wl,-build-id=none,--start-group,-lfile_io_stubs,-lm,-lgcc,-lc,-lg,-lxil,--end-group")
set(CMAKE_CXX_STANDARD_LIBRARIES "-Wl,-build-id=none,--start-group,-lfile_io_stubs,-lstdc++,-lm,-lgcc,-lc,-lg,-lxil,--end-group")

link_directories(${CMAKE_CURRENT_BINARY_DIR}/bsp)

set(LIBMETAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bsp/include)
set(XIL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bsp/include)
set(WITH_PROXY OFF CACHE BOOL "asdsf" FORCE)
set(WITH_APPS ON CACHE BOOL "asdasd" FORCE)
set(LIBMETAL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/bsp/lib/libmetal.a")
set(MACHINE "zynq7")
add_subdirectory(open-amp-master)

SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,-T ${CMAKE_CURRENT_LIST_DIR}/lscript.ld -specs=${CMAKE_CURRENT_SOURCE_DIR}/Xilinx.spec")
SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -Wl,-T ${CMAKE_CURRENT_LIST_DIR}/lscript.ld -specs=${CMAKE_CURRENT_SOURCE_DIR}/Xilinx.spec")

add_subdirectory(bsp)
add_subdirectory(openamp)

add_subdirectory(variants)

add_subdirectory(unit_tests)

set(MAIN_SRCS
	src/main.c
	src/interrupt.c
	src/scheduler.c
	src/workload.c)

set(variants openamp ipc-tunnel-ocm ipc-tunnel-ddr ipc-tunnel-ocm-cached ipc-tunnel-ddr-cached)

foreach(var ${variants})
	add_executable(dippa-soft-${var} ${MAIN_SRCS})
	target_include_directories(dippa-soft-${var} PRIVATE include)
	target_link_libraries(dippa-soft-${var} PRIVATE openamp_support bsp variant-${var})
endforeach()
