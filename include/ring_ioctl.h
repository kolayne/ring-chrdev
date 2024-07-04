#ifndef RING_IOCTL
#define RING_IOCTL

#include <linux/ioctl.h>
#include <linux/types.h>

// https://www.kernel.org/doc/html/latest/userspace-api/ioctl/ioctl-number.html
#define IO_TYPE     'r'
#define IO_NR_START 0x20


struct last_acc {
  uid_t uid;
  pid_t tgid;
};

#define IR_LAST_WRITER _IOR(IO_TYPE, IO_NR_START + 0, struct last_acc *)
#define IR_LAST_READER _IOR(IO_TYPE, IO_NR_START + 1, struct last_acc *)

#endif  // RING_IOCTL
