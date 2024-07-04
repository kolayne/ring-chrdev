#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

#include "../include/ring_ioctl.h"

int main(int argc, const char *argv[]) {
    if (argc <= 2) {
        const char *default_[] = {"", "/dev/ring", NULL};
        argv = default_;
    }

    int fd = open(argv[1], 0);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct last_acc last;
    int res = ioctl(fd, IR_LAST_WRITER, &last);
    if (res < 0) {
        perror("ioctl 1");
        return 1;
    }
    printf("Last writer: tgid=%d, uid=%d\n", last.tgid, last.uid);
    res = ioctl(fd, IR_LAST_READER, &last);
    if (res < 0) {
        perror("ioctl 1");
        return 1;
    }
    printf("Last reader: tgid=%d, uid=%d\n", last.tgid, last.uid);
}
