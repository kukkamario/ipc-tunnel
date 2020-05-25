#include "scheduler.h"

#include <xttcps.h>
#include <xparameters_ps.h>
#include "interrupt.h"

#include <xscutimer.h>

static void TimerT0Interrupt(void* userData);

static void T1Interrupt(void* userData);
static void T2Interrupt(void* userData);

static XTtcPs f_timerT0;
static uint32_t f_t1Multiplier = 4;
static uint32_t f_t2Multiplier = 16;


static void(*f_t0Task)(void) = 0;
static void(*f_t1Task)(void) = 0;
static void(*f_t2Task)(void) = 0;


static void ConfigureTTCTimer(
        uint32_t frequencyHz,
        InterruptPriority_t interruptPriority,
        void (*interruptHandler)(void*),
        void* userData)
{
    XTtcPs_Config* config = XTtcPs_LookupConfig(XPAR_XTTCPS_2_DEVICE_ID);
    InterruptNumber_t interruptId = INTERRUPT_SPI_TTC0_2;

    /* Stop the TTC by manually writing to registers. CfgInitialize
     * will fail if the counter is already running.
     *
     * The TTC may be running if CPU1 software is restarted without reseting
     * the CPU
     */
    Xil_Out32(config->BaseAddress + XTTCPS_CNT_CNTRL_OFFSET,
                Xil_In32(config->BaseAddress + XTTCPS_CNT_CNTRL_OFFSET)
              | XTTCPS_CNT_CNTRL_DIS_MASK);


    s32 initResult = XTtcPs_CfgInitialize(&f_timerT0, config, config->BaseAddress);
    if (initResult != XST_SUCCESS) {
        xil_printf("XTtcPs_CfgInitialize failed: %i\n", initResult);
    }

    XInterval interval;
    u8 prescaler;

    XTtcPs_SetOptions(&f_timerT0,
                        XTTCPS_OPTION_INTERVAL_MODE
                      | XTTCPS_OPTION_WAVE_DISABLE);

    XTtcPs_CalcIntervalFromFreq(&f_timerT0, frequencyHz, &interval, &prescaler);

    XTtcPs_SetPrescaler(&f_timerT0, prescaler);
    XTtcPs_SetInterval(&f_timerT0, interval);

    xil_printf("Setup timer with prescaler: %i  interval: %i\r\n",
               prescaler,
               interval);

    /* Only trigger interrupt when interval timeout is reached */
    XTtcPs_EnableInterrupts(&f_timerT0, XTTCPS_IXR_INTERVAL_MASK);

    INTERRUPT_RegisterHandler(interruptId, interruptHandler, userData);
    
    INTERRUPT_SetPriorityAndTriggerType(
                interruptId,
                interruptPriority,
                INTERRUPT_TRIGGER_TYPE_HIGH);

    INTERRUPT_BindSPIToThisCPU(interruptId);

    INTERRUPT_Enable(interruptId);
}


#define XSCUTIMER_CLOCK_HZ ( XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 2UL )

XScuTimer f_scuTimer;

static void ConfigureScuTimer(uint32_t frequencyHz,
                              InterruptPriority_t interruptPriority,
                              void (*interruptHandler)(void*),
                              void* userData) {
    XScuTimer_Config* config = XScuTimer_LookupConfig(XPAR_SCUTIMER_DEVICE_ID);
    XScuTimer_CfgInitialize(&f_scuTimer, config, config->BaseAddr);
    
    XScuTimer_EnableAutoReload(&f_scuTimer);
    XScuTimer_SetPrescaler(&f_scuTimer, 0);
    XScuTimer_LoadTimer(&f_scuTimer, XSCUTIMER_CLOCK_HZ / frequencyHz);
    
    INTERRUPT_RegisterHandler(INTERRUPT_SCU_TIMER, interruptHandler, userData);
    INTERRUPT_Enable(INTERRUPT_SCU_TIMER);
    XScuTimer_Start(&f_scuTimer);
    
    INTERRUPT_SetPriorityAndTriggerType(INTERRUPT_SCU_TIMER, interruptPriority, INTERRUPT_TRIGGER_TYPE_HIGH);
    
    XScuTimer_ClearInterruptStatus( &f_scuTimer );
    XScuTimer_EnableInterrupt(&f_scuTimer);
}

void SCHEDULER_Init(const SchedulerConfig_t* conf)
{
    INTERRUPT_CriticalSection {
        f_t1Multiplier = conf->t1Multiplier;
        f_t2Multiplier = conf->t2Multiplier;
        
        f_t0Task = conf->t0Task;
        f_t1Task = conf->t1Task;
        f_t2Task = conf->t2Task;
        /*ConfigureTTCTimer(
                conf->t0Frequency,
                INTERRUPT_PRIORITY_SCHEDULER_TIMER,
                &TimerT0Interrupt,
                conf->t0Task);*/
        
        ConfigureScuTimer(conf->t0Frequency,
                          INTERRUPT_PRIORITY_SCHEDULER_TIMER,
                          &TimerT0Interrupt,
                          conf->t0Task);
        
        
        INTERRUPT_RegisterHandler(INTERRUPT_SGI_T1,
                                  &T1Interrupt,
                                  conf->t1Task);
    
        INTERRUPT_SetPriorityAndTriggerType(
                    INTERRUPT_SGI_T1,
                    INTERRUPT_PRIORITY_T1,
                    INTERRUPT_TRIGGER_TYPE_HIGH);
        
        INTERRUPT_Enable(INTERRUPT_SGI_T1);
        
        INTERRUPT_RegisterHandler(INTERRUPT_SGI_T2,
                                  &T2Interrupt,
                                  conf->t2Task);
    
        INTERRUPT_SetPriorityAndTriggerType(
                    INTERRUPT_SGI_T2,
                    INTERRUPT_PRIORITY_T2,
                    INTERRUPT_TRIGGER_TYPE_HIGH);
        
        INTERRUPT_Enable(INTERRUPT_SGI_T2);
        
        /*XTtcPs_ResetCounterValue(&f_timerT0);
        XTtcPs_Start(&f_timerT0);*/
    }
}


void SCHEDULER_Stop(void)
{
    INTERRUPT_CriticalSection {
        XScuTimer_Stop(&f_scuTimer);
        XScuTimer_ClearInterruptStatus( &f_scuTimer );
        XScuTimer_DisableInterrupt(&f_scuTimer);
        INTERRUPT_Disable(INTERRUPT_SCU_TIMER);
    }
}

static uint32_t t1Counter = 0;
static uint32_t t2Counter = 0;

static void TimerT0Interrupt(void* userData)
{
    XScuTimer_ClearInterruptStatus( &f_scuTimer );
    /*XTtcPs_ResetCounterValue(&f_timerT0);
    uint32_t timerEvent = XTtcPs_GetInterruptStatus(&f_timerT0);
    XTtcPs_ClearInterruptStatus(&f_timerT0, timerEvent);*/

    
    f_t0Task();
    
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
}

static void T1Interrupt(void* userData)
{
    Xil_EnableNestedInterrupts();
    f_t1Task();
    Xil_DisableNestedInterrupts();
}

static void T2Interrupt(void* userData)
{
    Xil_EnableNestedInterrupts();
    f_t2Task();
    Xil_DisableNestedInterrupts();
}
