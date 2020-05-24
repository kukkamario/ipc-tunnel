
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openamp/open_amp.h>
#include "platform_info.h"
#include <xil_printf.h>
#include <xtime_l.h>
#include <sleep.h>
#include "packets.h"

#include "variant.h"

#define LPRINTF(format, ...) xil_printf(format, ##__VA_ARGS__)
//#define LPRINTF(format, ...)
#define LPERROR(format, ...) LPRINTF("ERROR: " format, ##__VA_ARGS__)

#define MAX_RECV_LATENCIES 10000

BaremetalToLinux reply;

static uint8_t buffer[1024 * 16]  __attribute__ ((aligned (16)));
static uint32_t recvLatencies[MAX_RECV_LATENCIES];
static uint32_t recvLatencyCount = 0;

static int phase = 0;
static int packetCounter = 0;
static int sendPacketSize = 0;
static int sendPacketCount = 0;

static uint64_t totalReceiveTime = 0;
static bool f_running = true;

static void Phase0Read(uint8_t* buf, uint32_t packetSize, void* user) {
    uint64_t timestamp;
    XTime_GetTime(&timestamp);
    
    if (totalReceiveTime == 0) {
        totalReceiveTime = timestamp;
    }

    LinuxToBaremetal* recv_data = (LinuxToBaremetal*)buf;
    ++packetCounter;
    
    if (recvLatencyCount < MAX_RECV_LATENCIES) {
        recvLatencies[recvLatencyCount] = (uint32_t)(timestamp - recv_data->send_timestamp);
        recvLatencyCount++;
    }
    
    if (recv_data->control_flags & CONTROL_FLAG_NEXT) {
        phase = 1;
        sendPacketSize = packetSize;
        totalReceiveTime = timestamp - totalReceiveTime;
        
        //xil_printf("CONTROL_FLAG_NEXT received, moving to phase=1, control_flags: %x\r\n", (uint32_t)recv_data->control_flags);
        usleep(50000);
    }
    else if (recv_data->control_flags & CONTROL_FLAG_SHUTDOWN) {
        LPERROR("Shutdown request, control_flags: %x\r\n", (uint32_t)recv_data->control_flags);
        f_running = false;
    }
}

static void Phase2Read(uint8_t* buf, uint32_t packetSize, void* user) 
{
    LinuxToBaremetal* recv_data = (LinuxToBaremetal*)buf;
    
    if (recv_data->control_flags & CONTROL_FLAG_NEXT) {
        sendPacketCount = 0;
        recvLatencyCount = 0;
        phase = 0;
        packetCounter = 0;
        
        //xil_printf("CONTROL_FLAG_NEXT received, moving to phase=0, control_flags: %x\r\n", (uint32_t)recv_data->control_flags);
        
        BaremetalToLinux* packet = (BaremetalToLinux*)buffer;
        XTime_GetTime(&packet->send_timestamp);
        packet->control_flags = CONTROL_FLAG_NEXT;
        packet->linux_to_baremetal_latency = totalReceiveTime;
        
        totalReceiveTime = 0;
        
        while (!VARIANT_WriteChan0(buffer, sendPacketSize)) { }
    }
    else {
        if (recv_data->control_flags & CONTROL_FLAG_SHUTDOWN) {
            LPERROR("Shutdown request, control_flags: %x\r\n", (uint32_t)recv_data->control_flags);
            f_running = false;
        }
    }
}

static void Test(void) {
    f_running = true;
    xil_printf("REMOTE: Starting test\r\n");
    
    while (f_running) {
        
        if (phase == 0) {
            VARIANT_ReadChan0(buffer, sizeof(buffer), &Phase0Read, NULL);
        }
        else if (phase == 1) {
            
            BaremetalToLinux* packet = (BaremetalToLinux*)buffer;
            XTime_GetTime(&packet->send_timestamp);
            if (sendPacketCount < MAX_RECV_LATENCIES) {
                packet->linux_to_baremetal_latency = recvLatencies[sendPacketCount];
                
            }
            else {
                packet->linux_to_baremetal_latency = 0;
            }
            
            sendPacketCount += 1;
            
            packet->control_flags = 0;
            if (sendPacketCount == packetCounter) {
                packet->control_flags |= CONTROL_FLAG_NEXT;
                phase = 2;
                
                //xil_printf("All packets sent, moving to phase=2\r\n");
            }
            
            /* retry send as many times as it is necessary */
            while (!VARIANT_WriteChan0(buffer, sendPacketSize)) { }
        }
        else if (phase == 2) {
            VARIANT_ReadChan0(buffer, sizeof(buffer), &Phase2Read, NULL);
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  Application entry point
 *-----------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    void *platform;
    struct rpmsg_device *rpdev;
    int ret;

    LPRINTF("Starting application...\r\n");

    /* Initialize platform */
    ret = platform_init(argc, argv, &platform);
    if (ret) {
        LPERROR("Failed to initialize platform.\r\n");
        ret = -1;
    } else {
        if (VARIANT_Initialize(platform)) {
            
            Test();
            
            VARIANT_Destruct();
        }
    }

    LPRINTF("Stopping application...\r\n");
    platform_cleanup(platform);

    return ret;
}
