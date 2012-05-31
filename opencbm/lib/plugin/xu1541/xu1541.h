#ifndef XU1541_H
#define XU1541_H

#include <stdio.h>
#include <libusb.h>

#include "opencbm.h"

#ifndef FUNC_ENTER
  #define FUNC_ENTER()
#endif

struct xu1541_usb_handle {
	libusb_context *ctx;
	libusb_device_handle *devh;
};

/* time out 10% after device itself times out */
/* so make sure we should normally never time out on usb */
#define USB_TIMEOUT (XU1541_W4L_TIMEOUT * 1100)

#include "xu1541_types.h"

/* vendor and product id (donated by ftdi) */
#define XU1541_VID  0x0403
#define XU1541_PID  0xc632

/* calls required for standard io */
extern int xu1541_init(struct xu1541_usb_handle **HandleXu1541);
extern void xu1541_close(struct xu1541_usb_handle *HandleXu1541);
extern int xu1541_ioctl(struct xu1541_usb_handle *HandleXu1541, unsigned int cmd, unsigned int addr, unsigned int secaddr);
extern int xu1541_write(struct xu1541_usb_handle *HandleXu1541, const __u_char *data, size_t len);
extern int xu1541_read(struct xu1541_usb_handle *HandleXu1541, __u_char *data, size_t len);

/* calls for speeder supported modes */
extern int xu1541_special_write(struct xu1541_usb_handle *HandleXu1541, int mode, const __u_char *data, size_t size);
extern int xu1541_special_read(struct xu1541_usb_handle *HandleXu1541, int mode, __u_char *data, size_t size);

#endif // XU1541_H
