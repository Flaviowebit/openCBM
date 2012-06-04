/*
 * xum1541 driver for bulk and control messages
 * Copyright 2009-2010 Nate Lawson <nate@root.org>
 * Copyright 2010 Spiro Trikaliotis
 *
 * Incorporates some code from the xu1541 driver by:
 * Copyright 2007 Till Harbaum <till@harbaum.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

/*! **************************************************************
** \file lib/plugin/xum1541/xum1541.c \n
** \author Nate Lawson \n
** \version $Id: xum1541.c,v 1.19 2010-09-12 17:00:49 wmsr Exp $ \n
** \n
** \brief libusb-based xum1541 access routines
****************************************************************/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "opencbm.h"

#include "arch.h"
#include "dynlibusb.h"
#include "getpluginaddress.h"
#include "xum1541.h"

// XXX Fix for Linux/Mac build, should be moved
#ifndef LIBUSB_PATH_MAX
#define LIBUSB_PATH_MAX 512
#endif

static int debug_level = -1; /*!< \internal \brief the debugging level for debugging output */

/*! \internal \brief Output debugging information for the xum1541

 \param level
   The output level; output will only be produced if this level is less or equal the debugging level

 \param msg
   The printf() style message to be output
*/
static void
xum1541_dbg(int level, char *msg, ...)
{
    va_list argp;

    /* determine debug mode if not yet known */
    if (debug_level == -1) {
        char *val = getenv("XUM1541_DEBUG");
        if (val)
            debug_level = atoi(val);
    }

    if (level <= debug_level) {
        fprintf(stderr, "[XUM1541] ");
        va_start(argp, msg);
        vfprintf(stderr, msg, argp);
        va_end(argp);
        fprintf(stderr, "\n");
    }
}

// Cleanup after a failure
static void
xum1541_cleanup(libusb_device_handle *HandleXum1541, char *msg, ...)
{
    va_list args;

    if (msg != NULL) {
        va_start(args, msg);
        fprintf(stderr, msg, args);
        va_end(args);
    }
    if (HandleXum1541 != NULL)
        usb.close(HandleXum1541);
}

// USB bus enumeration
static void
xum1541_enumerate(struct xum1541_usb_handle *uh, int PortNumber)
{
    static char xumProduct[] = "xum1541"; // Start of USB product string id
    static int prodLen = sizeof(xumProduct) - 1;
    unsigned char string[256];
    int len, serialnum, leastserial;
    libusb_device **list;
    libusb_device_handle *found = NULL;
    libusb_device *preferredDefaultHandle = NULL;
    struct libusb_device_descriptor descriptor;
    ssize_t cnt;
    ssize_t i = 0;
    int err = 0;

    if (PortNumber < 0 || PortNumber > MAX_ALLOWED_XUM1541_SERIALNUM) {
        // Normalise the Portnumber for invalid values
        PortNumber = 0;
    }

    xum1541_dbg(0, "scanning usb ...");

    // discover devices
    uh->devh = NULL;
    leastserial = MAX_ALLOWED_XUM1541_SERIALNUM + 1;

    cnt = usb.get_device_list(uh->ctx, &list);
    if (cnt < 0)
    {
        xum1541_dbg(0, "enumeration error: %s", usb.error_name(cnt));
        return;
    }

    for (i = 0; i < cnt; i++)
    {
        libusb_device *device = list[i];
        if (LIBUSB_SUCCESS != usb.get_device_descriptor(device, &descriptor))
            continue;

        xum1541_dbg(1, "device %04x:%04x", descriptor.idVendor, descriptor.idProduct);

        // First, find our vendor and product id
        if (descriptor.idVendor != XUM1541_VID || descriptor.idProduct != XUM1541_PID)
            continue;

        xum1541_dbg(0, "found xu/xum1541 version %04x on bus %d, device %d",
            descriptor.bcdDevice, usb.get_bus_number(device),
            usb.get_device_address(device));

        err = usb.open(device, &found);
        if (LIBUSB_SUCCESS != err) {
            fprintf(stderr, "error: Cannot open USB device: %s\n",
               usb.error_name(err));
            continue;
        }

        // Get device product name and try to match against "xum1541".
        // If no match, it could be an xum1541 so don't report an error.
        len = usb.get_string_descriptor_ascii(found, descriptor.iProduct,
            string, sizeof(string) - 1);
        if (len < 0) {
            xum1541_cleanup(found, "error: cannot query product name: %s\n",
                usb.error_name(len));
            continue;
        }

        string[len] = '\0';
        if (len < prodLen || strstr((char *)string, xumProduct) == NULL) {
            xum1541_cleanup(found, NULL);
            continue;
        }
        xum1541_dbg(0, "xum1541 name: %s", string);

        len = usb.get_string_descriptor_ascii(found, descriptor.iSerialNumber,
            string, sizeof(string) - 1);
        if (len < 0 && PortNumber != 0) {
            // we need the serial number, when PortNumber is not 0
            xum1541_cleanup(found, "error: cannot query serial number: %s\n",
            usb.error_name(len));
            continue;
        }

        serialnum = 0;
        if (len > 0 && len <= 3) {
            string[len] = '\0';
            serialnum = atoi((char *)string);
        }

        if (PortNumber == serialnum) {
            xum1541_dbg(0, "xum1541 serial number: %3u", serialnum);
            uh->devh = found;
            break;
        }

        // keep in mind the handle, if the device's
        // serial number is less than previous ones
        if(serialnum < leastserial) {
            leastserial = serialnum;
            preferredDefaultHandle = device;
        }
        xum1541_cleanup(found, NULL);
    }

    // if no default device was found because only specific devices were present,
    // determine the default device from the specific ones and open it
    if (NULL == uh->devh && preferredDefaultHandle != NULL) {
        err = usb.open(preferredDefaultHandle, &uh->devh);
        if (LIBUSB_SUCCESS != err)
            fprintf(stderr, "error: Cannot open USB device: %s\n", usb.error_name(err));
    }

    usb.free_device_list(list, 1);
}

