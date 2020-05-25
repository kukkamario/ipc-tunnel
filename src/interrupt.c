/*
 * interrupt_internal.c
 *
 *  Created on: 24.10.2017
 *      Author: joona.poyhia
 */

#include "interrupt.h"
#include <xil_printf.h>

#include <xscugic.h>
#include <xscugic_hw.h>
#include <xscutimer.h>

/* --------------------------------------------------------------
 * MACRO DEFINITIONS
 * --------------------------------------------------------------
 */

/* --------------------------------------------------------------
 * PRIVATE VARIABLE DEFINITIONS
 * --------------------------------------------------------------
 */

extern XScuGic xInterruptController;

/* --------------------------------------------------------------
 * PRIVATE FUNCTION DECLARATIONS
 * --------------------------------------------------------------
 */

/* --------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * --------------------------------------------------------------
 */

void INTERRUPT_RegisterHandler(
        InterruptNumber_t interrupt,
        void (*handler)(void*),
        void* userData)
{
    XScuGic_Connect(&xInterruptController, interrupt, handler, userData);
}

void INTERRUPT_Enable(InterruptNumber_t interrupt)
{
    XScuGic_Enable(&xInterruptController, interrupt);
}

void INTERRUPT_Disable(InterruptNumber_t interrupt)
{
    XScuGic_Disable(&xInterruptController, interrupt);
}

void INTERRUPT_SetPriorityAndTriggerType(
        InterruptNumber_t interrupt,
        InterruptPriority_t priority,
        InterruptTriggerType_t triggerType)
{
    XScuGic_SetPriorityTriggerType(&xInterruptController, interrupt, priority, triggerType);
}

void INTERRUPT_BindSPIToThisCPU(InterruptNumber_t spiInterrupt)
{
    XScuGic_InterruptMaptoCpu(&xInterruptController, XPAR_CPU_ID, spiInterrupt);
}


void INTERRUPT_TriggerLocalSGI(InterruptNumber_t interrupt)
{
    Xil_AssertVoid(INTERRUPT_SGI_FIRST <= interrupt && interrupt <= INTERRUPT_SGI_LAST);

    uint32_t mask = ((2 << 16U) | interrupt) & (XSCUGIC_SFI_TRIG_CPU_MASK | XSCUGIC_SFI_TRIG_INTID_MASK);
    *(volatile uint32_t*)(XPAR_PS7_SCUGIC_0_DIST_BASEADDR + XSCUGIC_SFI_TRIG_OFFSET) = mask;
}

/*
void INTERRUPT_TriggerCPU0SGI(InterruptCpu0SGI_t interrupt)
{
    uint32_t mask = ((1 << 16U) | interrupt) & (XSCUGIC_SFI_TRIG_CPU_MASK | XSCUGIC_SFI_TRIG_INTID_MASK);
    *(volatile uint32_t*)(XPAR_PS7_SCUGIC_0_DIST_BASEADDR + XSCUGIC_SFI_TRIG_OFFSET) = mask;
}
*/

