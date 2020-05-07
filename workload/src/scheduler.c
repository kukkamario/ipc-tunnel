#include "scheduler.h"

#include "interrupt.h"
#include <xttcps.h>
#include <xparameters_ps.h>

static void TimerT0Interrupt(void* timerPtr);
static void Timer1MsInterrupt(void* timerPtr);

static XTtcPs f_timerT0;
static uint32_t f_t1Multiplier = 10;

static void ConfigureTTCTimer(
        XTtcPs* timer,
        uint16_t deviceId,
        uint32_t frequencyHz,
        InterruptNumber_t interruptId,
        InterruptPriority_t interruptPriority,
        Xil_InterruptHandler interruptHandler)
{
    XTtcPs_Config* config = XTtcPs_LookupConfig(deviceId);

    /* Stop the TTC by manually writing to registers. CfgInitialize
     * will fail if the counter is already running.
     *
     * The TTC may be running if CPU1 software is restarted without reseting
     * the CPU
     */
    Xil_Out32(config->BaseAddress + XTTCPS_CNT_CNTRL_OFFSET,
                Xil_In32(config->BaseAddress + XTTCPS_CNT_CNTRL_OFFSET)
              | XTTCPS_CNT_CNTRL_DIS_MASK);


    s32 initResult = XTtcPs_CfgInitialize(timer, config, config->BaseAddress);
    if (initResult != XST_SUCCESS) {
        xil_printf("XTtcPs_CfgInitialize failed: %i\n", initResult);
    }

    XInterval interval;
    u8 prescaler;

    XTtcPs_SetOptions(timer,
                        XTTCPS_OPTION_INTERVAL_MODE
                      | XTTCPS_OPTION_WAVE_DISABLE);

    XTtcPs_CalcIntervalFromFreq(timer, frequencyHz, &interval, &prescaler);

    XTtcPs_SetPrescaler(timer, prescaler);
    XTtcPs_SetInterval(timer, interval);

    xil_printf("Setup timer with prescaler: %i  interval: %i\r\n",
               prescaler,
               interval);

    /* Only trigger interrupt when interval timeout is reached */
    XTtcPs_EnableInterrupts(timer, XTTCPS_IXR_INTERVAL_MASK);

    INTERRUPT_RegisterHandler(
                interruptId,
                interruptHandler,
                timer);

    INTERRUPT_SetPriorityAndTriggerType(
                interruptId,
                interruptPriority,
                INTERRUPT_TRIGGER_TYPE_RISING_EDGE);

    INTERRUPT_BindSPIToThisCPU(interruptId);

    INTERRUPT_Enable(interruptId);
}

void SCHEDULER_Init(const SchedulerConfig_t* conf)
{
    f_t1Multiplier = conf->t1Multiplier;
    ConfigureTTCTimer(
            &f_timerT0,
            XPAR_XTTCPS_0_DEVICE_ID,
            f_t1Multiplier,
            INTERRUPT_SPI_TTC0_2,
            INTERRUPT_PRIORITY_SCHEDULER_TIMER,
            &TimerT0Interrupt);
}

static void TimerT0Interrupt(void* timerPtr)
{
    
}

static void Timer1MsInterrupt(void* timerPtr)
{
    
}
