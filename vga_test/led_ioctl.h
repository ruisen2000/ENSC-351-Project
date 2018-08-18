#ifndef _LED_IOCTL_H
#define _LED_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>


//Note: These must be unique for each driver you create
#define LED_IOCTL_BASE 'L'

// READ/WRITE ops
#define LED_WRITE _IOW(LED_IOCTL_BASE, 0, __u32)


#endif

