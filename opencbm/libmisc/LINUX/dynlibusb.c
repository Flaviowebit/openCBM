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
** \file libmisc/LINUX/dynlibusb.h \n
** \author Spiro Trikaliotis \n
** \version $Id: dynlibusb.c,v 1.4 2010-08-15 09:01:54 wmsr Exp $ \n
** \n
** \brief Allow for libusb (1.0) to be loaded dynamically
****************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "opencbm.h"

#include "arch.h"
#include "dynlibusb.h"
#include "getpluginaddress.h"

usb_dll_t usb = {
    .shared_object_handle = NULL,
    .open = libusb_open,
    .close = libusb_close,
    .bulk_transfer = libusb_bulk_transfer,
    .control_transfer = libusb_control_transfer,
    .set_configuration = libusb_set_configuration,
    .claim_interface = libusb_claim_interface,
    .release_interface = libusb_release_interface,
    .clear_halt = libusb_clear_halt,
    .error_name = libusb_error_name,
    .init = libusb_init,
    .exit = libusb_exit,
    .get_device_descriptor = libusb_get_device_descriptor,
    .get_string_descriptor_ascii = libusb_get_string_descriptor_ascii,
    .get_device = libusb_get_device,
    .get_device_list = libusb_get_device_list,
    .free_device_list = libusb_free_device_list,
    .get_bus_number = libusb_get_bus_number,
    .get_device_address = libusb_get_device_address,
};

int dynlibusb_init(void) {
    int error = 0;

    return error;
}

void dynlibusb_uninit(void) {
}
