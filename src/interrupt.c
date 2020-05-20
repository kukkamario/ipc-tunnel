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

#include <metal/irq.h>

/* --------------------------------------------------------------
 * MACRO DEFINITIONS
 * --------------------------------------------------------------
 */

/* --------------------------------------------------------------
 * PRIVATE VARIABLE DEFINITIONS
 * --------------------------------------------------------------
 */

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
        int (*handler)(int, void*),
        void* userData)
{
    metal_irq_register(interrupt, handler, userData);
}

void INTERRUPT_Enable(InterruptNumber_t interrupt)
{
    metal_irq_enable(interrupt);
}

void INTERRUPT_Disable(InterruptNumber_t interrupt)
{
    metal_irq_disable(interrupt);
}




void INTERRUPT_SetPriorityAndTriggerType(
        InterruptNumber_t interrupt,
        InterruptPriority_t priority,
        InterruptTriggerType_t triggerType)
{
    XScuGic_SetPriTrigTypeByDistAddr(XPAR_SCUGIC_DIST_BASEADDR, interrupt, priority, triggerType);
}

void INTERRUPT_BindSPIToThisCPU(InterruptNumber_t spiInterrupt)
{
    /* This function should only be used with shared peripheral interrupts */
    Xil_AssertVoid(INTERRUPT_SPI_FIRST <= spiInterrupt && spiInterrupt <= INTERRUPT_SPI_LAST);

    uint32_t Cpu_Id = XPAR_CPU_ID;
    
    volatile uint32_t* spiTargetRegisterPtr = (volatile uint32_t*)(
                XPAR_SCUGIC_DIST_BASEADDR + XSCUGIC_SPI_TARGET_OFFSET_CALC(spiInterrupt));
    
    u32 RegValue, Offset;
	RegValue = *spiTargetRegisterPtr;

	Offset = (spiInterrupt & 0x3U);
	Cpu_Id = (0x1U << 1);

	RegValue = (RegValue & (~(0xFFU << (Offset*8U))) );
	RegValue |= ((Cpu_Id) << (Offset*8U));

	*spiTargetRegisterPtr = RegValue;
}


void INTERRUPT_TriggerLocalSGI(InterruptNumber_t interrupt)
{
    Xil_AssertVoid(INTERRUPT_SGI_FIRST <= interrupt && interrupt <= INTERRUPT_SGI_LAST);

    uint32_t mask = ((2 << 16U) | interrupt) & (XSCUGIC_SFI_TRIG_CPU_MASK | XSCUGIC_SFI_TRIG_INTID_MASK);
    *(volatile uint32_t*)(XPAR_PS7_SCUGIC_0_DIST_BASEADDR + XSCUGIC_SFI_TRIG_OFFSET) = mask;
}

void INTERRUPT_TriggerCPU0SGI(InterruptCpu0SGI_t interrupt)
{
    uint32_t mask = ((1 << 16U) | interrupt) & (XSCUGIC_SFI_TRIG_CPU_MASK | XSCUGIC_SFI_TRIG_INTID_MASK);
    *(volatile uint32_t*)(XPAR_PS7_SCUGIC_0_DIST_BASEADDR + XSCUGIC_SFI_TRIG_OFFSET) = mask;
}


