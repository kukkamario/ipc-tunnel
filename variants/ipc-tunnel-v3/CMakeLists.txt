add_library(variant-ipc-tunnel-v3 ipc_tunnel.c variant.c)

target_link_libraries(variant-ipc-tunnel-v3 PRIVATE bsp PUBLIC variant-header)

