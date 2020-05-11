add_library(variant-header INTERFACE)
target_include_directories(variant-header INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(ipc-tunnel-v3)
add_subdirectory(openamp)
