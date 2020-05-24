#ifndef LATENCY_TEST_PACKETS_H
#define LATENCY_TEST_PACKETS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum {
    CONTROL_FLAG_SHUTDOWN = 1,
    CONTROL_FLAG_NEXT = 2,
};

typedef struct {
    uint64_t send_timestamp;
    uint32_t control_flags;
    uint32_t packet_id;
} LinuxToBaremetal;

typedef struct {
    uint64_t send_timestamp;
    uint64_t linux_to_baremetal_latency;
    uint32_t control_flags;
    uint32_t packet_id;
} BaremetalToLinux;

#ifdef __cplusplus
}
#endif


#endif  // LATENCY_TEST_PACKETS_H
