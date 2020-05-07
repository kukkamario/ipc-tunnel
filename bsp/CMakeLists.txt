CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
PROJECT(BSP)

add_library(file_io_stubs STATIC file_io_stubs.c)

add_library(bsp INTERFACE)
target_include_directories(bsp
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    
target_link_directories(bsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(bsp INTERFACE
    metal
    open_amp-static
    xil
    file_io_stubs
)

target_include_directories(bsp INTERFACE ../open-amp-master/lib/include)


target_compile_definitions(bsp INTERFACE USE_AMP=1)
