#include <asm/errno.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/mm.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#define CLASS_NAME "ipc_tunnel"
#define DEVICE_NAME "ipc_tunnel"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lassi Hämäläinen");
MODULE_DESCRIPTION("CPU0 -> CPU1 IPC packet tunnel");
MODULE_VERSION("0.3");

/* Undefine this to use device memory type */
#define USE_CACHED_MEMORY 1

#define DEVICE_COUNT 6

struct TunnelInstance {
    const struct TunnelConfig* config;

    struct ControlHeader* control_header;
    uint8_t* send_buffer;
    uint8_t* receive_buffer;
    uint16_t send_packet_size;
    uint16_t receive_packet_size;


    dev_t dev;
    struct cdev c_dev;

    int is_open;

    wait_queue_head_t read_queue;
};

struct TunnelConfig {
    uint32_t control_header_address;

    uint32_t send_buffer_address;
    uint32_t receive_buffer_address;

    uint16_t send_max_packet_size;
    uint16_t send_buffered_packet_count;
    uint16_t receive_max_packet_size;
    uint16_t receive_buffered_packet_count;

    uint32_t shared_buffer_address;
    uint32_t shared_buffer_size;

    int cpu0_notify_ipi;

    void (*cpu0_notify_ipi_handler)(void);
};

struct ControlHeader {
    uint32_t cpu0_write_index;
    uint32_t cpu0_read_index;

    uint32_t _padding1[6];

    volatile uint32_t cpu1_write_index;
    volatile uint32_t cpu1_read_index;

    uint32_t _padding2[6];
};

struct PacketHeader {
    uint32_t packet_size;
    uint32_t reserved_;
    uint64_t data[0];
};

struct ReadPacket {
    struct PacketHeader* packet;
    uint32_t next_read_index;
};

struct WritePacket {
    struct PacketHeader* packet;
    uint32_t next_write_index;
};

/* External Zynq spesific functions */
extern int set_ipi_handler(int ipinr, void* handler, char* desc);
extern void clear_ipi_handler(int ipinr);


static void tunnel0_ipi_notify_handler(void);
static void tunnel1_ipi_notify_handler(void);
static void tunnel2_ipi_notify_handler(void);
static void tunnel3_ipi_notify_handler(void);
static void tunnel4_ipi_notify_handler(void);
static void tunnel5_ipi_notify_handler(void);



static const struct TunnelConfig tunnel_configs[DEVICE_COUNT] = {
    {
        .control_header_address = 0xFFFF0000,
        .send_buffer_address = 0xFFFF0040,
        .receive_buffer_address = 0xFFFF1100,

        .send_max_packet_size = 0x780,
        .send_buffered_packet_count = 2,
        .receive_max_packet_size = 0x780,
        .receive_buffered_packet_count = 2,

        .shared_buffer_address = 0xFFFFE000,
        .shared_buffer_size = 0x1000,

        .cpu0_notify_ipi = 14,
        .cpu0_notify_ipi_handler = &tunnel0_ipi_notify_handler
    },
    {
        .control_header_address = 0xFFFF3000,
        .send_buffer_address = 0xFFFF3040,
        .receive_buffer_address = 0xFFFF5100,

        .send_max_packet_size = 0x780,
        .send_buffered_packet_count = 4,
        .receive_max_packet_size = 0x780,
        .receive_buffered_packet_count = 4,

        .shared_buffer_address = 0x0,
        .shared_buffer_size = 0x0,

        .cpu0_notify_ipi = 13,

        .cpu0_notify_ipi_handler = &tunnel1_ipi_notify_handler
    },
    {
        .control_header_address = 0xFFFF7200,
        .send_buffer_address = 0xFFFF7240,
        .receive_buffer_address = 0xFFFF9800,

        .send_max_packet_size = 0x1200,
        .send_buffered_packet_count = 2,
        .receive_max_packet_size = 0x1200,
        .receive_buffered_packet_count = 2,

        .shared_buffer_address = 0xFFFFC000,
        .shared_buffer_size = 0x3000,

        .cpu0_notify_ipi = 12,

        .cpu0_notify_ipi_handler = &tunnel2_ipi_notify_handler
    },
    {
        .control_header_address = 0x3FFF0000,
        .send_buffer_address = 0x3FFF0040,
        .receive_buffer_address = 0x3FFF1100,

        .send_max_packet_size = 0x780,
        .send_buffered_packet_count = 2,
        .receive_max_packet_size = 0x780,
        .receive_buffered_packet_count = 2,

        .shared_buffer_address = 0x3FFFE000,
        .shared_buffer_size = 0x1000,

        .cpu0_notify_ipi = 11,
        .cpu0_notify_ipi_handler = &tunnel3_ipi_notify_handler
    },
    {
        .control_header_address = 0x3FFF3000,
        .send_buffer_address = 0x3FFF3040,
        .receive_buffer_address = 0x3FFF5100,

        .send_max_packet_size = 0x780,
        .send_buffered_packet_count = 4,
        .receive_max_packet_size = 0x780,
        .receive_buffered_packet_count = 4,

        .shared_buffer_address = 0x0,
        .shared_buffer_size = 0x0,

        .cpu0_notify_ipi = 10,

        .cpu0_notify_ipi_handler = &tunnel4_ipi_notify_handler
    },
    {
        .control_header_address = 0x3FFF7200,
        .send_buffer_address = 0x3FFF7240,
        .receive_buffer_address = 0x3FFF9800,

        .send_max_packet_size = 0x1200,
        .send_buffered_packet_count = 2,
        .receive_max_packet_size = 0x1200,
        .receive_buffered_packet_count = 2,

        .shared_buffer_address = 0x3FFFC000,
        .shared_buffer_size = 0x2000,

        .cpu0_notify_ipi = 9,

        .cpu0_notify_ipi_handler = &tunnel5_ipi_notify_handler
    },
};



