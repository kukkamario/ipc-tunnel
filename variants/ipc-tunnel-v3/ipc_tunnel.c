#include "ipc_tunnel.h"

#include <string.h>

#include <xil_mmu.h>
#include <xil_printf.h>
#include <xscugic.h>

#if IPC_TUNNEL_CACHED
    #define USE_ATOMIC
#endif

#ifdef USE_ATOMIC
#include <stdatomic.h>
#define ATOMIC_UINT32 _Atomic( uint32_t)
#define ATOMIC_READ(ptr) atomic_load_explicit((ptr), memory_order_acquire)
#define ATOMIC_WRITE(ptr, val) atomic_store_explicit((ptr), (val), memory_order_release)

#define MEMORY_BARRIER()
#define PACKET_SIZE_ALIGNMENT 32u

#else
#define ATOMIC_UINT32 uint32_t
#define ATOMIC_READ(ptr) (*ptr)
#define ATOMIC_WRITE(ptr, val) (*(ptr) = (val))

#define MEMORY_BARRIER() dsb()
#define PACKET_SIZE_ALIGNMENT 8u
#endif

typedef struct IpcTunnelControlHeader_s
{
    volatile ATOMIC_UINT32 cpu0_write_index;
    volatile ATOMIC_UINT32 cpu0_read_index;

    uint32_t _padding1[6];

    volatile ATOMIC_UINT32 cpu1_write_index;
    volatile ATOMIC_UINT32 cpu1_read_index;

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

void IPC_TUNNEL_Init(IpcTunnel_t* tunnel, const IpcTunnelConfig_t* config)
{
    tunnel->config = config;

    tunnel->control = (ControlHeader_t*)config->controlBlockAddress;
    tunnel->receiveRingBuffer = (uint8_t*)config->sendBufferAddress;
    tunnel->sendRingBuffer = (uint8_t*)config->receiveBufferAddress;

    /* Actual size of the packet should include the header and alignment to 32 bytes (cacheline) */
    tunnel->sendPacketSize = ((config->sendPacketMaxSize + sizeof(PacketHeader_t)) + (PACKET_SIZE_ALIGNMENT - 1u)) & ~(PACKET_SIZE_ALIGNMENT - 1u);
    tunnel->receivePacketSize = ((config->receivePacketMaxSize + sizeof(PacketHeader_t)) + (PACKET_SIZE_ALIGNMENT - 1u)) & ~(PACKET_SIZE_ALIGNMENT - 1u);

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

    uint32_t readIndex = ATOMIC_READ(&tunnel->control->cpu1_read_index);
    uint32_t cpu0WriteIndex = ATOMIC_READ(&tunnel->control->cpu0_write_index);

    if (readIndex != cpu0WriteIndex) {
        PacketHeader_t* packet = GetReadBufferPacket(tunnel, readIndex);

        rx = packet->packetSize;
        if (size < rx) rx = size;

        memcpy(buffer, packet->data, rx);

        MarkPacketAsRead(tunnel, readIndex);
    }

    return rx;
}

bool IPC_TUNNEL_Write(IpcTunnel_t* tunnel, const uint8_t* buffer, uint16_t size)
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

    uint32_t writeIndex = ATOMIC_READ(&tunnel->control->cpu1_write_index);
    uint32_t cpu0ReadIndex = ATOMIC_READ(&tunnel->control->cpu0_read_index);
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

    uint32_t writeIndex = ATOMIC_READ(&tunnel->control->cpu1_write_index);
    uint32_t nextWriteIndex = GetNextWriteIndex(tunnel, writeIndex);
    uint32_t cpu0ReadIndex = ATOMIC_READ(&tunnel->control->cpu0_read_index);

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

    uint32_t readIndex = ATOMIC_READ(&tunnel->control->cpu1_read_index);
    uint32_t cpu0WriteIndex = ATOMIC_READ(&tunnel->control->cpu0_write_index);

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

static void MarkPacketAsRead(IpcTunnel_t* tunnel, uint32_t previousReadIndex)
{
    if (++previousReadIndex == tunnel->config->receiveBufferedPacketCount) {
        previousReadIndex = 0;
    }
    MEMORY_BARRIER();
    ATOMIC_WRITE(&tunnel->control->cpu1_read_index, previousReadIndex);
}

static void SendPacket(IpcTunnel_t* tunnel, uint32_t nextWriteIndex)
{
    MEMORY_BARRIER();
    ATOMIC_WRITE(&tunnel->control->cpu1_write_index, nextWriteIndex);
    
    MEMORY_BARRIER();
    /* Trigger software interrupt on the other CPU */
    uint32_t mask = ((1 << 16U) | tunnel->config->cpu0KickSGI) & (XSCUGIC_SFI_TRIG_CPU_MASK | XSCUGIC_SFI_TRIG_INTID_MASK);
    *(volatile uint32_t*)(XPAR_PS7_SCUGIC_0_DIST_BASEADDR + XSCUGIC_SFI_TRIG_OFFSET) = mask;
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
