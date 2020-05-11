#include "variant.h"
#include <openamp/rpmsg.h>
#include "platform_info.h"


#define SERVICE_NAME0 "dippa-channel0"
#define SERVICE_NAME1 "dippa-channel1"
#define SERVICE_NAME2 "dippa-channel2"

static struct rpmsg_device *rpdev = 0;
static void* f_platform = NULL;

#define RPMSG_MAX_DATA_SIZE 496
#define PACKET_BUFFER_SIZE 4

typedef struct {
    uint32_t packetSize;
    uint8_t buf[RPMSG_MAX_DATA_SIZE];
} BufferedPacket;

typedef struct {
    int id;
    struct rpmsg_endpoint lept;
    int packetsBegin;
    int packetsEnd;
    BufferedPacket packets[PACKET_BUFFER_SIZE];
} Channel;

Channel f_chans[3] = {
    {
        .id = 0,
        .packetsBegin = 0,
        .packetsEnd = 0
    },
    {
        .id = 1,
        .packetsBegin = 0,
        .packetsEnd = 0
    },
    {
        .id = 2,
        .packetsBegin = 0,
        .packetsEnd = 0
    }
};

static int f_channelBlocked = -1;
static uint8_t* f_channelBlockedBuf = 0;
static uint32_t f_channelBlockedSize = 0;
static uint32_t f_packetsBuffered = 0;

static int rpmsg_endpoint_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
                              uint32_t src, void *priv)
{
    Channel* chan = (Channel*)ept->priv;
    
    if (f_channelBlocked == chan->id) {
        memcpy(f_channelBlockedBuf, data, len);
        f_channelBlockedSize = len;
    }
    else {
        if (chan->packetsEnd != PACKET_BUFFER_SIZE) {
            BufferedPacket* packet = &chan->packets[chan->packetsEnd];
            packet->packetSize = len;
            memcpy(packet->buf, data, len);
            
            chan->packetsEnd += 1;
            
            if (chan->packetsEnd == PACKET_BUFFER_SIZE) {
                chan->packetsEnd = 0;
            }
            
            /* mark ring buffer as full */
            if (chan->packetsBegin == chan->packetsEnd) {
                chan->packetsEnd = PACKET_BUFFER_SIZE;
            }
            
        }
        else {
            xil_printf("Dropped packet channel %i\r\n", chan->id);
        }
        ++f_packetsBuffered;
    }
    return RPMSG_SUCCESS;
}

static void rpmsg_service_unbind(struct rpmsg_endpoint *ept)
{
}

bool VARIANT_Initialize(void* platform)
{
    f_platform = platform; 
    rpdev = platform_create_rpmsg_vdev(platform, 0,
                       VIRTIO_DEV_SLAVE,
                       NULL, NULL);
    
    int ret;
    
    ret = rpmsg_create_ept(&f_chans[0].lept, rpdev, SERVICE_NAME0,
                   0, RPMSG_ADDR_ANY, rpmsg_endpoint_cb,
                   rpmsg_service_unbind);
    
    f_chans[0].lept.priv = &f_chans[0];
    if (ret) {
        xil_printf("Failed to create endpoint0.\r\n");
        return false;
    }
    
    ret = rpmsg_create_ept(&f_chans[1].lept, rpdev, SERVICE_NAME1,
                   0, RPMSG_ADDR_ANY, rpmsg_endpoint_cb,
                   rpmsg_service_unbind);
    f_chans[1].lept.priv = &f_chans[1];
    if (ret) {
        xil_printf("Failed to create endpoint1.\r\n");
        return false;
    }
    
    ret = rpmsg_create_ept(&f_chans[2].lept, rpdev, SERVICE_NAME2,
                   0, RPMSG_ADDR_ANY, rpmsg_endpoint_cb,
                   rpmsg_service_unbind);
    
    f_chans[2].lept.priv = &f_chans[2];
    if (ret) {
        xil_printf("Failed to create endpoint2.\r\n");
        return false;
    }
    
    return TRUE;
}

static uint32_t ReadChannel(int channelId, uint8_t* buffer, uint32_t size) {
    Channel* chan = &f_chans[channelId];
    
    if (chan->packetsBegin != chan->packetsEnd) {
        BufferedPacket* packet = &chan->packets[chan->packetsBegin];
        memcpy(buffer, packet->buf, packet->packetSize);
        
        ++chan->packetsBegin;
        if (chan->packetsBegin == PACKET_BUFFER_SIZE) {
            chan->packetsBegin = 0;
        }
        if (chan->packetsEnd == PACKET_BUFFER_SIZE) {
            chan->packetsEnd = chan->packetsBegin - 1;
            if (chan->packetsEnd < 0) {
                chan->packetsEnd = PACKET_BUFFER_SIZE - 1;
            }
        }
        
        return packet->packetSize;
    }
    
    f_channelBlocked = channelId;
    f_channelBlockedBuf = buffer;
    f_channelBlockedSize = 0;
    
    f_packetsBuffered = 0;
    
    // Just expect that size is enough...
    while (platform_poll_noblock(f_platform) && f_channelBlockedSize == 0 && f_packetsBuffered < 3) {}
    return f_channelBlockedSize;
}


uint32_t VARIANT_ReadChan0(uint8_t* buffer, uint32_t size)
{
    return ReadChannel(0, buffer, size);
}

uint32_t VARIANT_ReadChan1(uint8_t* buffer, uint32_t size)
{
    return ReadChannel(1, buffer, size);
}

uint32_t VARIANT_ReadChan2(uint8_t* buffer, uint32_t size)
{
    return ReadChannel(2, buffer, size);
}

bool VARIANT_WriteChan0(const uint8_t* buffer, uint32_t size)
{
    return rpmsg_send(&f_chans[0].lept, buffer, size) == RPMSG_SUCCESS;
}
bool VARIANT_WriteChan1(const uint8_t* buffer, uint32_t size)
{
    return rpmsg_send(&f_chans[1].lept, buffer, size) == RPMSG_SUCCESS;
}
bool VARIANT_WriteChan2(const uint8_t* buffer, uint32_t size)
{
    return rpmsg_send(&f_chans[2].lept, buffer, size) == RPMSG_SUCCESS;
}

uint32_t VARIANT_PacketSizeChan0(void)
{
    return RPMSG_MAX_DATA_SIZE;
}
uint32_t VARIANT_PacketSizeChan1(void)
{
    return RPMSG_MAX_DATA_SIZE;
}
uint32_t VARIANT_PacketSizeChan2(void)
{
    return RPMSG_MAX_DATA_SIZE;
}

void VARIANT_Destruct(void)
{
    rpmsg_destroy_ept(&f_chans[2].lept);
    rpmsg_destroy_ept(&f_chans[1].lept);
    rpmsg_destroy_ept(&f_chans[0].lept);
    
    platform_release_rpmsg_vdev(rpdev);
}