static struct TunnelInstance tunnels[DEVICE_COUNT];
static struct class* device_class = NULL;
static dev_t first_device_number;

#ifdef USE_CACHED_MEMORY
static uint32_t get_cpu0_write_index(struct ControlHeader* header)
{
    return header->cpu0_write_index;
}

static uint32_t get_cpu1_read_index(struct ControlHeader* header)
{
    return smp_load_acquire(&header->cpu1_read_index);
}

static uint32_t get_cpu0_read_index(struct ControlHeader* header)
{
    return header->cpu0_read_index;
}

static uint32_t get_cpu1_write_index(struct ControlHeader* header)
{
    return smp_load_acquire(&header->cpu1_write_index);
}

#else

static uint32_t get_cpu0_write_index(struct ControlHeader* header)
{
    return readl(&header->cpu0_write_index);
}

static uint32_t get_cpu1_read_index(struct ControlHeader* header)
{
    return readl(&header->cpu1_read_index);
}

static uint32_t get_cpu0_read_index(struct ControlHeader* header)
{
    return readl(&header->cpu0_read_index);
}

static uint32_t get_cpu1_write_index(struct ControlHeader* header)
{
    return  readl(&header->cpu1_write_index);
}

#endif

static struct PacketHeader* get_read_packet(struct TunnelInstance* tunnel, uint32_t read_index) {
    return (struct PacketHeader*)(tunnel->receive_buffer + tunnel->receive_packet_size * read_index);
}

static struct PacketHeader* get_write_packet(struct TunnelInstance* tunnel, uint32_t write_index) {
    return (struct PacketHeader*)(tunnel->send_buffer + tunnel->send_packet_size * write_index);
}

static int try_get_read_packet(struct TunnelInstance* tunnel, struct ReadPacket* packet)
{
    uint32_t readIndex;
    uint32_t cpu1WriteIndex;
    readIndex = get_cpu0_read_index(tunnel->control_header);
    cpu1WriteIndex = get_cpu1_write_index(tunnel->control_header);

    if (readIndex != cpu1WriteIndex) {
        packet->packet = get_read_packet(tunnel, readIndex);

        if (++readIndex == tunnel->config->receive_buffered_packet_count) {
            readIndex = 0;
        }

        packet->next_read_index = readIndex;
        return 1;
    }

    return 0;
}

static int try_get_write_packet(struct TunnelInstance* tunnel, struct WritePacket* packet)
{
    uint32_t writeIndex;
    uint32_t cpu1ReadIndex;
    uint32_t nextWriteIndex;

    writeIndex = get_cpu0_write_index(tunnel->control_header);

    nextWriteIndex = writeIndex + 1;
    if (nextWriteIndex == tunnel->config->send_buffered_packet_count) {
        nextWriteIndex = 0;
    }

    cpu1ReadIndex = get_cpu1_read_index(tunnel->control_header);

    if (nextWriteIndex != cpu1ReadIndex) {
        packet->next_write_index = nextWriteIndex;
        packet->packet = get_write_packet(tunnel, writeIndex);
        return 1;
    }

    return 0;
}

#ifdef USE_CACHED_MEMORY
static void mark_packet_as_read(struct TunnelInstance* tunnel, struct ReadPacket* packet) {
    smp_store_release(&tunnel->control_header->cpu0_read_index, packet->next_read_index);
}

