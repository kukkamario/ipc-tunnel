#include "ipc_tunnel.h"

#include <string.h>

#include <xil_mmu.h>

#include <xil_printf.h>

typedef struct IpcTunnelControlHeader_s
{
    volatile uint32_t cpu0_write_index;
    volatile uint32_t cpu0_read_index;

    uint32_t _padding1[6];

    uint32_t cpu1_write_index;
    uint32_t cpu1_read_index;

    uint32_t _padding2[6];
} ControlHeader_t;

typedef struct PacketHeader_s {
    uint32_t packetSize;
    uint32_t reserved_;
    uint64_t data[0];
} PacketHeader_t;

static void MarkPacketAsRead(IpcTunnel_t* tunnel, uint32_t previousReadIndex);
static void SendPacket(IpcTunnel_t* tunnel, uint32_t nextWriteIndex);
static uint32_t GetNextWriteIndex(IpcTunnel_t* tunnel, uint32_t writeIndex);
static PacketHeader_t* GetWriteBufferPacket(IpcTunnel_t* tunnel, uint32_t index);
static PacketHeader_t* GetReadBufferPacket(IpcTunnel_t* tunnel, uint32_t index);

static const IpcTunnelConfig_t f_configs[] = {
    {
        .controlBlockAddress = 0xFFFF0000,
        .sendBufferAddress = 0xFFFF0040,
        .receiveBufferAddress = 0xFFFF1100,

        .sendPacketMaxSize = 0x780,
        .sendBufferedPacketCount = 2,
        .receivePacketMaxSize = 0x780,
        .receiveBufferedPacketCount = 2,

        .cpu0KickSGI = INTERRUPT_CPU0_SGI_IPC_GENERAL,

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

        .cpu0KickSGI = INTERRUPT_CPU0_SGI_IPC_PDP,

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

        .cpu0KickSGI = INTERRUPT_CPU0_SGI_IPC_WTCP,

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

        .cpu0KickSGI = INTERRUPT_CPU0_SGI_IPC_PROCESSDATALINK,

        .sharedMemoryAddress = 0xFFFFC000,
        .sharedMemorySize = 0x3000
    }
};


void IPC_TUNNEL_Init(IpcTunnel_t* tunnel, const IpcTunnelConfig_t* config)
{
    tunnel->config = config;

    tunnel->control = (ControlHeader_t*)config->controlBlockAddress;
    tunnel->receiveRingBuffer = (uint8_t*)config->sendBufferAddress;
    tunnel->sendRingBuffer = (uint8_t*)config->receiveBufferAddress;

    /* Actual size of the packet should include the header and alignment to 64 bit*/
    tunnel->sendPacketSize = ((config->sendPacketMaxSize + sizeof(PacketHeader_t)) + 7) & ~7u;
    tunnel->receivePacketSize = ((config->receivePacketMaxSize + sizeof(PacketHeader_t)) + 7) & ~7u;


    tunnel->directWriteIndex = 0;

    memset(tunnel->control, 0, sizeof(ControlHeader_t));

    uint32_t sendBufSize = config->sendBufferedPacketCount * tunnel->sendPacketSize;
    uint32_t recvBufSize = config->receiveBufferedPacketCount * tunnel->receivePacketSize;

    xil_printf("IPC_TUNNEL_Init 0x%x\r\n", config->controlBlockAddress);
    xil_printf("\tSend: buffer 0x%x  0x%x    %u packets, max size %u\r\n",
               config->sendBufferAddress,
               sendBufSize,
               (uint32_t)config->sendBufferedPacketCount,
               (uint32_t)config->sendPacketMaxSize);
    xil_printf("\tRecv: buffer 0x%x  0x%x    %u packets, max size %u\r\n",
               config->receiveBufferAddress,
               recvBufSize,
               (uint32_t)config->receiveBufferedPacketCount,
               (uint32_t)config->receivePacketMaxSize);
}

uint16_t IPC_TUNNEL_Read(IpcTunnel_t* tunnel, uint8_t* buffer, uint16_t size)
{
    uint32_t rx = 0;

    uint32_t readIndex = tunnel->control->cpu1_read_index;
    uint32_t cpu0WriteIndex = tunnel->control->cpu0_write_index;

    if (readIndex != cpu0WriteIndex) {
        PacketHeader_t* packet = GetReadBufferPacket(tunnel, readIndex);

        rx = packet->packetSize;
        if (size < rx) rx = size;

        memcpy(buffer, packet->data, rx);

        MarkPacketAsRead(tunnel, readIndex);
    }

    return rx;
}

