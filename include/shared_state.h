#ifndef DIPPA_SHARED_STATE_H_
#define DIPPA_SHARED_STATE_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHAREDSTATE_PACKET_LATENCY_BUF_SIZE 8
#define SHAREDSTATE_BACKLOG 16

#define SHAREDSTATE_VARIABLE_COUNT 16

typedef struct {
	uint64_t lastSetReceiveTimestamp;
	uint64_t lastSetSendTimestamp;
	uint16_t lastSetPacketId;
	uint16_t flags;
	uint16_t padding1;
	uint16_t padding2;
	
	double vd[SHAREDSTATE_VARIABLE_COUNT];
	float vf[SHAREDSTATE_VARIABLE_COUNT];
	uint16_t vs[SHAREDSTATE_VARIABLE_COUNT];
	uint8_t vb[SHAREDSTATE_VARIABLE_COUNT];
} SharedState_Variables;

typedef struct {
	uint16_t commandPacketId;
	uint32_t commandPacketLatency;
	uint32_t totalDroppedPackets;
	uint32_t timeLevelStartTimes[SHAREDSTATE_BACKLOG];
	uint32_t timeLevelDurations[SHAREDSTATE_BACKLOG];
	uint32_t iterationNumber;
	uint32_t lastPacketSendTime;
} SharedState_TimeLevelStats;

typedef struct {
	uint64_t timestamp;
	SharedState_Variables variables;
	SharedState_TimeLevelStats stats;
} SharedState_T0DataPacket;

typedef struct {
	uint64_t timestamp;
	SharedState_TimeLevelStats stats;
} SharedState_T0ShmDataPacket;

enum SharedState_VariableType {
	SHAREDSTATE_VAR_DOUBLE = 0,
	SHAREDSTATE_VAR_FLOAT = 1,
	SHAREDSTATE_VAR_SHORT = 2,
	SHAREDSTATE_VAR_BYTE = 3
};

typedef union {
	double vd;
	float vf;
	uint16_t vs;
	uint8_t vb;
} SharedState_Value;

typedef struct {
	uint8_t variableId;
	uint8_t variableType;
} SharedState_VariableSet;

typedef struct {
	uint64_t timestamp;
	uint16_t packetId;
	uint16_t flags;
	uint16_t varSetCommands;
	
	// Followed by multiple SharedState_VariableSet packets and values
} SharedState_T0CommandPacket;

typedef struct {
	uint64_t timestamp;
	SharedState_TimeLevelStats stats;
} SharedState_T1DataPacket;

typedef struct {
	volatile uint32_t updateAtomicCounter;
	uint32_t prevUpdateTime;
	uint64_t timestamp;
	SharedState_Variables vars;
} SharedState_T0SharedMemory;


typedef struct {
	uint64_t timestamp;
	uint16_t packetId;
	uint16_t flags;
} SharedState_T1CommandPacket;

typedef struct {
	uint64_t timestamp;
	SharedState_TimeLevelStats stats;
} SharedState_T2DataPacket;

typedef struct {
	uint64_t timestamp;
	uint16_t packetId;
	uint16_t flags;
} SharedState_T2CommandPacket;


#ifdef __cplusplus
}
#endif

#endif  // DIPPA_SHARED_STATE_H_