// Check for a firmware version compatible with this plugin
static int
xum1541_check_version(int version)
{
    xum1541_dbg(0, "firmware version %d, library version %d", version,
        XUM1541_VERSION);
    if (version < XUM1541_VERSION) {
        fprintf(stderr, "xum1541 firmware version too low (%d < %d)\n",
            version, XUM1541_VERSION);
        fprintf(stderr, "please update your xum1541 firmware\n");
        return -1;
    } else if (version > XUM1541_VERSION) {
        fprintf(stderr, "xum1541 firmware version too high (%d > %d)\n",
            version, XUM1541_VERSION);
        fprintf(stderr, "please update your OpenCBM plugin\n");
        return -1;
    }
    return 0;
}

/*! \brief Query unique identifier for the xum1541 device
  This function tries to find an unique identifier for the xum1541 device.

  \param PortNumber
   The device's serial number to search for also. It is not considered, if set to 0.

  \return
    0 on success, -1 on error. On error, the handle is cleaned up if it
    was already active.

  \remark
    On success, xum1541_handle contains a valid handle to the xum1541 device.
    In this case, the device configuration has been set and the interface
    been claimed. xum1541_close() should be called when the user is done
    with it.
*/
const char *
xum1541_device_path(int PortNumber)
{
    struct xum1541_usb_handle uh;
    static char dev_path[sizeof(XUM1541_PREFIX) + 3 + 1 + 3 + 1];

    usb.init(&uh.ctx);
    xum1541_enumerate(&uh, PortNumber);
    if (uh.devh != NULL) {
        snprintf(dev_path, sizeof(dev_path), XUM1541_PREFIX "%d/%d",
            usb.get_bus_number(usb.get_device(uh.devh)),
            usb.get_device_address(usb.get_device(uh.devh)));
        xum1541_close(&uh);
    } else {
        snprintf(dev_path, sizeof(dev_path), XUM1541_PREFIX);
        fprintf(stderr, "error: no xum1541 device found\n");
    }
    usb.exit(uh.ctx);

    return dev_path;
}

static int
xum1541_clear_halt(libusb_device_handle *devh)
{
    int ret;

    ret = usb.clear_halt(devh, XUM_BULK_IN_ENDPOINT | LIBUSB_ENDPOINT_IN);
    if (ret != 0) {
        fprintf(stderr, "USB clear halt request failed for in ep: %s\n",
            usb.error_name(ret));
        return -1;
    }
    ret = usb.clear_halt(devh, XUM_BULK_OUT_ENDPOINT);
    if (ret != 0) {
        fprintf(stderr, "USB clear halt request failed for out ep: %s\n",
            usb.error_name(ret));
        return -1;
    }

    return 0;
}

