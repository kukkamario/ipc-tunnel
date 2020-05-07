/*
 * interrupt_internal.c
 *
 *  Created on: 24.10.2017
 *      Author: joona.poyhia
 */

#include "interrupt.h"
#include <xil_printf.h>

#include <xscugic.h>

/* --------------------------------------------------------------
 * MACRO DEFINITIONS
 * --------------------------------------------------------------
 */

/* --------------------------------------------------------------
 * PRIVATE VARIABLE DEFINITIONS
 * --------------------------------------------------------------
 */

/* Defined inside freertos10 port (portZynq7000.c) */
extern XScuGic xInterruptController;

/* --------------------------------------------------------------
 * PRIVATE FUNCTION DECLARATIONS
 * --------------------------------------------------------------
 */

static void InterruptDefaultHandler(void* userData);

/* --------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * --------------------------------------------------------------
 */

void INTERRUPT_Init(void)
{
    xil_printf("\r\n\nREMOTE: INTERRUPTInit()\r\n");

    XScuGic_Config *XScuGicConfig;
    int status = 0;

    /* Initialize the GIC */
    XScuGicConfig = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID);

    status = XScuGic_CfgInitialize(
                &xInterruptController,
                XScuGicConfig,
                XScuGicConfig->CpuBaseAddress);

    if (status != XST_SUCCESS)
    {
        xil_printf("XScuGic_CfgInitialize failed: %i\r\n", status);
    }
    
    /* Set default handler for all interrupts */
    for (uint32_t i = 0; i < XSCUGIC_MAX_NUM_INTR_INPUTS; ++i)
    {
        XScuGic_Connect(
                    &xInterruptController,
                    i, /* interrupt number */
                    &InterruptDefaultHandler,
                    (void*)(intptr_t)i);

    }

    xil_printf("REMOTE: INTERRUPTInit done\r\n");
}


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
    XScuGic_SetPriorityTriggerType(
                &xInterruptController,
                interrupt,
                priority,
                triggerType);
}

void INTERRUPT_BindSPIToThisCPU(InterruptNumber_t spiInterrupt)
{
    /* This function should only be used with shared peripheral interrupts */
    Xil_AssertVoid(INTERRUPT_SPI_FIRST <= spiInterrupt && spiInterrupt <= INTERRUPT_SPI_LAST);

    XScuGic_InterruptMaptoCpu(&xInterruptController, XPAR_CPU_ID, spiInterrupt);
}


void INTERRUPT_TriggerLocalSGI(InterruptNumber_t interrupt)
{
    Xil_AssertVoid(INTERRUPT_SGI_FIRST <= interrupt && interrupt <= INTERRUPT_SGI_LAST);

    XScuGic_SoftwareIntr(&xInterruptController, interrupt, 0x2);
}

void INTERRUPT_TriggerCPU0SGI(InterruptCpu0SGI_t interrupt)
{
    XScuGic_SoftwareIntr(&xInterruptController, interrupt, 0x1);
}

/* --------------------------------------------------------------
 * PRIVATE FUNCTION DEFINITIONS
 * --------------------------------------------------------------
 */

static void InterruptDefaultHandler(void* userData)
{
    uint32_t interruptNum = (uint32_t)(intptr_t)userData;
}

