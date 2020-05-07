#ifndef WORKLOAD_SCHEDULER_H_
#define WORKLOAD_SCHEDULER_H_
#include <stdint.h>

typedef struct {
  uint32_t t0Frequency;
  uint32_t t1Multiplier;

  void(*backgroundTask)(void);
  void(*t0Task)(void);
  void(*t1Task)(void);
} SchedulerConfig_t;

void SCHEDULER_Init(const SchedulerConfig_t* conf);

#endif   // WORKLOAD_SCHEDULER_H_