/*! \brief Initialize the xum1541 device
  This function tries to find and identify the xum1541 device.

  \param HandleXum1541
   Pointer to a XUM1541_HANDLE which will contain the file handle of the USB device.

  \param PortNumber
   The device's serial number to search for also. It is not considered, if set to 0.

  \return
    0 on success, -1 on error. On error, the handle is cleaned up if it
    was already active.

  \remark
    On success, xum1541_handle contains a valid handle to the xum1541 device.
    In this case, the device configuration has been set and the interface
    been claimed. xum1541_close() should be called when the user is done
    with it.
*/
int
xum1541_init(struct xum1541_usb_handle **uhp, int PortNumber)
{
    unsigned char devInfo[XUM_DEVINFO_SIZE], devStatus;
    int len, ret;
    struct xum1541_usb_handle *uh;
    int interface_claimed = 0;
    int success = 0;

    *uhp = uh = malloc(sizeof(struct xum1541_usb_handle));
    if (NULL == uh) {
        perror("malloc");
        return -1;
    }

    usb.init(&uh->ctx);
    xum1541_enumerate(uh, PortNumber);

    if (uh->devh == NULL) {
        fprintf(stderr, "error: no xum1541 device found\n");
        usb.exit(uh->ctx);
        free(uh);
        return -1;
    }

    do {
        // Select first and only device configuration.
        ret = usb.set_configuration(uh->devh, 1);
        if (ret != LIBUSB_SUCCESS) {
            xum1541_cleanup(uh->devh, "USB error: %s\n", usb.error_name(ret));
            break;
        }

        /*
         * Get exclusive access to interface 0.
         * After this point, do cleanup using xum1541_close() instead of
         * xum1541_cleanup().
         */
        ret = usb.claim_interface(uh->devh, 0);
        if (ret != LIBUSB_SUCCESS) {
            xum1541_cleanup(uh->devh, "USB error: %s\n", usb.error_name(ret));
            break;
        }

        interface_claimed = 1;

        // Check the basic device info message for firmware version
        memset(devInfo, 0, sizeof(devInfo));
        len = usb.control_transfer(uh->devh, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
            XUM1541_INIT, 0, 0, (unsigned char*)devInfo, sizeof(devInfo), USB_TIMEOUT);
        if (len < 2) {
            fprintf(stderr, "USB request for XUM1541 info failed: %s\n",
                usb.error_name(len));
            break;
        }
        if (xum1541_check_version(devInfo[0]) != 0)
            break;

        if (len >= 4)
            xum1541_dbg(0, "device capabilities %02x status %02x", devInfo[1], devInfo[2]);

        // Check for the xum1541's current status. (Not the drive.)
        devStatus = devInfo[2];
        if ((devStatus & XUM1541_DOING_RESET) != 0) {
            fprintf(stderr, "previous command was interrupted, resetting\n");
            // Clear the stalls on both endpoints
            if (xum1541_clear_halt(uh->devh) < 0)
            break;
        }

        success = 1;
    } while(0);

    /* error cleanup */
    if (!success) {
        if (interface_claimed)
            usb.release_interface(uh->devh, 0);

        xum1541_close(uh);
    }

    return success ? 0 : -1;
}

/*! \brief close the xum1541 device

 \param HandleXum1541
   Pointer to a XUM1541_HANDLE which will contain the file handle of the USB device.

 \remark
    This function releases the interface and closes the xum1541 handle.
*/
void
xum1541_close(struct xum1541_usb_handle *uh)
{
    int ret;

    xum1541_dbg(0, "Closing USB link");

    ret = usb.control_transfer(uh->devh, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_OUT,
        XUM1541_SHUTDOWN, 0, 0, NULL, 0, 1000);
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr,
            "USB request for XUM1541 close failed, continuing: %s\n",
            usb.error_name(ret));
    }
    ret = usb.release_interface(uh->devh, 0);
    if (ret != LIBUSB_SUCCESS)
        fprintf(stderr, "USB release intf error: %s\n", usb.error_name(ret));

    usb.close(uh->devh);
    usb.exit(uh->ctx);
    free(uh);
}

/*! \brief  Handle synchronous USB control messages, e.g. for RESET.
    xum1541_ioctl() is used for bulk messages.

 \param HandleXum1541
   A XUM1541_HANDLE which contains the file handle of the USB device.

 \param cmd
   The command to run.

 \return
   Returns the value the USB device sent back.
*/
int
xum1541_control_transfer(struct xum1541_usb_handle *uh, unsigned int cmd)
{
    int nBytes;

    xum1541_dbg(1, "control msg %d", cmd);

    nBytes = usb.control_transfer(uh->devh, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_OUT,
        cmd, 0, 0, NULL, 0, USB_TIMEOUT);
    if (nBytes < 0) {
        fprintf(stderr, "USB error in xum1541_control_transfer: %s\n",
            usb.error_name(nBytes));
        exit(-1);
    }

    return nBytes;
}

