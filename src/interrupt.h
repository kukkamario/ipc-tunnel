
#ifndef WORKLOAD_INTERRUPT_H
#define WORKLOAD_INTERRUPT_H

#include <xparameters.h>
#include <xparameters_ps.h>
#include <xil_exception.h>
#include <stdint.h>
/* --------------------------------------------------------------
 * PUBLIC MACRO DEFINITIONS
 * --------------------------------------------------------------
 */

/* Lowest 3 bits of 8bit interrupt priority are unused on Zynq7 so interrupt
 * priorities have to be shifted by 3
 */
#define INTERRUPT_PRIORITY_SHIFT 3

/* Prefer INTERRUPT_CriticalSection because these don't take account
 * nested critical sections
*/
#define INTERRUPT_EXCEPTION_ENABLE() Xil_ExceptionEnable()
#define INTERRUPT_EXCEPTION_DISABLE() Xil_ExceptionDisable()

/* --------------------------------------------------------------
 * PUBLIC TYPE DEFINITIONS
 * --------------------------------------------------------------
 */

typedef enum {
    /* First software generated interrupt */
    INTERRUPT_SGI_FIRST = 0,

    INTERRUPT_SGI_T0 = 1,
    INTERRUPT_SGI_T1 = 2,
    INTERRUPT_SGI_T2 = 3,

    /* Last software generated interrupt */
    INTERRUPT_SGI_LAST = 15,

    /* First private peripheral interrupt */
    INTERRUPT_PPI_FIRST = 27,

    INTERRUPT_SCU_TIMER = XPAR_SCUTIMER_INTR,
    INTERRUPT_PPI_SCU_WDT = XPS_SCU_WDT_INT_ID,

    /* Last private peripheral interrupt */
    INTERRUPT_PPI_LAST = 31,

    /* First shared peripheral interrupt */
    INTERRUPT_SPI_FIRST = 32,
    
    /* TTC0_0 and TTC0_1 are reserved by Linux running on CPU0 */
    INTERRUPT_SPI_TTC0_2 = XPS_TTC0_2_INT_ID,

    INTERRUPT_SPI_TTC1_0 = XPS_TTC1_0_INT_ID,
    INTERRUPT_SPI_TTC1_1 = XPS_TTC1_1_INT_ID,
    INTERRUPT_SPI_TTC1_2 = XPS_TTC1_2_INT_ID,

    /* Last shared peripheral interrupt */
    INTERRUPT_SPI_LAST = 91,
    
    /* Invalid interrupt number that some functions accept as to signify that
     * interrupt shouldn't be used */
    INTERRUPT_INVALID = 0xFF
} InterruptNumber_t;

/* Lower priority value means higher priority */
typedef enum {
    /* Timer or PWM interrupt for T0 */
    INTERRUPT_PRIORITY_T0 = 1 << INTERRUPT_PRIORITY_SHIFT,

    /* SGI interrupt for T1*/
    INTERRUPT_PRIORITY_T1 = 2 << INTERRUPT_PRIORITY_SHIFT,

    INTERRUPT_PRIORITY_T2 = 3 << INTERRUPT_PRIORITY_SHIFT,

    /* when timer is used to provide T0 scheduling, timer interrupt has this priority */
    INTERRUPT_PRIORITY_SCHEDULER_TIMER = 0,
} InterruptPriority_t;

typedef enum {
    INTERRUPT_TRIGGER_TYPE_HIGH = 1,
    INTERRUPT_TRIGGER_TYPE_RISING_EDGE = 3
} InterruptTriggerType_t;


/* --------------------------------------------------------------
 * PUBLIC FUNCTION DECLARATIONS
 * --------------------------------------------------------------
 */

extern void INTERRUPT_RegisterHandler(
        InterruptNumber_t interrupt,
        void (*handler)(void*),
        void* userData);

extern void INTERRUPT_Enable(InterruptNumber_t interrupt);

extern void INTERRUPT_Disable(InterruptNumber_t interrupt);

/** Sets priority and trigger type for an interrupt
 *
 * @param interrupt Interrupt that should be configured
 * @param priority Priority of the interrupt
 * @param triggerType Trigger type of the interrupt.
 *                    Many interrupts require specific trigger type.
 *                    See https://www.xilinx.com/support/documentation/user_guides/ug585-Zynq-7000-TRM.pdf
 *                    for correct trigger types
 */
extern void INTERRUPT_SetPriorityAndTriggerType(
        InterruptNumber_t interrupt,
        InterruptPriority_t priority,
        InterruptTriggerType_t triggerType);


/** Binds shared peripheral interrupt to only trigger on this CPU */
extern void INTERRUPT_BindSPIToThisCPU(InterruptNumber_t spiInterrupt);

/** Triggers software generated interrupt on this CPU */
extern void INTERRUPT_TriggerLocalSGI(InterruptNumber_t interrupt);

/** Disables IRQ and FIQ exceptions and returns the previous state of the CPSR register */
static inline uint32_t INTERRUPT_EnterCritical(void){
    uint32_t old = mfcpsr();
    mtcpsr(old | XIL_EXCEPTION_ALL);
    return old;
}

/** Restores CPSR register from given IRQ and FIQ state  */
static inline void INTERRUPT_ExitCritical(uint32_t state)
{
    /* Restore only IRQ and FIQ mask bits */
    mtcpsr((mfcpsr() & ~XIL_EXCEPTION_ALL) | (state & XIL_EXCEPTION_ALL));
}

/** Syntax sugar for Enter & ExitCritical section
 * 
 * Usage:
 *   INTERRUPT_CriticalSection {
 *     Do stuff requiring critical section here
 *   }
*/
#define INTERRUPT_CriticalSection \
    for (uint32_t _criticalSectionState ## __LINE__ = INTERRUPT_EnterCritical(); \
         _criticalSectionState ## __LINE__; \
         INTERRUPT_ExitCritical(_criticalSectionState ## __LINE__), (_criticalSectionState ## __LINE__) = 0)


#endif /* WORKLOAD_INTERRUPT_H */
