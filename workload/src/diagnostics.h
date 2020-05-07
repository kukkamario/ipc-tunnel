#ifndef WORKLOAD_DIAGNOSTICS_H
#define WORKLOAD_DIAGNOSTICS_H
#include <stdint.h>

#define DIAGNOSTIC_BUFFER_SIZE 1000

typedef struct {
    uint64_t startTime;
    uint64_t startTimeOffsetCounter;
    uint64_t startTimeVarianceCounter;
    uint64_t durationCounter;
    uint64_t durationVarianceCounter;
    uint32_t averageDuration;
    uint32_t averageStartTimeOffset;
    uint32_t averageDurationVariance;
    uint32_t averageStartTimeVariance;
    uint32_t counter;
    uint32_t samplingPeriodCount;
} DiagnosticTimeLevel_t;

typedef enum {
    DIAG_PHASE_WARMUP,
    DIAG_PHASE_DURATION,
    DIAG_PHASE_DURATION_VARIANCE,
    DIAG_PHASE_START_TIME_JITTER,
    DIAG_PHASE_END
} DiagnosticPhase_t;

typedef struct {
    DiagnosticTimeLevel_t t0, t1, t2;
    DiagnosticPhase_t phase;
} DiagnosticData_t;

void DIAGNOSTIC_Init(DiagnosticData_t* data);

void DIAGNOSTIC_StartT0(DiagnosticData_t* data);
void DIAGNOSTIC_EndT0(DiagnosticData_t* data);

void DIAGNOSTIC_StartT1(DiagnosticData_t* data);
void DIAGNOSTIC_EndT1(DiagnosticData_t* data);


void DIAGNOSTIC_StartT2(DiagnosticData_t* data);
void DIAGNOSTIC_EndT2(DiagnosticData_t* data);

void DIAGNOSTIC_NextPhase(void);


#endif  // WORKLOAD_DIAGNOSTICS_H