static int
xum1541_wait_status(libusb_device_handle *HandleXum1541)
{
    int nBytes, deviceBusy, ret;
    unsigned char statusBuf[XUM_STATUSBUF_SIZE];

    xum1541_dbg(2, "xum1541_wait_status checking for status");
    deviceBusy = 1;
    while (deviceBusy) {
        nBytes = 0;
        ret = usb.bulk_transfer(HandleXum1541,
            XUM_BULK_IN_ENDPOINT | LIBUSB_ENDPOINT_IN,
            (unsigned char*)statusBuf, XUM_STATUSBUF_SIZE, &nBytes, LIBUSB_NO_TIMEOUT);
        if (nBytes == XUM_STATUSBUF_SIZE) {
            switch (XUM_GET_STATUS(statusBuf)) {
            case XUM1541_IO_BUSY:
                xum1541_dbg(2, "device busy, waiting");
                break;
            case XUM1541_IO_ERROR:
                fprintf(stderr, "device reports error\n");
                /* FALLTHROUGH */
            case XUM1541_IO_READY:
                deviceBusy = 0;
                break;
            default:
                fprintf(stderr, "unknown status value: %d\n",
                    XUM_GET_STATUS(statusBuf));
                exit(-1);
            }
        } else {
            fprintf(stderr, "USB error in xum1541_wait_status: %s\n",
                usb.error_name(ret));
            exit(-1);
        }
    }

    // Once we have a valid response (done ok), get extended status
    if (XUM_GET_STATUS(statusBuf) == XUM1541_IO_READY)
        ret = XUM_GET_STATUS_VAL(statusBuf);
    else
        ret = -1;

    xum1541_dbg(2, "return val = %x", ret);
    return ret;
}

/*! \brief Perform an ioctl on the xum1541, which is any command other than
    read/write or special device management commands such as INIT and RESET.

 \param HandleXum1541
   A XUM1541_HANDLE which contains the file handle of the USB device.

 \param cmd
   The command to run.

 \param addr
   The IEC device to use or 0 if not needed.

 \param secaddr
   The IEC secondary address to use or 0 if not needed.

 \return
   Returns the device status byte, which is 0 if the command does not
   have a return value. For some commands, the status byte gives
   info from the device such as the active IEC lines.
*/
int
xum1541_ioctl(struct xum1541_usb_handle *uh, unsigned int cmd, unsigned int addr, unsigned int secaddr)
{
    int nBytes, ret;
    unsigned char cmdBuf[XUM_CMDBUF_SIZE];

    xum1541_dbg(1, "ioctl %d for device %d, sub %d", cmd, addr, secaddr);
    cmdBuf[0] = (unsigned char)cmd;
    cmdBuf[1] = (unsigned char)addr;
    cmdBuf[2] = (unsigned char)secaddr;
    cmdBuf[3] = 0;

    // Send the 4-byte command block
    ret = usb.bulk_transfer(uh->devh,
        XUM_BULK_OUT_ENDPOINT | LIBUSB_ENDPOINT_OUT,
        (unsigned char *)cmdBuf, sizeof(cmdBuf), &nBytes, LIBUSB_NO_TIMEOUT);
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr, "USB error in xum1541_ioctl cmd: %s\n",
            usb.error_name(ret));
        exit(-1);
    }

    // If we have a valid response, return extended status
    ret = xum1541_wait_status(uh->devh);
    xum1541_dbg(2, "return val = %x", ret);
    return ret;
}