boolean IPC_TUNNEL_Write(IpcTunnel_t* tunnel, const uint8_t* buffer, uint16_t size)
{
    if (size == 0)
    {
        return FALSE;
    }

    if (size > tunnel->config->sendPacketMaxSize)
    {
        /* Packet doesn't fit in the ring buffer */
        return FALSE;
    }

    uint32_t writeIndex = tunnel->control->cpu1_write_index;
    uint32_t cpu0ReadIndex = tunnel->control->cpu0_read_index;
    uint32_t nextWriteIndex = GetNextWriteIndex(tunnel, writeIndex);

    if (nextWriteIndex != cpu0ReadIndex)
    {
        PacketHeader_t* packet = GetWriteBufferPacket(tunnel, writeIndex);
        packet->packetSize = size;
        memcpy(packet->data, buffer, size);

        SendPacket(tunnel, nextWriteIndex);
        return TRUE;
    }

    //xil_printf("IPC_TUNNEL_Write: tunnel %p full, write size %i\r\n", (void*)tunnel->control, size);


    return FALSE;
}

uint8_t* IPC_TUNNEL_BeginDirectWrite(IpcTunnel_t* tunnel, uint16_t size)
{
    if (size == 0) {
        return 0;
    }

    if (size > tunnel->config->sendPacketMaxSize) {
        /* Packet doesn't fit in the ring buffer */
        return 0;
    }

    uint32_t writeIndex = tunnel->control->cpu1_write_index;
    uint32_t nextWriteIndex = GetNextWriteIndex(tunnel, writeIndex);
    uint32_t cpu0ReadIndex = tunnel->control->cpu0_read_index;

    if (nextWriteIndex != cpu0ReadIndex) {
        PacketHeader_t* packet = GetWriteBufferPacket(tunnel, writeIndex);
        packet->packetSize = size;

        tunnel->directWriteIndex = nextWriteIndex;

        return (uint8_t*)packet->data;
    }


    //xil_printf("IPC_TUNNEL_BeginDirectWrite: tunnel %p full, write size %i\r\n", (void*)tunnel->control, size);
    return 0;
}

void IPC_TUNNEL_EndDirectWrite(IpcTunnel_t* tunnel)
{
    SendPacket(tunnel, tunnel->directWriteIndex);
}

uint16_t IPC_TUNNEL_BeginDirectRead(IpcTunnel_t* tunnel, const uint8_t** dataPtrOut)
{
    uint32_t rx = 0;

    uint32_t readIndex = tunnel->control->cpu1_read_index;
    uint32_t cpu0WriteIndex = tunnel->control->cpu0_write_index;

    if (readIndex != cpu0WriteIndex) {
        PacketHeader_t* packet = GetReadBufferPacket(tunnel, readIndex);

        *dataPtrOut = (uint8_t*)packet->data;
        rx = packet->packetSize;

        tunnel->directReadIndex = readIndex;
    }

    return rx;
}

void IPC_TUNNEL_EndDirectRead(IpcTunnel_t* tunnel)
{
    MarkPacketAsRead(tunnel, tunnel->directReadIndex);
}

uint8_t* IPC_TUNNEL_GetSharedMemoryPointer(IpcTunnel_t* tunnel)
{
    return (uint8_t*)tunnel->config->sharedMemoryAddress;
}

uint32_t IPC_TUNNEL_GetSharedMemorySize(IpcTunnel_t* tunnel)
{
    return tunnel->config->sharedMemorySize;
}

const IpcTunnelConfig_t* IPC_TUNNEL_GetConfig(IpcTunnelConfigInstance_t tunnelIndex)
{
    return &f_configs[tunnelIndex];
}



static void MarkPacketAsRead(IpcTunnel_t* tunnel, uint32_t previousReadIndex)
{
    /* Data memory barrier to make sure that reading data has completed before
     * we update cpu1_read_offset and allow other core to write over it
     */
    dmb();

    if (++previousReadIndex == tunnel->config->receiveBufferedPacketCount) {
        previousReadIndex = 0;
    }

    tunnel->control->cpu1_read_index = previousReadIndex;
}

static void SendPacket(IpcTunnel_t* tunnel, uint32_t nextWriteIndex)
{
    /* Data memory barrier to make sure that memcpy has completed before we
     * update cpu1_write_offset and allow other core to read the packet.
     *
     * Other core can read this even before SGI is triggered when in it is
     * in non-blocking mode
     */
    dmb();
    tunnel->control->cpu1_write_index = nextWriteIndex;

    /* Data synchronization barrier to make sure index is always updated
     * before SGI is triggered.
     */
    dsb();
    INTERRUPT_TriggerCPU0SGI(tunnel->config->cpu0KickSGI);
}

static uint32_t GetNextWriteIndex(IpcTunnel_t* tunnel, uint32_t writeIndex)
{
    if (++writeIndex == tunnel->config->sendBufferedPacketCount) {
        writeIndex = 0;
    }

    return writeIndex;
}

static PacketHeader_t* GetWriteBufferPacket(IpcTunnel_t* tunnel, uint32_t index)
{
    return (PacketHeader_t*)(tunnel->sendRingBuffer
                             + index * tunnel->sendPacketSize);
}

static PacketHeader_t* GetReadBufferPacket(IpcTunnel_t* tunnel, uint32_t index)
{
    return (PacketHeader_t*)(tunnel->receiveRingBuffer
                             + index * tunnel->receivePacketSize);
}
