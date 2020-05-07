#include "diagnostics.h"
#include <string.h>

#define INVALID_INDEX 0xFFFFu


volatile uint32_t doNotOptimize;

static uint64_t GetGlobalTimer(void) {
    return 0;
}

static  void TimeLevel_Init(DiagnosticTimeLevel_t* t)
{
    t->startTime = 0;
    t->startTimeOffsetCounter = 0;
    t->startTimeVarianceCounter = 0;
    t->durationCounter = 0;
    t->durationVarianceCounter = 0;
    t->averageDuration = 0;
    t->averageDurationVariance = 0;
    t->averageStartTimeOffset = 0;
    t->averageStartTimeVariance = 0;
    t->counter = 0;
    t->samplingPeriodCount = 0;
}

static void TimeLevel_NextPhase(DiagnosticTimeLevel_t* t, DiagnosticPhase_t prevPhase) {
    switch (prevPhase) {
    case DIAG_PHASE_WARMUP:
        t->startTime = 0;
        break;
    case DIAG_PHASE_DURATION:

        break;
    case DIAG_PHASE_START_TIME_JITTER:
        break;
    case DIAG_PHASE_END:
        break;
    }
}

static void TimeLevel_Start(DiagnosticTimeLevel_t* t, DiagnosticPhase_t phase) {
    uint64_t startTime = GetGlobalTimer();

    switch (phase) {
    case DIAG_PHASE_WARMUP:
        break;
    case DIAG_PHASE_DURATION:
        break;
    case DIAG_PHASE_START_TIME_JITTER:
        if (t->counter > 0) {
            uint32_t startTimeOffset = t->startTime - startTime;
            t->startTimeOffsetCounter += startTimeOffset;

            if (t->averageStartTimeOffset) {
                int32_t diff = (int32_t)startTimeOffset - (int32_t)t->averageStartTimeOffset;
                t->startTimeVarianceCounter += diff * diff;
            }
        }
        break;
    case DIAG_PHASE_END:
        break;
    }


    t->startTime = startTime;
}

static void TimeLevel_End(DiagnosticTimeLevel_t* t, DiagnosticPhase_t phase) {
    uint64_t endTime = GetGlobalTimer();

    switch (phase) {
    case DIAG_PHASE_WARMUP:
        break;
    case DIAG_PHASE_DURATION: {
        uint32_t duration = endTime - t->startTime;
        t->durationCounter += duration;

        if (t->averageDuration) {
        }

        t->counter += 1;
        break;
    }
    case DIAG_PHASE_DURATION_VARIANCE: {
        uint32_t duration = endTime - t->startTime;

        int32_t diff = (int32_t)duration - (int32_t)t->averageDuration;
        t->durationVarianceCounter += diff * diff;

        t->counter += 1;
        break;
    }
    case DIAG_PHASE_START_TIME_JITTER:

        break;
    case DIAG_PHASE_END:
        break;

    }

    uint32_t averageDuration = t->durationCounter / t->counter;
    uint32_t averageDurationVariance = t->durationVarianceCounter / t->counter;
    uint32_t averageStartTimeOffset = t->startTimeOffsetCounter / (t->counter - 1);
    uint32_t averageStartTimeVariance = t->startTimeVarianceCounter / (t->counter - 1);
    uint32_t newDurationAverage = ((uint64_t)t->averageDuration * t->samplingPeriodCount + averageDuration) / (t->samplingPeriodCount + 1);
    uint32_t newDurationVarianceAverage = ((uint64_t)t->averageDurationVariance * t->samplingPeriodCount + averageDurationVariance) / (t->samplingPeriodCount + 1);
    uint32_t newStartTimeOffsetAverage = ((uint64_t)t->averageStartTimeOffset * t->samplingPeriodCount + averageStartTimeOffset) / (t->samplingPeriodCount + 1);
    uint32_t newStartTimeVarianceAverage = ((uint64_t)t->averageStartTimeVariance * t->samplingPeriodCount + averageStartTimeVariance) / (t->samplingPeriodCount + 1);

    doNotOptimize = averageDuration;
    doNotOptimize = averageDurationVariance;
    doNotOptimize = averageStartTimeOffset;
    doNotOptimize = averageStartTimeVariance;
/*
    if (t->counter == SAMPLE_LIMIT) {
        if (t->durationSamplingPeriodCount) {
            // The first time recording averages
            t->averageDuration = averageDuration;
            t->averageStartTimeOffset = averageStartTimeOffset;
        }
        else {
            t->averageDuration = newDurationAverage;
        }
    }*/
}

void DIAGNOSTIC_Init(DiagnosticData_t* data)
{
    TimeLevel_Init(&data->t0);
    TimeLevel_Init(&data->t1);
    TimeLevel_Init(&data->t2);
    data->phase = DIAG_PHASE_WARMUP;
}

void DIAGNOSTIC_StartT0(DiagnosticData_t* data)
{
    TimeLevel_Start(&data->t0, data->phase);
}

void DIAGNOSTIC_EndT0(DiagnosticData_t* data)
{
    TimeLevel_End(&data->t0, data->phase);
}

void DIAGNOSTIC_StartT1(DiagnosticData_t* data);
void DIAGNOSTIC_EndT1(DiagnosticData_t* data);


void DIAGNOSTIC_StartT2(DiagnosticData_t* data);
void DIAGNOSTIC_EndT2(DiagnosticData_t* data);

void DIAGNOSTIC_NextPhase()
{

}
