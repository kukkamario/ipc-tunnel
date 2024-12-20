#ifndef WORKLOAD_SCHEDULER_H_
#define WORKLOAD_SCHEDULER_H_
#include <stdint.h>

typedef struct {
  uint32_t t0Frequency;
  uint32_t t1Multiplier;
  uint32_t t2Multiplier;

  void(*t0Task)(void);
  void(*t1Task)(void);
  void(*t2Task)(void);
} SchedulerConfig_t;

void SCHEDULER_Init(const SchedulerConfig_t* conf);

void SCHEDULER_Stop(void);

#endif   // WORKLOAD_SCHEDULER_H_