/*! \brief Write data to the xum1541 device

 \param HandleXum1541
   A XUM1541_HANDLE which contains the file handle of the USB device.

 \param mode
    Drive protocol to use to read the data from the device (e.g,
    XUM1541_CBM is normal IEC wire protocol).

 \param data
    Pointer to buffer which contains the data to be written to the xum1541

 \param size
    The number of bytes to write to the xum1541

 \return
    The number of bytes actually written, 0 on device error. If there is a
    fatal error, returns -1.
*/
int
xum1541_write(struct xum1541_usb_handle *uh, __u_char modeFlags, const __u_char *data, size_t size)
{
    int wr, mode, ret;
    size_t bytesWritten, bytes2write;
    __u_char cmdBuf[XUM_CMDBUF_SIZE];

    mode = modeFlags & 0xf0;
    xum1541_dbg(1, "write %d %d bytes from address %p flags %x",
        mode, size, data, modeFlags & 0x0f);

    // Send the write command
    cmdBuf[0] = XUM1541_WRITE;
    cmdBuf[1] = modeFlags;
    cmdBuf[2] = size & 0xff;
    cmdBuf[3] = (size >> 8) & 0xff;
    ret = usb.bulk_transfer(uh->devh,
        XUM_BULK_OUT_ENDPOINT | LIBUSB_ENDPOINT_OUT,
        (unsigned char *)cmdBuf, sizeof(cmdBuf), &wr, LIBUSB_NO_TIMEOUT);
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr, "USB error in write cmd: %s\n",
            usb.error_name(ret));
        return -1;
    }

    bytesWritten = 0;
    while (bytesWritten < size) {
        bytes2write = size - bytesWritten;
        if (bytes2write > XUM_MAX_XFER_SIZE)
            bytes2write = XUM_MAX_XFER_SIZE;
        wr = 0;
        ret = usb.bulk_transfer(uh->devh,
            XUM_BULK_OUT_ENDPOINT | LIBUSB_ENDPOINT_OUT,
            (unsigned char *)data, bytes2write, &wr, LIBUSB_NO_TIMEOUT);
        if (ret != LIBUSB_SUCCESS) {
            fprintf(stderr, "USB error in write data: %s\n",
                usb.error_name(ret));
            return -1;
        } else if (wr > 0)
            xum1541_dbg(2, "wrote %d bytes", wr);

        data += wr;
        bytesWritten += wr;

        /*
         * If we wrote less than we requested (or 0), the transfer is done
         * even if we had more data to write still.
         */
        if (wr < (int)bytes2write)
            break;
    }

    // If this is the CBM protocol, wait for the status message.
    if (mode == XUM1541_CBM) {
        ret = xum1541_wait_status(uh->devh);
        if (ret >= 0)
            xum1541_dbg(2, "wait done, extended status %d", ret);
        else
            xum1541_dbg(2, "wait done with error");
        bytesWritten = ret;
    }

    xum1541_dbg(2, "write done, got %d bytes", bytesWritten);
    return bytesWritten;
}

/*! \brief Read data from the xum1541 device

 \param HandleXum1541
   A XUM1541_HANDLE which contains the file handle of the USB device.

 \param mode
    Drive protocol to use to read the data from the device (e.g,
    XUM1541_CBM is normal IEC wire protocol).

 \param data
    Pointer to a buffer which will contain the data read from the xum1541

 \param size
    The number of bytes to read from the xum1541

 \return
    The number of bytes actually read, 0 on device error. If there is a
    fatal error, returns -1.
*/
int
xum1541_read(struct xum1541_usb_handle *uh, __u_char mode, __u_char *data, size_t size)
{
    int rd, ret;
    size_t bytesRead, bytes2read;
    unsigned char cmdBuf[XUM_CMDBUF_SIZE];

    xum1541_dbg(1, "read %d %d bytes to address %p",
               mode, size, data);

    // Send the read command
    cmdBuf[0] = XUM1541_READ;
    cmdBuf[1] = mode;
    cmdBuf[2] = size & 0xff;
    cmdBuf[3] = (size >> 8) & 0xff;
    ret = usb.bulk_transfer(uh->devh,
        XUM_BULK_OUT_ENDPOINT | LIBUSB_ENDPOINT_OUT,
        (unsigned char *)cmdBuf, sizeof(cmdBuf), &rd, LIBUSB_NO_TIMEOUT);
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr, "USB error in read cmd: %s\n",
            usb.error_name(ret));
        return -1;
    }

    // Read the actual data now that it's ready.
    bytesRead = 0;
    while (bytesRead < size) {
        bytes2read = size - bytesRead;
        if (bytes2read > XUM_MAX_XFER_SIZE)
            bytes2read = XUM_MAX_XFER_SIZE;
        ret = usb.bulk_transfer(uh->devh,
            XUM_BULK_IN_ENDPOINT | LIBUSB_ENDPOINT_IN,
            (unsigned char *)data, bytes2read, &rd, LIBUSB_NO_TIMEOUT);
        if (ret != LIBUSB_SUCCESS) {
            fprintf(stderr, "USB error in read data(%p, %d): %s\n",
               data, (int)size, usb.error_name(ret));
            return -1;
        } else if (rd > 0)
            xum1541_dbg(2, "read %d bytes", rd);

        data += rd;
        bytesRead += rd;

        /*
         * If we read less than we requested (or 0), the transfer is done
         * even if we had more data to read still.
         */
        if (rd < (int)bytes2read)
            break;
    }

    xum1541_dbg(2, "read done, got %d bytes", bytesRead);
    return bytesRead;
}
