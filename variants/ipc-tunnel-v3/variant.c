#include "variant.h"
#include "ipc_tunnel.h"

static const IpcTunnelConfig_t f_configs[] = {
    {
        .controlBlockAddress = 0xFFFF0000,
        .sendBufferAddress = 0xFFFF0040,
        .receiveBufferAddress = 0xFFFF1100,

        .sendPacketMaxSize = 0x780,
        .sendBufferedPacketCount = 2,
        .receivePacketMaxSize = 0x780,
        .receiveBufferedPacketCount = 2,

        .cpu0KickSGI = 1,

        .sharedMemoryAddress = 0,
        .sharedMemorySize = 0
    },
    {
        .controlBlockAddress = 0xFFFF2200,
        .sendBufferAddress = 0xFFFF2240,
        .receiveBufferAddress = 0xFFFF2800,

        .sendPacketMaxSize = 0x120,
        .sendBufferedPacketCount = 4,
        .receivePacketMaxSize = 0x120,
        .receiveBufferedPacketCount = 4,

        .cpu0KickSGI = 2,

        .sharedMemoryAddress = 0,
        .sharedMemorySize = 0
    },
    {
        .controlBlockAddress = 0xFFFF3000,
        .sendBufferAddress = 0xFFFF3040,
        .receiveBufferAddress = 0xFFFF5100,

        .sendPacketMaxSize = 0x780,
        .sendBufferedPacketCount = 4,
        .receivePacketMaxSize = 0x780,
        .receiveBufferedPacketCount = 4,

        .cpu0KickSGI = 3,

        .sharedMemoryAddress = 0,
        .sharedMemorySize = 0
    }
};

static IpcTunnel_t f_tunnels[3];

bool VARIANT_Initialize(void* platform)
{
    (void)platform;
    
    IPC_TUNNEL_Init(&f_tunnels[0], &f_configs[0]);
    IPC_TUNNEL_Init(&f_tunnels[1], &f_configs[1]);
    IPC_TUNNEL_Init(&f_tunnels[2], &f_configs[2]);
    
    return true;
}

uint32_t VARIANT_ReadChan0(uint8_t* buffer, uint32_t size)
{
    return IPC_TUNNEL_Read(&f_tunnels[0], buffer, size);
}

uint32_t VARIANT_ReadChan1(uint8_t* buffer, uint32_t size)
{
    return IPC_TUNNEL_Read(&f_tunnels[1], buffer, size);
}

uint32_t VARIANT_ReadChan2(uint8_t* buffer, uint32_t size)
{
    return IPC_TUNNEL_Read(&f_tunnels[2], buffer, size);
}

bool VARIANT_WriteChan0(const uint8_t* buffer, uint32_t size)
{
    return IPC_TUNNEL_Write(&f_tunnels[0], buffer, size);
}
bool VARIANT_WriteChan1(const uint8_t* buffer, uint32_t size)
{
    return IPC_TUNNEL_Write(&f_tunnels[1], buffer, size);
}


bool VARIANT_WriteChan2(const uint8_t* buffer, uint32_t size)
{
    return IPC_TUNNEL_Write(&f_tunnels[2], buffer, size);
}


uint32_t VARIANT_PacketSizeChan0(void)
{
    return f_configs[0].sendPacketMaxSize;
}

uint32_t VARIANT_PacketSizeChan1(void)
{
    return f_configs[1].sendPacketMaxSize;
}

uint32_t VARIANT_PacketSizeChan2(void)
{
    return f_configs[2].sendPacketMaxSize;
}

void VARIANT_Destruct(void) {
    
}
