#include "scheduler.h"

#include <xttcps.h>
#include <xparameters_ps.h>
#include "interrupt.h"

static int TimerT0Interrupt(int irq, void* userData);

static int T1Interrupt(int irq, void* userData);
static int T2Interrupt(int irq, void* userData);

static XTtcPs f_timerT0;
static uint32_t f_t1Multiplier = 4;
static uint32_t f_t2Multiplier = 16;


static void(*f_t0Task)(void) = 0;
static void(*f_t1Task)(void) = 0;
static void(*f_t2Task)(void) = 0;


static void ConfigureTTCTimer(
        XTtcPs* timer,
        uint16_t deviceId,
        uint32_t frequencyHz,
        InterruptNumber_t interruptId,
        InterruptPriority_t interruptPriority,
        int (*interruptHandler)(int, void*),
        void* userData)
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

    INTERRUPT_RegisterHandler(interruptId, interruptHandler, userData);
    
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
    f_t2Multiplier = conf->t2Multiplier;
    
    f_t0Task = conf->t0Task;
    f_t1Task = conf->t1Task;
    f_t2Task = conf->t2Task;
    ConfigureTTCTimer(
            &f_timerT0,
            XPAR_XTTCPS_0_DEVICE_ID,
            conf->t0Frequency,
            INTERRUPT_SPI_TTC0_2,
            INTERRUPT_PRIORITY_SCHEDULER_TIMER,
            &TimerT0Interrupt,
            conf->t0Task);
    
    INTERRUPT_RegisterHandler(INTERRUPT_SGI_T1,
                              &T1Interrupt,
                              conf->t1Task);

    INTERRUPT_SetPriorityAndTriggerType(
                INTERRUPT_SGI_T1,
                INTERRUPT_PRIORITY_T1,
                INTERRUPT_TRIGGER_TYPE_RISING_EDGE);
    
    INTERRUPT_Enable(INTERRUPT_SGI_T1);
    
    INTERRUPT_RegisterHandler(INTERRUPT_SGI_T2,
                              &T2Interrupt,
                              conf->t2Task);

    INTERRUPT_SetPriorityAndTriggerType(
                INTERRUPT_SGI_T2,
                INTERRUPT_PRIORITY_T2,
                INTERRUPT_TRIGGER_TYPE_RISING_EDGE);
    
    INTERRUPT_Enable(INTERRUPT_SGI_T2);
}

static uint32_t t1Counter = 0;
static uint32_t t2Counter = 0;

static int TimerT0Interrupt(int irq, void* userData)
{
    int (*task)(void) = userData;
    
    ++t1Counter;
    ++t2Counter;
    
    if (t1Counter == f_t1Multiplier) {
        t1Counter = 0;
        INTERRUPT_TriggerLocalSGI(INTERRUPT_SGI_T1);
    }
    
    if (t2Counter == f_t2Multiplier) {
        t2Counter = 0;
        INTERRUPT_TriggerLocalSGI(INTERRUPT_SGI_T2);
    }
    return 1;
}

static int T1Interrupt(int irq, void* userData)
{
    (void)irq;
    Xil_EnableNestedInterrupts();
    int (*task)(void) = userData;
    task();
    Xil_DisableNestedInterrupts();
    
    return 1;
}

static int T2Interrupt(int irq, void* userData)
{
    (void)irq;
    Xil_EnableNestedInterrupts();
    int (*task)(void) = userData;
    
    task();
    Xil_DisableNestedInterrupts();
    
    return 1;
}
