
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

static uint8_t replyBuffer[2048] __attribute__ ((aligned (16)));
static BaremetalToLinux* const reply = (BaremetalToLinux*)&replyBuffer;

static uint8_t buffer[2048]  __attribute__ ((aligned (16)));

static bool running = true;
unsigned f_step = 0;

void ReadCallback(uint8_t* buf, uint32_t packetSize, void* user)
{
    uint64_t timestamp;
    XTime_GetTime(&timestamp);
    
    LinuxToBaremetal* recv_data = (LinuxToBaremetal*)buf;

    reply->linux_to_baremetal_latency = timestamp - recv_data->send_timestamp;
    
    if (recv_data->control_flags & CONTROL_FLAG_SHUTDOWN) {
        LPERROR("Shutdown request\r\n");
        running = false;
    }
    else if (recv_data->control_flags & CONTROL_FLAG_NEXT) {
        ++f_step;
    }
    else {
        /* Wait a while cause Linux side to switch task */
        if ((f_step & 1) == 0) usleep(5000);

        XTime_GetTime(&reply->send_timestamp);
        if (!VARIANT_WriteChan0((const uint8_t*)reply, packetSize)) {
            xil_printf("rpmsg_send failed\r\n");
        }
    }

}

static void Test(void) {
    while (running) {
        VARIANT_ReadChan0(buffer, sizeof(buffer), &ReadCallback, NULL);
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
