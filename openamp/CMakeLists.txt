add_library(openamp_support OBJECT
    helper.c
    platform_info.c
    rsc_table.c
    zynq_a9_rproc.c)

target_link_libraries(openamp_support PUBLIC bsp)
target_include_directories(openamp_support PUBLIC .)
