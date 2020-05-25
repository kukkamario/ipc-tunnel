
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openamp/open_amp.h>
#include "platform_info.h"
#include <xil_printf.h>
#include <xtime_l.h>
#include <sleep.h>

#include "variant.h"
#include "scheduler.h"
#include "application.h"

#define LPRINTF(format, ...) xil_printf(format, ##__VA_ARGS__)
//#define LPRINTF(format, ...)
#define LPERROR(format, ...) LPRINTF("ERROR: " format, ##__VA_ARGS__)

static uint64_t totalReceiveTime = 0;


static const SchedulerConfig_t f_schedulerConfig = {
    .t0Frequency = 20000,
    .t1Multiplier = 4,
    .t2Multiplier = 20,
    
    .t0Task = &APPLICATION_T0,
    .t1Task = &APPLICATION_T1,
    .t2Task = &APPLICATION_T2
};

static uint8_t f_packetBuffer[0x1000];

static bool f_initialPacketReceived = false;
static void HandleInitialPacket(uint8_t* buf, uint32_t size, void* user) 
{
    f_initialPacketReceived = true;
}

static void Execute(void) {
    if (!APPLICATION_Init()) return;
    
    xil_printf("Waiting start packet\r\n");
    
    while (!f_initialPacketReceived) {
        VARIANT_ReadChan0(f_packetBuffer, sizeof(f_packetBuffer), &HandleInitialPacket, 0);
    }
    
    xil_printf("Starting scheduling\r\n");
    
    SCHEDULER_Init(&f_schedulerConfig);
    Xil_ExceptionEnable();
    xil_printf("Enabled exceptions\r\n");
    
    while(APPLICATION_Running()) {
        APPLICATION_BG();
    }
    
    
    xil_printf("Stopping scheduler\r\n");
    SCHEDULER_Stop();
}

/*-----------------------------------------------------------------------------*
 *  Application entry point
 *-----------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    void *platform;
    int ret;

    LPRINTF("Starting application...\r\n");

    /* Initialize platform */
    ret = platform_init(argc, argv, &platform);
    if (ret) {
        LPERROR("Failed to initialize platform.\r\n");
        ret = -1;
    } else {
        if (VARIANT_Initialize(platform)) {
            
            Execute();
            
            VARIANT_Destruct();
        }
    }

    LPRINTF("Stopping application...\r\n");
    platform_cleanup(platform);

    return ret;
}
