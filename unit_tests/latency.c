
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

BaremetalToLinux reply;

static uint8_t buffer[1024]  __attribute__ ((aligned (16)));

static void Test(void) {
    bool running = true;
    while (running) {
        uint32_t packetSize = VARIANT_ReadChan0(buffer, sizeof(buffer));
        
        if (packetSize == sizeof(LinuxToBaremetal)) {
            uint64_t timestamp;
            XTime_GetTime(&timestamp);
            
            LinuxToBaremetal* recv_data = (LinuxToBaremetal*)buffer;
    
            reply.linux_to_baremetal_latency = timestamp - recv_data->send_timestamp;
            
            if (recv_data->control_flags & CONTROL_FLAG_SHUTDOWN) {
                LPERROR("Shutdown request\r\n");
                running = false;
            }
            else {
                /* Wait a moment to allow linux app to invoke read to get the best possible latency */
                usleep(5000);
    
                XTime_GetTime(&reply.send_timestamp);
                if (!VARIANT_WriteChan0((const uint8_t*)&reply, sizeof(reply))) {
                    xil_printf("rpmsg_send failed\r\n");
                }
            }
            
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
