#include "openamp.hpp"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/rpmsg.h>
#include <cstring>

#include <iostream>

#define RPMSG_GET_KFIFO_SIZE 1
#define RPMSG_GET_FREE_SPACE 3

#define RPMSG_BUS_SYS "/sys/bus/rpmsg"

int rpmsg_create_ept(int rpfd, struct rpmsg_endpoint_info *eptinfo)
{
    int ret;

    ret = ioctl(rpfd, RPMSG_CREATE_EPT_IOCTL, eptinfo);
    if (ret)
        perror("Failed to create endpoint.\n");
    return ret;
}

static char *get_rpmsg_ept_dev_name(const char *rpmsg_char_name,
                    const char *ept_name,
                    char *ept_dev_name)
{
    char sys_rpmsg_ept_name_path[64];
    char svc_name[64];
    const char *sys_rpmsg_path = "/sys/class/rpmsg";
    FILE *fp;
    int i;
    int ept_name_len;

    for (i = 0; i < 128; i++) {
        sprintf(sys_rpmsg_ept_name_path, "%s/%s/rpmsg%d/name",
            sys_rpmsg_path, rpmsg_char_name, i);
        printf("checking %s\n", sys_rpmsg_ept_name_path);
        if (access(sys_rpmsg_ept_name_path, F_OK) < 0)
            continue;
        fp = fopen(sys_rpmsg_ept_name_path, "r");
        if (!fp) {
            printf("failed to open %s\n", sys_rpmsg_ept_name_path);
            break;
        }
        fgets(svc_name, sizeof(svc_name), fp);
        fclose(fp);
        printf("svc_name: %s.\n",svc_name);
        ept_name_len = std::strlen(ept_name);
        if (ept_name_len > sizeof(svc_name))
            ept_name_len = sizeof(svc_name);
        if (!strncmp(svc_name, ept_name, ept_name_len)) {
            sprintf(ept_dev_name, "rpmsg%d", i);
            return ept_dev_name;
        }
    }

    printf("Not able to RPMsg endpoint file for %s:%s.\n",
           rpmsg_char_name, ept_name);
    return NULL;
}

static int bind_rpmsg_chrdev(const char *rpmsg_dev_name)
{
    char fpath[256];
    const char *rpmsg_chdrv = "rpmsg_chrdev";
    int fd;
    int ret;

    /* rpmsg dev overrides path */
    sprintf(fpath, "%s/devices/%s/driver_override",
        RPMSG_BUS_SYS, rpmsg_dev_name);
    fd = open(fpath, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s, %s\n",
            fpath, strerror(errno));
        return -EINVAL;
    }
    ret = write(fd, rpmsg_chdrv, strlen(rpmsg_chdrv) + 1);
    if (ret < 0) {
        fprintf(stderr, "Failed to write %s to %s, %s\n",
            rpmsg_chdrv, fpath, strerror(errno));
        return -EINVAL;
    }
    close(fd);

    /* bind the rpmsg device to rpmsg char driver */
    sprintf(fpath, "%s/drivers/%s/bind", RPMSG_BUS_SYS, rpmsg_chdrv);
    fd = open(fpath, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s, %s\n",
            fpath, strerror(errno));
        return -EINVAL;
    }
    ret = write(fd, rpmsg_dev_name, strlen(rpmsg_dev_name) + 1);
    if (ret < 0) {
        fprintf(stderr, "Failed to write %s to %s, %s\n",
            rpmsg_dev_name, fpath, strerror(errno));
        return -EINVAL;
    }
    close(fd);
    return 0;
}

