
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openamp/open_amp.h>
#include "platform_info.h"
#include <xil_printf.h>
#include <xtime_l.h>
#include <sleep.h>
#include "packets.h"

#define LPRINTF(format, ...) xil_printf(format, ##__VA_ARGS__)
//#define LPRINTF(format, ...)
#define LPERROR(format, ...) LPRINTF("ERROR: " format, ##__VA_ARGS__)

/* Local variables */
static struct rpmsg_endpoint lept;
static volatile int shutdown_req = 0;

BaremetalToLinux reply;


/*-----------------------------------------------------------------------------*
 *  RPMSG callbacks setup by remoteproc_resource_init()
 *-----------------------------------------------------------------------------*/
static int rpmsg_endpoint_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
                 uint32_t src, void *priv)
{
    if (len == sizeof(LinuxToBaremetal)) {
        uint64_t timestamp;
        XTime_GetTime(&timestamp);

        LinuxToBaremetal recv_data;
        memcpy(&recv_data, data, sizeof(LinuxToBaremetal));

        reply.linux_to_baremetal_latency = timestamp - recv_data.send_timestamp;

        if (recv_data.control_flags & CONTROL_FLAG_SHUTDOWN) {
            LPERROR("Shutdown request\r\n");
            shutdown_req = 1;
        }
        else {
            /* Wait a moment to allow linux app to invoke read to get the best possible latency */
            usleep(5000);

            XTime_GetTime(&reply.send_timestamp);
            if (rpmsg_send(ept, &reply, sizeof(reply)) < 0) {
                xil_printf("rpmsg_send failed\r\n");
            }
        }
    }

    return RPMSG_SUCCESS;
}

static void rpmsg_service_unbind(struct rpmsg_endpoint *ept)
{
    (void)ept;
    LPERROR("Endpoint is destroyed\r\n");
    shutdown_req = 1;
}

/*-----------------------------------------------------------------------------*
 *  Application
 *-----------------------------------------------------------------------------*/
int app(struct rpmsg_device *rdev, void *priv)
{
    int ret;

    ret = rpmsg_create_ept(&lept, rdev, "rpmsg-openamp-demo-channel",
                   0, RPMSG_ADDR_ANY, rpmsg_endpoint_cb,
                   rpmsg_service_unbind);
    if (ret) {
        LPERROR("Failed to create endpoint. %i\r\n", ret);
        return -1;
    }

    LPRINTF("Waiting for events...\r\n");

    struct remoteproc *rproc = priv;
    struct remoteproc_priv *prproc;
    prproc = rproc->priv;
    atomic_flag_test_and_set(&prproc->nokick);

    while(1) {
        platform_poll(priv);
        /* we got a shutdown request, exit */
        if (shutdown_req) {
            break;
        }
    }
    rpmsg_destroy_ept(&lept);

    return 0;
}

/*-----------------------------------------------------------------------------*
 *  Application entry point
 *-----------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    void *platform;
    struct rpmsg_device *rpdev;
    int ret;

    LPRINTF("Starting application...\r\n");

    /* Initialize platform */
    ret = platform_init(argc, argv, &platform);
    if (ret) {
        LPERROR("Failed to initialize platform.\r\n");
        ret = -1;
    } else {
        rpdev = platform_create_rpmsg_vdev(
                    platform,0,RPMSG_REMOTE,
                    NULL, NULL);

        if (!rpdev) {
            LPERROR("Failed to create rpmsg virtio device.\r\n");
            ret = -1;
        } else {
            app(rpdev, platform);
            platform_release_rpmsg_vdev(rpdev);
            ret = 0;
        }
    }

    LPRINTF("Stopping application...\r\n");
    platform_cleanup(platform);

    return ret;
}
