#include "application.h"
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

static volatile bool f_running = true;


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
    
    if (packet->flags == 0xDEAD) f_running = false;
    
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

bool APPLICATION_Init(void)
{
    xil_printf("WORKLOAD_Init\r\n");
    memset(&s_t0Packet, 0, sizeof(s_t0Packet));
    memset(&s_t1Packet, 0, sizeof(s_t1Packet));
    memset(&s_t2Packet, 0, sizeof(s_t2Packet));
    return true;
}


static bool f_t0InitDone = false;

void APPLICATION_T0(void)
{
    s_t0StartTime = GlobalTimer();
    
    if (f_t0InitDone) {
        VARIANT_ReadChan0(f_t0PacketBuffer, sizeof(f_t0PacketBuffer), &HandleT0Packet, NULL);
    }
    
    WORKLOAD_T0();
    
    // Skip actual work during the initial run because its timing may not be as precise
    if (f_t0InitDone) {
        
        memmove(&s_t0Packet.stats.timeLevelStartTimes[1], &s_t0Packet.stats.timeLevelStartTimes[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
        memmove(&s_t0Packet.stats.timeLevelDurations[1], &s_t0Packet.stats.timeLevelDurations[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
        s_t0Packet.timestamp = GlobalTimer();
        s_t0Packet.stats.timeLevelStartTimes[0] = s_t0StartTime;
        s_t0Packet.stats.timeLevelDurations[0] = s_t0Packet.timestamp - s_t0StartTime;
        s_t0Packet.variables = s_variables;
        
        if (!VARIANT_WriteChan0((const uint8_t*)&s_t0Packet, sizeof(s_t0Packet))) {
            s_t0Packet.stats.totalDroppedPackets += 1;
        }
    
        s_t0Packet.stats.iterationNumber += 1;
        s_t0Packet.stats.lastPacketSendTime = GlobalTimer() - s_t0Packet.timestamp;
    }
    else {
        f_t0InitDone = true;
    }
}

void APPLICATION_T1(void)
{
    uint64_t startTime = GlobalTimer();
    WORKLOAD_T1();
    
    VARIANT_ReadChan1(f_t1PacketBuffer, sizeof(f_t1PacketBuffer), &HandleT1Packet, NULL);
    
    memmove(&s_t1Packet.stats.timeLevelStartTimes[1], &s_t1Packet.stats.timeLevelStartTimes[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    memmove(&s_t1Packet.stats.timeLevelDurations[1], &s_t1Packet.stats.timeLevelDurations[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    s_t1Packet.timestamp = GlobalTimer();
    s_t1Packet.stats.timeLevelStartTimes[0] = startTime;
    s_t1Packet.stats.timeLevelDurations[0] = s_t1Packet.timestamp - startTime;
    
    if (!VARIANT_WriteChan1((const uint8_t*)&s_t1Packet, sizeof(s_t1Packet))) {
        s_t1Packet.stats.totalDroppedPackets += 1;
    }
    
    s_t1Packet.stats.iterationNumber += 1;
    s_t1Packet.stats.lastPacketSendTime = GlobalTimer() - s_t1Packet.timestamp;
}

void APPLICATION_T2(void)
{
    uint64_t startTime = GlobalTimer();
    WORKLOAD_T2();
    
    VARIANT_ReadChan2(f_t2PacketBuffer, sizeof(f_t2PacketBuffer), &HandleT2Packet, NULL);
    
    memmove(&s_t2Packet.stats.timeLevelStartTimes[1], &s_t2Packet.stats.timeLevelStartTimes[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    memmove(&s_t2Packet.stats.timeLevelDurations[1], &s_t2Packet.stats.timeLevelDurations[0], sizeof(uint32_t) * (SHAREDSTATE_BACKLOG - 1));
    s_t2Packet.timestamp = GlobalTimer();
    s_t2Packet.stats.timeLevelStartTimes[0] = startTime;
    s_t2Packet.stats.timeLevelDurations[0] = s_t2Packet.timestamp - startTime;
    
    if (!VARIANT_WriteChan2((const uint8_t*)&s_t2Packet, sizeof(s_t2Packet))) {
        s_t2Packet.stats.totalDroppedPackets += 1;
    }
    
    s_t2Packet.stats.iterationNumber += 1;
    s_t2Packet.stats.lastPacketSendTime = GlobalTimer() - s_t2Packet.timestamp;
}

void APPLICATION_BG(void)
{
	WORKLOAD_BG();
}


bool APPLICATION_Running()
{
    return f_running;
}