static int get_rpmsg_chrdev_fd(const char *rpmsg_dev_name,
                   char *rpmsg_ctrl_name)
{
    char dpath[256];
    char fpath[300];
    const char *rpmsg_ctrl_prefix = "rpmsg_ctrl";
    DIR *dir;
    struct dirent *ent;
    int fd;

    sprintf(dpath, "%s/devices/%s/rpmsg", RPMSG_BUS_SYS, rpmsg_dev_name);
    dir = opendir(dpath);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open dir %s\n", dpath);
        return -EINVAL;
    }
    while ((ent = readdir(dir)) != NULL) {
        if (!strncmp(ent->d_name, rpmsg_ctrl_prefix,
                strlen(rpmsg_ctrl_prefix))) {
            printf("Opening file %s.\n", ent->d_name);
            sprintf(fpath, "/dev/%s", ent->d_name);
            fd = open(fpath, O_RDWR /*| O_NONBLOCK*/);
            if (fd < 0) {
                fprintf(stderr,
                    "Failed to open rpmsg char dev %s,%s\n",
                    fpath, strerror(errno));
                return fd;
            }
            sprintf(rpmsg_ctrl_name, "%s", ent->d_name);
            return fd;
        }
    }

    fprintf(stderr, "No rpmsg char dev file is found\n");
    return -EINVAL;
}

OpenAMPComm::~OpenAMPComm()
{
    if (fd >= 0) close(fd);
    if (charfd >= 0)
        close(charfd);
}

std::string OpenAMPComm::GetInterfaceName()
{
    return "OpenAMP";
}

bool OpenAMPComm::Initialize()
{
    int ret;
    const char *rpmsg_dev="virtio0.rpmsg-openamp-demo-channel.-1.0";
    char rpmsg_char_name[16];
    char fpath[256];
    struct rpmsg_endpoint_info eptinfo;
    char ept_dev_name[16];
    char ept_dev_path[32];

    /* Load rpmsg_char driver */
    printf("\r\nMaster>probe rpmsg_char\r\n");
    ret = system("modprobe rpmsg_char");
    if (ret < 0) {
        perror("Failed to load rpmsg_char driver.\n");
        return false;
    }

    printf("\r\n Open rpmsg dev %s! \r\n", rpmsg_dev);
    sprintf(fpath, "%s/devices/%s", RPMSG_BUS_SYS, rpmsg_dev);
    if (access(fpath, F_OK)) {
        fprintf(stderr, "Not able to access rpmsg device %s, %s\n",
            fpath, strerror(errno));
        return false;
    }
    ret = bind_rpmsg_chrdev(rpmsg_dev);
    if (ret < 0)
        return ret;
    charfd = get_rpmsg_chrdev_fd(rpmsg_dev, rpmsg_char_name);
    if (charfd < 0)
        return false;

    /* Create endpoint from rpmsg char driver */
    strcpy(eptinfo.name, "rpmsg-openamp-demo-channel");
    eptinfo.src = 0;
    eptinfo.dst = 0xFFFFFFFF;
    ret = rpmsg_create_ept(charfd, &eptinfo);
    if (ret) {
        printf("failed to create RPMsg endpoint.\n");
        return false;
    }
    if (!get_rpmsg_ept_dev_name(rpmsg_char_name, eptinfo.name,
                    ept_dev_name))
        return -EINVAL;
    sprintf(ept_dev_path, "/dev/%s", ept_dev_name);
    fd = open(ept_dev_path, O_RDWR /*| O_NONBLOCK*/);
    if (fd < 0) {
        perror("Failed to open rpmsg device.");
        close(charfd);
        return false;
    }

    return true;
}

void OpenAMPComm::Send(Target t, const uint8_t* data, size_t size)
{
    ssize_t written = write(fd, data, size);
    if (written != (ssize_t)size) {
        std::cerr << "Failed to write " << size << ". ret: " << written << std::endl;
    }
}

Target OpenAMPComm::ReceiveAnyBlock(uint8_t* data, size_t& size)
{
    ssize_t ret = 0;
    do {
        ret = read(fd, data, size);
    } while (ret == -1 && errno == EAGAIN);

    if (ret == -1) {
        perror("Failed to read");
    }

    return Target::T0;
}

uint16_t OpenAMPComm::GetMaxPacketSize() const
{
    return 496;
}
