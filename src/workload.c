#include "workload.h"
#include <xparameters.h>
#include <stdint.h>
#include <xtime_l.h>
#include <string.h>
#include <math.h>
#include <xil_printf.h>

#include "variant.h"
#include "shared_state.h"

static uint64_t GlobalTimer() {
    uint32_t low, high;
    
    do {
        high = *(volatile uint32_t*)(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_UPPER_OFFSET);
        low = *(volatile uint32_t*)(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
    } while (*(volatile uint32_t*)(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_UPPER_OFFSET) != high);
    return (((uint64_t) high) << 32u) | (uint64_t) low;
}

uint8_t f_t0PacketBuffer[0x780] __attribute__ ((aligned (8)));
uint8_t f_t1PacketBuffer[0x780] __attribute__ ((aligned (8)));
uint8_t f_t2PacketBuffer[0x780] __attribute__ ((aligned (8)));


static SharedState_Variables s_variables;


#define T0_WORK_DIFFICULTY 1
#define T1_WORK_DIFFICULTY 1
#define T2_WORK_DIFFICULTY 1

static void Do_T0_Work(void) {
    /* This is just random calculations to simulate real workload */
    
    static double s_counter = 1.41;
    
    for (int j = 0; j < T0_WORK_DIFFICULTY; ++j) {
        for (int i = 0; i < SHAREDSTATE_VARIABLE_COUNT; ++i) {
            s_variables.vd[i] += 2.1 * s_variables.vd[i] + s_counter + 231.4;
            s_counter += 0.14;
            
            if (s_counter > 50.0) {
                s_counter = -23.4;
            }
            
            if (s_variables.vd[i] > 10000.0 || s_variables.vd[i] < -10000.0) {
                s_variables.vd[i] = sin(s_variables.vd[i] / 10000.0);
                
                
                s_variables.vs[i] += 1;
            }
            
            s_variables.vb[(s_variables.vs[i] * 123 + 13) % SHAREDSTATE_VARIABLE_COUNT] += 1;
            s_variables.vf[i] += s_variables.vb[(int)(s_counter * s_variables.vf[s_variables.vb[i]]) % SHAREDSTATE_VARIABLE_COUNT] * s_counter;
            
            if (s_variables.vf[i] > 2143515.0f || s_variables.vf[i] < -125412.0f) {
                s_variables.vf[i] = 123.452 * s_counter;
            }
        }
    }
}

static void Do_T1_Work(void) {
    static volatile double s_workStuff = 1.0;
    
    double var = s_workStuff;
    for (int i = 0; i < T1_WORK_DIFFICULTY; ++i) {
        var += 123.0;
        

        if (var > 1512436.0) {
            var = 123.0 - i;
        }
        
        var = cos(var) * 0.65 * s_workStuff;
        
        
    }
    
    if (var > -32165132.0 && var < 1234239051597.0) {
        s_workStuff = var;
    }
    else {
        s_workStuff = 634.0;
    }
}

static void Do_T2_Work(void) {
    static volatile double s_workStuff = 1.0;
    
    double var = s_workStuff;
    for (int i = 0; i < T2_WORK_DIFFICULTY; ++i) {
        var += 123.0;

        if (var > 1512436.0) {
            var = 123.0 - i;
        }
        
        var = cos(var) * 0.65 * s_workStuff;
        
        
    }
    
    if (var > -32165132.0 && var < 1234239051597.0) {
        s_workStuff = var;
    }
    else {
        s_workStuff = 634.0;
    }
}

static SharedState_T0DataPacket s_t0Packet;
static SharedState_T1DataPacket s_t1Packet;
static SharedState_T2DataPacket s_t2Packet;
static uint64_t s_t0StartTime;

static void HandleT0Packet(uint8_t* data, uint32_t size, void* user)
{
    uint64_t receiveTime = GlobalTimer();
    SharedState_T0CommandPacket* packet = (SharedState_T0CommandPacket*)data;
    s_t0Packet.stats.commandPacketId = packet->packetId;
    s_t0Packet.stats.commandPacketLatency = receiveTime - packet->timestamp;
    s_variables.lastSetPacketId  = packet->packetId;
    s_variables.lastSetReceiveTimestamp = receiveTime;
    s_variables.lastSetSendTimestamp = packet->timestamp;
    
    const uint8_t* variableSetPtr = data + sizeof(SharedState_T0CommandPacket);
    for (int i = 0; i < packet->varSetCommands; ++i) {
        const SharedState_VariableSet* setCmd = (const SharedState_VariableSet*)variableSetPtr;
        variableSetPtr += sizeof(SharedState_VariableSet);
        switch (setCmd->variableType) {
        case SHAREDSTATE_VAR_DOUBLE:
            memcpy(&s_variables.vd[setCmd->variableId], variableSetPtr, sizeof(double));
            variableSetPtr += sizeof(double);
            break;
        case SHAREDSTATE_VAR_FLOAT:
            memcpy(&s_variables.vf[setCmd->variableId], variableSetPtr, sizeof(float));
            variableSetPtr += sizeof(float);
            break;
        case SHAREDSTATE_VAR_SHORT:
            memcpy(&s_variables.vs[setCmd->variableId], variableSetPtr, sizeof(uint16_t));
            variableSetPtr += sizeof(uint16_t);
            break;
        case SHAREDSTATE_VAR_BYTE:
            memcpy(&s_variables.vb[setCmd->variableId], variableSetPtr, sizeof(uint8_t));
            variableSetPtr += sizeof(uint8_t);
            break;
        }
    }
}

static void HandleT1Packet(uint8_t* data, uint32_t size, void* user)
{
    uint64_t receiveTime = GlobalTimer();
    SharedState_T1CommandPacket* packet = (SharedState_T1CommandPacket*)data;
    
    s_t1Packet.stats.commandPacketId = packet->packetId;
    s_t1Packet.stats.commandPacketLatency = receiveTime - packet->timestamp;
}

static void HandleT2Packet(uint8_t* data, uint32_t size, void* user)
{
    uint64_t receiveTime = GlobalTimer();
    SharedState_T2CommandPacket* packet = (SharedState_T2CommandPacket*)data;
    
    s_t2Packet.stats.commandPacketId = packet->packetId;
    s_t2Packet.stats.commandPacketLatency = receiveTime - packet->timestamp;
}

void WORKLOAD_Init()
{
    xil_printf("WORKLOAD_Init\r\n");
    memset(&s_variables, 0, sizeof(s_variables));
    memset(&s_t0Packet, 0, sizeof(s_t0Packet));
}

void WORKLOAD_T0(void)
{
    if (s_t0Packet.stats.iterationNumber == 0) {
        xil_printf("T0 start\r\n");
    }
    
    s_t0StartTime = GlobalTimer();
    Do_T0_Work();
    
    VARIANT_ReadChan0(f_t0PacketBuffer, sizeof(f_t0PacketBuffer), &HandleT0Packet, NULL);
    
    memmove(&s_t0Packet.stats.timeLevelStartTimes[1], &s_t0Packet.stats.timeLevelStartTimes[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    memmove(&s_t0Packet.stats.timeLevelDurations[1], &s_t0Packet.stats.timeLevelDurations[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    s_t0Packet.timestamp = GlobalTimer();
    s_t0Packet.stats.timeLevelStartTimes[0] = s_t0StartTime;
    s_t0Packet.stats.timeLevelDurations[0] = s_t0Packet.timestamp - s_t0StartTime;
    
    if (!VARIANT_WriteChan0((const uint8_t*)&s_t0Packet, sizeof(s_t0Packet))) {
        s_t0Packet.stats.totalDroppedPackets += 1;
    }
    
    
    if (s_t0Packet.stats.iterationNumber % 10000 == 0) {
        xil_printf("T0 %u\r\n", s_t0Packet.stats.iterationNumber);
    }
    
    s_t0Packet.stats.iterationNumber += 1;
    s_t0Packet.stats.lastPacketSendTime = GlobalTimer() - s_t0Packet.timestamp;
}

void WORKLOAD_T1(void)
{
    if (s_t1Packet.stats.iterationNumber == 0) {
        xil_printf("T1 start\r\n");
    }
    
    uint64_t startTime = GlobalTimer();
    Do_T1_Work();
    
    VARIANT_ReadChan1(f_t1PacketBuffer, sizeof(f_t1PacketBuffer), &HandleT1Packet, NULL);
    
    memmove(&s_t1Packet.stats.timeLevelStartTimes[1], &s_t1Packet.stats.timeLevelStartTimes[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    memmove(&s_t1Packet.stats.timeLevelDurations[1], &s_t1Packet.stats.timeLevelDurations[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    s_t1Packet.timestamp = GlobalTimer();
    s_t1Packet.stats.timeLevelStartTimes[0] = startTime;
    s_t1Packet.stats.timeLevelDurations[0] = s_t1Packet.timestamp - startTime;
    
    if (!VARIANT_WriteChan1((const uint8_t*)&s_t1Packet, sizeof(s_t1Packet))) {
        s_t1Packet.stats.totalDroppedPackets += 1;
    }
    
    if (s_t1Packet.stats.iterationNumber % 10000 == 0) {
        xil_printf("T1 %u\r\n", s_t0Packet.stats.iterationNumber);
    }
    
    s_t1Packet.stats.iterationNumber += 1;
    s_t1Packet.stats.lastPacketSendTime = GlobalTimer() - s_t1Packet.timestamp;
}

void WORKLOAD_T2(void)
{
    if (s_t2Packet.stats.iterationNumber == 0) {
        xil_printf("T2 start\r\n");
    }
    
    uint64_t startTime = GlobalTimer();
    Do_T2_Work();
    
    VARIANT_ReadChan1(f_t2PacketBuffer, sizeof(f_t2PacketBuffer), &HandleT2Packet, NULL);
    
    memmove(&s_t2Packet.stats.timeLevelStartTimes[1], &s_t2Packet.stats.timeLevelStartTimes[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    memmove(&s_t2Packet.stats.timeLevelDurations[1], &s_t2Packet.stats.timeLevelDurations[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    s_t2Packet.timestamp = GlobalTimer();
    s_t2Packet.stats.timeLevelStartTimes[0] = startTime;
    s_t2Packet.stats.timeLevelDurations[0] = s_t2Packet.timestamp - startTime;
    
    if (!VARIANT_WriteChan2((const uint8_t*)&s_t2Packet, sizeof(s_t2Packet))) {
        s_t2Packet.stats.totalDroppedPackets += 1;
    }
    
    if (s_t2Packet.stats.iterationNumber % 10000 == 0) {
        xil_printf("T1 %u\r\n", s_t0Packet.stats.iterationNumber);
    }
    
    s_t2Packet.stats.iterationNumber += 1;
    s_t2Packet.stats.lastPacketSendTime = GlobalTimer() - s_t2Packet.timestamp;
}

void WORKLOAD_BG(void)
{
}

