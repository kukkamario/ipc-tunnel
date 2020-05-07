add_library(workload
  src/workload.c
  src/diagnostics.c
  src/interrupt.c
  src/scheduler.c
)

target_include_directories(workload PUBLIC include)
target_link_libraries(workload PUBLIC bsp)
