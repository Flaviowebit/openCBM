/*
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *  Copyright 2010 Spiro Trikaliotis
 *
*/

/*! **************************************************************
** \file libmisc/dynlibusb.h \n
** \author Spiro Trikaliotis \n
** \version $Id: dynlibusb.h,v 1.5 2010-08-15 08:06:57 wmsr Exp $ \n
** \n
** \brief Allow for libusb (1.0) to be loaded dynamically
**
****************************************************************/

#ifndef OPENCBM_LIBMISC_DYNLIBUSB_H
#define OPENCBM_LIBMISC_DYNLIBUSB_H

#include <libusb.h>

#include "getpluginaddress.h"

#ifndef LIBUSB_APIDECL
# define LIBUSB_APIDECL
#endif

typedef
struct usb_dll_s {
    SHARED_OBJECT_HANDLE shared_object_handle;

    /*
     * these definitions are taken directly from libusb.h. 
     * commented out entries are not currently unused.
     */

    int (LIBUSB_APIDECL *open)(libusb_device *dev, libusb_device_handle **handle);
    void (LIBUSB_APIDECL *close)(libusb_device_handle *dev);
    int (LIBUSB_APIDECL *bulk_transfer)(libusb_device_handle *dev_handle, unsigned char endpoint, unsigned char *data, int length, int *actual_length, unsigned int timeout);
    int (LIBUSB_APIDECL *control_transfer)(libusb_device_handle *dev_handle, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout);

    int (LIBUSB_APIDECL *set_configuration)(libusb_device_handle *dev, int configuration);
    int (LIBUSB_APIDECL *claim_interface)(libusb_device_handle *dev, int interface);
    int (LIBUSB_APIDECL *release_interface)(libusb_device_handle *dev, int interface);
    int (LIBUSB_APIDECL *clear_halt)(libusb_device_handle *dev, unsigned char ep);
    const char *(LIBUSB_APIDECL *error_name)(int error_code);
    int (LIBUSB_APIDECL *init)(libusb_context **ctx);
    void (LIBUSB_APIDECL *exit)(libusb_context *ctx);

    int (LIBUSB_APIDECL *get_device_descriptor)(libusb_device *dev,struct libusb_device_descriptor *desc);
    int (LIBUSB_APIDECL *get_string_descriptor_ascii)(libusb_device_handle *dev, uint8_t desc_index, unsigned char* data, int length);

    ssize_t (LIBUSB_APIDECL *get_device_list)(libusb_context *ctx, libusb_device ***list);
    void (LIBUSB_APIDECL *free_device_list)(libusb_device **list, int unref_devices);
    uint8_t (LIBUSB_APIDECL *get_bus_number)(libusb_device *dev);
    uint8_t (LIBUSB_APIDECL *get_device_address)(libusb_device *dev);
    libusb_device *(LIBUSB_APIDECL *get_device)(libusb_device_handle *devh);
} usb_dll_t;

extern usb_dll_t usb;

extern int dynlibusb_init(void);
extern void dynlibusb_uninit(void);

#endif /* #ifndef OPENCBM_LIBMISC_DYNLIBUSB_H */
