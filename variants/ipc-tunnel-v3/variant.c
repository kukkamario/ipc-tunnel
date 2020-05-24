#include "variant.h"
#include "ipc_tunnel.h"
#include <xil_mmu.h>

static const IpcTunnelConfig_t f_configs[] = {
    {
        .controlBlockAddress = 0xFFFF0000,
        .sendBufferAddress = 0xFFFF0040,
        .receiveBufferAddress = 0xFFFF1100,

        .sendPacketMaxSize = 0x780,
        .sendBufferedPacketCount = 2,
        .receivePacketMaxSize = 0x780,
        .receiveBufferedPacketCount = 2,

        .cpu0KickSGI = 14,

        .sharedMemoryAddress = 0xFFFF2200,
        .sharedMemorySize = 0xE00
    },
    {
        .controlBlockAddress = 0xFFFF3000,
        .sendBufferAddress = 0xFFFF3040,
        .receiveBufferAddress = 0xFFFF5100,

        .sendPacketMaxSize = 0x780,
        .sendBufferedPacketCount = 4,
        .receivePacketMaxSize = 0x780,
        .receiveBufferedPacketCount = 4,

        .cpu0KickSGI = 13,

        .sharedMemoryAddress = 0,
        .sharedMemorySize = 0
    },
    {
        .controlBlockAddress = 0xFFFF7200,
        .sendBufferAddress = 0xFFFF7240,
        .receiveBufferAddress = 0xFFFF9800,

        .sendPacketMaxSize = 0x1200,
        .sendBufferedPacketCount = 2,
        .receivePacketMaxSize = 0x1200,
        .receiveBufferedPacketCount = 2,

        .cpu0KickSGI = 12,

        .sharedMemoryAddress = 0xFFFFC000,
        .sharedMemorySize = 0x3000
    },

    {
        .controlBlockAddress = 0x3FFF0000,
        .sendBufferAddress = 0x3FFF0040,
        .receiveBufferAddress = 0x3FFF1100,

        .sendPacketMaxSize = 0x780,
        .sendBufferedPacketCount = 2,
        .receivePacketMaxSize = 0x780,
        .receiveBufferedPacketCount = 2,

        .cpu0KickSGI = 11,

        .sharedMemoryAddress = 0x3FFF2200,
        .sharedMemorySize = 0xE00
    },
    {
        .controlBlockAddress = 0x3FFF3000,
        .sendBufferAddress = 0x3FFF3040,
        .receiveBufferAddress = 0x3FFF5100,

        .sendPacketMaxSize = 0x780,
        .sendBufferedPacketCount = 4,
        .receivePacketMaxSize = 0x780,
        .receiveBufferedPacketCount = 4,

        .cpu0KickSGI = 10,

        .sharedMemoryAddress = 0,
        .sharedMemorySize = 0
    },
    {
        .controlBlockAddress = 0x3FFF7200,
        .sendBufferAddress = 0x3FFF7240,
        .receiveBufferAddress = 0x3FFF9800,

        .sendPacketMaxSize = 0x1200,
        .sendBufferedPacketCount = 2,
        .receivePacketMaxSize = 0x1200,
        .receiveBufferedPacketCount = 2,

        .cpu0KickSGI = 9,

        .sharedMemoryAddress = 0x3FFFC000,
        .sharedMemorySize = 0x3000
    }
};

static IpcTunnel_t f_tunnels[3];

#ifdef IPC_TUNNEL_OCM
#define IPC_TUNNEL_CONFIG_OFFSET 0
#else
#define IPC_TUNNEL_CONFIG_OFFSET 3
#endif

bool VARIANT_Initialize(void* platform)
{
#ifdef IPC_TUNNEL_CACHED
    Xil_SetTlbAttributes( 0xFFFF0000, NORM_WB_CACHE );
    Xil_SetTlbAttributes( 0x3FFF0000, NORM_WB_CACHE );
#else
    Xil_SetTlbAttributes( 0xFFFF0000, 0x04de2 ); // S=b0 TEX=b100 AP=b11, Domain=b1111, C=b0, B=b0
    Xil_SetTlbAttributes( 0x3FFF0000, 0x04de2 ); // S=b0 TEX=b100 AP=b11, Domain=b1111, C=b0, B=b0
#endif
    (void)platform;
    
    IPC_TUNNEL_Init(&f_tunnels[0], &f_configs[IPC_TUNNEL_CONFIG_OFFSET]);
    IPC_TUNNEL_Init(&f_tunnels[1], &f_configs[IPC_TUNNEL_CONFIG_OFFSET + 1]);
    IPC_TUNNEL_Init(&f_tunnels[2], &f_configs[IPC_TUNNEL_CONFIG_OFFSET + 2]);
    
    return true;
}

void VARIANT_ReadChan0(uint8_t* buffer, uint32_t size, VARIANT_ReadCallback cb, void* user)
{
    uint16_t s = IPC_TUNNEL_Read(&f_tunnels[0], buffer, size);
    if (s > 0) {
        cb(buffer, s, user);
    }
}

void VARIANT_ReadChan1(uint8_t* buffer, uint32_t size, VARIANT_ReadCallback cb, void* user)
{
    uint16_t s = IPC_TUNNEL_Read(&f_tunnels[1], buffer, size);
    if (s > 0) {
        cb(buffer, s, user);
    }
}

void VARIANT_ReadChan2(uint8_t* buffer, uint32_t size, VARIANT_ReadCallback cb, void* user)
{
    uint16_t s = IPC_TUNNEL_Read(&f_tunnels[2], buffer, size);
    if (s > 0) {
        cb(buffer, s, user);
    }
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
