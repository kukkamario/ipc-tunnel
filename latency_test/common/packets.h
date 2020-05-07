#ifndef LATENCY_TEST_PACKETS_H
#define LATENCY_TEST_PACKETS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum {
    CONTROL_FLAG_SHUTDOWN = 1
};

typedef struct {
    uint64_t send_timestamp;
    uint64_t control_flags;
    int dummyData1;
    int dummyData2;
    int dummyData3;
    int dummyData4;
} LinuxToBaremetal;

typedef struct {
    uint64_t send_timestamp;
    uint64_t linux_to_baremetal_latency;
    int dummyData;
} BaremetalToLinux;

#ifdef __cplusplus
}
#endif


#endif  // LATENCY_TEST_PACKETS_H