static void send_packet(struct TunnelInstance* tunnel, struct WritePacket* packet) {
    smp_store_release(&tunnel->control_header->cpu0_write_index, packet->next_write_index);
}
#else
static void mark_packet_as_read(struct TunnelInstance* tunnel, struct ReadPacket* packet) {
    dsb();
    writel(packet->next_read_index, &tunnel->control_header->cpu0_read_index);
}

static void send_packet(struct TunnelInstance* tunnel, struct WritePacket* packet) {
    dsb();
    writel(packet->next_write_index, &tunnel->control_header->cpu0_write_index);
}
#endif


static int dev_open(struct inode* inodep, struct file* filep)
{
    int i;
    struct TunnelInstance *tunnel = 0;

    for (i = 0; i < DEVICE_COUNT; ++i)
    {
        if (inodep->i_cdev == &tunnels[i].c_dev)
        {
            tunnel = &tunnels[i];
            break;
        }
    }

    if (!tunnel) {
        printk(KERN_ERR "Can't find chardev for inode\n");
        return -EINVAL;
    }

    filep->private_data = tunnel;

    printk(KERN_INFO "dev_open %p\n", (void*)tunnel);
    if (tunnel->is_open)
    {
        return -EBUSY;
    }

    tunnel->is_open = 1;

    printk(KERN_INFO "CPU1_IPC_TUNNEL Opened\n");
    return 0;
}

static ssize_t dev_read(struct file* filep, char __user* buffer, size_t len, loff_t* offset)
{
    uint32_t rx = 0;
    struct ReadPacket packet;
    struct TunnelInstance* tunnel = (struct TunnelInstance*)filep->private_data;

    if (filep->f_flags & O_NONBLOCK) {
        /* non-blocking IO */

        if (!try_get_read_packet(tunnel, &packet)) {
            /* No data in queue */
            return -EAGAIN;
        }
    } else if (wait_event_interruptible(tunnel->read_queue, try_get_read_packet(tunnel, &packet)) != 0) {
        /* Waiting for data was interrupted */
        return -EINTR;
    }

    /* Read queue contains something */

    rx = packet.packet->packet_size;
    if (len < rx) {
        /* If read buffer is smaller than the package, part of the data is lost */
        rx = len;
    }

    if (copy_to_user(buffer, packet.packet->data, rx) != 0) {
        return -EFAULT;
    }

    mark_packet_as_read(tunnel, &packet);

    return (ssize_t)rx;
}

static ssize_t dev_write(struct file* filep, const char __user* buffer, size_t len, loff_t* offset)
{
    struct WritePacket write;
    struct TunnelInstance* tunnel = (struct TunnelInstance*)filep->private_data;

    if (len == 0) {
        return 0;
    }

    if (!tunnel) {
        return -EBADFD;
    }

    if (len > tunnel->config->send_max_packet_size) {
        /* Packet doesn't fit in the ring buffer */
        return -EFBIG;
    }

    if (try_get_write_packet(tunnel, &write)) {
        if (copy_from_user(write.packet->data, buffer, len) != 0) {
            return -EFAULT;
        }

        write.packet->packet_size = len;

        send_packet(tunnel, &write);
        return len;
    }

    return 0;
}

static unsigned int dev_poll(struct file* file, poll_table* wait)
{
    struct TunnelInstance* tunnel = (struct TunnelInstance*)file->private_data;
    poll_wait(file, &tunnel->read_queue, wait);

    if (   get_cpu0_read_index(tunnel->control_header)
        != get_cpu1_write_index(tunnel->control_header)) {
        /* there is readable data in the queue */
        return POLLIN | POLLRDNORM;
    }

    return 0;
}

static int dev_mmap(struct file* filep, struct vm_area_struct *vma)
{
    struct TunnelInstance* tunnel = (struct TunnelInstance*)filep->private_data;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

    unsigned long physical_base = tunnel->config->shared_buffer_address;
    unsigned long physical_size = tunnel->config->shared_buffer_size;
    unsigned long virtual_size = vma->vm_end - vma->vm_start;
    unsigned long pfn = (physical_base + offset) >> PAGE_SHIFT;

    /* Memory should be marked as SHARED (no COW if process is forked) */
    if ((vma->vm_flags & VM_SHARED) == 0) {
        return -EINVAL;
    }

    if (offset >= physical_size || physical_size - offset < virtual_size) {
        return -EINVAL;
    }
    vma->vm_flags |= VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	
#ifdef USE_CACHED_MEMORY
    vma->vm_page_prot = __pgprot_modify(vma->vm_page_prot, L_PTE_MT_MASK, __PAGE_SHARED | L_PTE_MT_WRITEBACK);
#else
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif
    return remap_pfn_range(vma,
                           vma->vm_start,
                           pfn,
                           virtual_size,
                           vma->vm_page_prot);
}

