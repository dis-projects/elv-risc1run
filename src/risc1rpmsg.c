#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include <sys/ioctl.h>
#include <linux/rpmsg.h>

#define RPMSG_BUS_SYS "/sys/bus/rpmsg"
#define RPMSG_SERVICE_NAME "rpmsg-lite-demo-channel"

/* Bind driver and create endpoint */
int risc1Bind(int rid)
{
    struct rpmsg_endpoint_info info;
    char *rpmsg_dev="virtio0.rpmsg-lite-demo-channel.-1.30";
    char buffer[256];
    int id;

    id = open("/dev/rpmsg0", O_RDWR);
    if (id >= 0) {
        fprintf(stderr, "rpmsg dev is available\n");
        return id;
    }

    /* First step
       echo rpmsg_chrdev > /sys/bus/rpmsg/devices/virtio0.rpmsg-lite-demo-channel.-1.30/driver_override 
    */
    sprintf(buffer, RPMSG_BUS_SYS "/devices/%s/driver_override", rpmsg_dev);
    id = open(buffer, O_WRONLY);
    if (id < 0) {
        fprintf(stderr, "Can't bind virtio\n");
        return id;
    }
    write(id, "rpmsg_chrdev", sizeof("rpmsg_chrdev"));
    close(id);
    /* Second step
        echo virtio0.rpmsg-lite-demo-channel.-1.30 > /sys/bus/rpmsg/drivers/rpmsg_chrdev/bind
    */
    id = open(RPMSG_BUS_SYS "/drivers/rpmsg_chrdev/bind", O_WRONLY);
    if (id < 0) {
        fprintf(stderr, "Can't bind chrdev\n");
        return id;
    }
    write(id, rpmsg_dev, strlen(rpmsg_dev));
    close(id);

    sleep(1);

    id = open("/dev/rpmsg_ctrl0", O_RDWR);
    if (id < 0) {
        fprintf(stderr, "Can't open rpmsg control\n");
        return id;
    }

    strncpy(info.name, RPMSG_SERVICE_NAME, 31);
    info.name[31] = '\0';
    info.dst = 0;

    /* Create endpoint 0 + 2 with file /dev/rpmsgI */
    info.src = 0 + 2;
    if (ioctl(id, RPMSG_CREATE_EPT_IOCTL, &info) < 0) {
        perror("create endpoint");
        return -1;
    }

    sleep(1);

    id = open("/dev/rpmsg0", O_RDWR);

    return id;
}
