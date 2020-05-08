#ifndef IPC_TUNNEL_H_
#define IPC_TUNNEL_H_
#include "types/deftypes.h"

#include "interrupt_internal.h"

typedef enum {
    IPC_TUNNEL_CONFIG_GENERAL = 0,
    IPC_TUNNEL_CONFIG_PDP = 1,
    IPC_TUNNEL_CONFIG_WTCP = 2,
    IPC_TUNNEL_CONFIG_PROCESSDATALINK = 3
} IpcTunnelConfigInstance_t;


typedef boolean (*IpcTunnelReadInterruptHandler_t)(
        const uint8_t* buffer,
        uint16_t size);

typedef struct IpcTunnelConfig_s {
    uintptr_t controlBlockAddress;
    uintptr_t sendBufferAddress;
    uintptr_t receiveBufferAddress;

    uint16_t sendPacketMaxSize;
    uint16_t sendBufferedPacketCount;
    uint16_t receivePacketMaxSize;
    uint16_t receiveBufferedPacketCount;

    InterruptCpu0SGI_t cpu0KickSGI;

    uintptr_t sharedMemoryAddress;
    uintptr_t sharedMemorySize;
} IpcTunnelConfig_t;

typedef struct IpcTunnel_s {
    const IpcTunnelConfig_t* config;

    struct IpcTunnelControlHeader_s* control;
    uint8_t* sendRingBuffer;
    uint8_t* receiveRingBuffer;
    uint16_t sendPacketSize;
    uint16_t receivePacketSize;

    uint32_t directWriteIndex;
    uint32_t directReadIndex;
} IpcTunnel_t;

void IPC_TUNNEL_Init(
        IpcTunnel_t* tunnel,
        const IpcTunnelConfig_t* config);

uint16_t IPC_TUNNEL_Read(IpcTunnel_t* tunnel, uint8_t *buffer, uint16_t size);

boolean IPC_TUNNEL_Write(IpcTunnel_t* tunnel, const uint8_t* buffer, uint16_t size);

uint8_t* IPC_TUNNEL_BeginDirectWrite(IpcTunnel_t* tunnel, uint16_t size);

void IPC_TUNNEL_EndDirectWrite(IpcTunnel_t* tunnel);

uint16_t IPC_TUNNEL_BeginDirectRead(IpcTunnel_t* tunnel, const uint8_t** dataPtrOut);

void IPC_TUNNEL_EndDirectRead(IpcTunnel_t* tunnel);

uint8_t* IPC_TUNNEL_GetSharedMemoryPointer(IpcTunnel_t* tunnel);
uint32_t IPC_TUNNEL_GetSharedMemorySize(IpcTunnel_t* tunnel);

const IpcTunnelConfig_t* IPC_TUNNEL_GetConfig(IpcTunnelConfigInstance_t tunnelIndex);


#endif  // IPC_TUNNEL_H_