static int dev_release(struct inode* inodep, struct file* filep)
{
    struct TunnelInstance *tunnel = (struct TunnelInstance*)filep->private_data;
    tunnel->is_open = 0;

    printk(KERN_INFO "CPU1_IPC_TUNNEL Released\n");
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
    .poll = dev_poll,
    .mmap = dev_mmap
};

/* Called when software interrupt CPU0_NOTIFY_IRQ is triggered
 * set_ipi_handler doesn't allow adding any extra data so we
 * need a separate handler for each tunnel
*/
static void tunnel0_ipi_notify_handler(void)
{
    wake_up_interruptible(&tunnels[0].read_queue);
}

static void tunnel1_ipi_notify_handler(void)
{
    wake_up_interruptible(&tunnels[1].read_queue);
}

static void tunnel2_ipi_notify_handler(void)
{
    wake_up_interruptible(&tunnels[2].read_queue);
}

static void tunnel3_ipi_notify_handler(void)
{
    wake_up_interruptible(&tunnels[3].read_queue);
}

static void tunnel4_ipi_notify_handler(void)
{
    wake_up_interruptible(&tunnels[4].read_queue);
}

static void tunnel5_ipi_notify_handler(void)
{
    wake_up_interruptible(&tunnels[5].read_queue);
}


static int __init ipc_tunnel_init(void)
{
    int i;
    int j;
    int ret = 0;
    struct device* dev_instance;

    for (i = 0; i < DEVICE_COUNT; ++i)
    {
        tunnels[i].dev = 0;
        tunnels[i].config = &tunnel_configs[i];
        tunnels[i].is_open = 0;
        tunnels[i].control_header = NULL;
        tunnels[i].send_buffer = NULL;
        tunnels[i].receive_buffer = NULL;
        tunnels[i].send_packet_size = 0;
        tunnels[i].receive_packet_size = 0;
    }


    printk(KERN_INFO "CPU1_IPC_TUNNEL: Initializing the CPU1_IPC_TUNNEL LKM\n");
    ret = alloc_chrdev_region(
        &first_device_number,
        0,
        DEVICE_COUNT,
        DEVICE_NAME);

    if (ret < 0) {
        printk(KERN_ALERT "CPU1_IPC_TUNNEL failed to register a major number\n");
        return ret;
    }
    printk(KERN_INFO "CPU1_IPC_TUNNEL: registered correctly with major number %d\n", MAJOR(ret));

    // Register the device class
    device_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(device_class)) {
        printk(KERN_ALERT "Failed to register device class\n");
        ret = PTR_ERR(device_class);
        goto unregister_chrdevs;
    }

    printk(KERN_INFO "CPU1_IPC_TUNNEL: device class registered correctly\n");

    for (i = 0; i < DEVICE_COUNT; ++i) {

        dev_t dev_num = MKDEV(MAJOR(first_device_number),
                              MINOR(first_device_number) + i);

        dev_instance = device_create(
            device_class,
            NULL,
            dev_num,
            NULL,
            "ipc_tunnel%d",
            i);

        if (IS_ERR(dev_instance)) {
            ret = PTR_ERR(dev_instance);
            goto destroy_devices;
        }

        tunnels[i].dev = dev_num;
    }

    for (i = 0; i < DEVICE_COUNT; ++i)
    {
        cdev_init(&tunnels[i].c_dev, &fops);
        if ((ret = cdev_add(&tunnels[i].c_dev, tunnels[i].dev, 1)) < 0)
        {
            for (j = i - 1; j >= 0; --j)
            {
                cdev_del(&tunnels[j].c_dev);
            }

            goto destroy_devices;
        }
    }

    /* Map shared memory regions */
    for (i = 0; i < DEVICE_COUNT; ++i)
    {
        tunnels[i].control_header =
                (struct ControlHeader*) memremap(
                    tunnel_configs[i].control_header_address,
                    sizeof(struct ControlHeader),
                    MEMREMAP_WB);

#ifdef USE_CACHED_MEMORY
        /* send buffer element size is aligned to 32 bytes (cache line size) */
        tunnels[i].send_packet_size = (tunnel_configs[i].send_max_packet_size
                                     + sizeof(struct PacketHeader) + 31) & ~31u;
		tunnels[i].send_buffer =
				(uint8_t*) memremap(
					tunnel_configs[i].send_buffer_address,
					tunnels[i].send_packet_size * tunnel_configs[i].send_buffered_packet_count,
					MEMREMAP_WB);

		/* receive buffer element size is aligned to 32 bytes (cache line size) */
        tunnels[i].receive_packet_size = (tunnel_configs[i].receive_max_packet_size
                                        + sizeof(struct PacketHeader) + 31) & ~31u;

		tunnels[i].receive_buffer =
				(uint8_t*) memremap(
					tunnel_configs[i].receive_buffer_address,
					tunnels[i].receive_packet_size * tunnel_configs[i].receive_buffered_packet_count,
					MEMREMAP_WB);
#else
		/* send buffer element size is aligned to 64 bits */
        tunnels[i].send_packet_size = (tunnel_configs[i].send_max_packet_size
                                     + sizeof(struct PacketHeader) + 7) & ~7u;
        tunnels[i].send_buffer =
                (uint8_t*) ioremap_wc(
                    tunnel_configs[i].send_buffer_address,
                    tunnels[i].send_packet_size * tunnel_configs[i].send_buffered_packet_count);

        /* receive buffer element size is aligned to 64 bits */
        tunnels[i].receive_packet_size = (tunnel_configs[i].receive_max_packet_size
                                        + sizeof(struct PacketHeader) + 7) & ~7u;
		tunnels[i].receive_buffer =
				(uint8_t*) ioremap_wc(
					tunnel_configs[i].receive_buffer_address,
					tunnels[i].receive_packet_size * tunnel_configs[i].receive_buffered_packet_count);
#endif	

        if (   !tunnels[i].control_header
            || !tunnels[i].send_buffer
            || !tunnels[i].receive_buffer)
        {
            ret = -ENOMEM;
            goto unmap_memory;
        }
    }

    for (i = 0; i < DEVICE_COUNT; ++i)
    {
        init_waitqueue_head(&tunnels[i].read_queue);

        set_ipi_handler(tunnel_configs[i].cpu0_notify_ipi,
                        (void*)tunnel_configs[i].cpu0_notify_ipi_handler,
                        "IPC_TUNNEL_CPU0_NOTIFY");
    }

    printk(KERN_INFO "CPU1_IPC_TUNNEL: device class created correctly\n");
    return 0;

