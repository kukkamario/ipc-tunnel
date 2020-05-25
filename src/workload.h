#ifndef WORKLOAD_H_
#define WORKLOAD_H_

#include "shared_state.h"

void WORKLOAD_Init(void);

void WORKLOAD_T0(void);
void WORKLOAD_T1(void);
void WORKLOAD_T2(void);

void WORKLOAD_BG(void);

extern SharedState_Variables s_variables;

#endif  // WORKLOAD_H_
