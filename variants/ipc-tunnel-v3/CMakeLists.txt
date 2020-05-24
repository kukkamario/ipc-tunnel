add_library(variant-ipc-tunnel-ocm ipc_tunnel.c variant.c)
target_link_libraries(variant-ipc-tunnel-ocm PRIVATE bsp PUBLIC variant-header)
target_compile_definitions(variant-ipc-tunnel-ocm PRIVATE IPC_TUNNEL_OCM=1)

add_library(variant-ipc-tunnel-ddr ipc_tunnel.c variant.c)
target_link_libraries(variant-ipc-tunnel-ddr PRIVATE bsp PUBLIC variant-header)
target_compile_definitions(variant-ipc-tunnel-ddr PRIVATE IPC_TUNNEL_DDR=1)

add_library(variant-ipc-tunnel-ocm-cached ipc_tunnel.c variant.c)
target_link_libraries(variant-ipc-tunnel-ocm-cached PRIVATE bsp PUBLIC variant-header)
target_compile_definitions(variant-ipc-tunnel-ocm-cached PRIVATE IPC_TUNNEL_OCM=1 IPC_TUNNEL_CACHED=1)

add_library(variant-ipc-tunnel-ddr-cached ipc_tunnel.c variant.c)
target_link_libraries(variant-ipc-tunnel-ddr-cached PRIVATE bsp PUBLIC variant-header)
target_compile_definitions(variant-ipc-tunnel-ddr-cached PRIVATE IPC_TUNNEL_DDR=1 IPC_TUNNEL_CACHED=1)