unmap_memory:
    for (i = 0; i < DEVICE_COUNT; ++i)
    {
#ifdef USE_CACHED_MEMORY
        if (tunnels[i].control_header)
        {
            memunmap(tunnels[i].control_header);
        }

        if (tunnels[i].send_buffer)
        {
            memunmap(tunnels[i].send_buffer);
        }

        if (tunnels[i].receive_buffer)
        {
            memunmap(tunnels[i].receive_buffer);
        }
#else
		if (tunnels[i].control_header)
        {
            iounmap(tunnels[i].control_header);
        }

        if (tunnels[i].send_buffer)
        {
            iounmap(tunnels[i].send_buffer);
        }

        if (tunnels[i].receive_buffer)
        {
            iounmap(tunnels[i].receive_buffer);
        }
#endif
    }

    for (i = 0; i < DEVICE_COUNT; ++i)
    {
        cdev_del(&tunnels[i].c_dev);
    }

destroy_devices:
    for (i = 0; i < DEVICE_COUNT; ++i)
    {
        if (tunnels[i].dev != 0)
        {
            device_destroy(device_class, tunnels[i].dev);
        }
    }

    class_destroy(device_class);

unregister_chrdevs:
    unregister_chrdev_region(first_device_number, DEVICE_COUNT);

    return ret;
}

static void __exit ipc_tunnel_exit(void)
{
    int i;

    for (i = 0; i < DEVICE_COUNT; ++i)
    {
        clear_ipi_handler(tunnel_configs[i].cpu0_notify_ipi);

#if USE_CACHED_MEMORY
        memunmap(tunnels[i].control_header);
        memunmap(tunnels[i].send_buffer);
        memunmap(tunnels[i].receive_buffer);
#else
		iounmap(tunnels[i].control_header);
        iounmap(tunnels[i].send_buffer);
        iounmap(tunnels[i].receive_buffer);
#endif

        cdev_del(&tunnels[i].c_dev);

        device_destroy(device_class, tunnels[i].dev);
    }

    class_destroy(device_class);
    unregister_chrdev_region(first_device_number, DEVICE_COUNT);

    printk(KERN_INFO "CPU1_IPC_TUNNEL: Exit\n");
}

module_init(ipc_tunnel_init);
module_exit(ipc_tunnel_exit);
