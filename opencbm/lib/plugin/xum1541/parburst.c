/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *  Copyright 2009-2010 Nate Lawson
 */

/*! ************************************************************** 
** \file lib/plugin/xum1541/WINDOWS/parburst.c \n
** \author Nate Lawson \n
** \version $Id: parburst.c,v 1.7 2010-09-05 00:24:30 natelawson Exp $ \n
** \n
** \brief Shared library / DLL for accessing the mnib driver functions, windows specific code
**
****************************************************************/

#include <stdio.h>
#include <stdlib.h>

#define DBG_USERMODE
#define DBG_PROGNAME "OPENCBM-XUM1541.DLL"
#include "debug.h"

#define OPENCBM_PLUGIN
#include "archlib.h"

#include "xum1541.h"


/*! \brief PARBURST: Read from the parallel port

 This function is a helper function for parallel burst:
 It reads from the parallel port.

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \return
   The value read from the parallel port

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

__u_char CBMAPIDECL
opencbm_plugin_parallel_burst_read(CBM_FILE HandleDevice)
{
    __u_char result;

    result = (__u_char)xum1541_ioctl((struct xum1541_usb_handle *)HandleDevice, XUM1541_PARBURST_READ, 0, 0);
    //printf("parburst read: %x\n", result);
    return result;
}

/*! \brief PARBURST: Write to the parallel port

 This function is a helper function for parallel burst:
 It writes to the parallel port.

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \param Value
   The value to be written to the parallel port

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

void CBMAPIDECL
opencbm_plugin_parallel_burst_write(CBM_FILE HandleDevice, __u_char Value)
{
    int result;

    result = xum1541_ioctl((struct xum1541_usb_handle *)HandleDevice, XUM1541_PARBURST_WRITE, Value, 0);
    //printf("parburst write: %x, res %x\n", Value, result);
}

int CBMAPIDECL
opencbm_plugin_parallel_burst_read_n(CBM_FILE HandleDevice, __u_char *Buffer,
    unsigned int Length)
{
    int result;

    result = xum1541_read((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB_COMMAND, Buffer, Length);
    if (result != Length) {
        DBG_WARN((DBG_PREFIX "parallel_burst_read_n: returned with error %d", result));
    }

    return result;
}

int CBMAPIDECL
opencbm_plugin_parallel_burst_write_n(CBM_FILE HandleDevice, __u_char *Buffer,
    unsigned int Length)
{
    int result;

    result = xum1541_write((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB_COMMAND, Buffer, Length);
    if (result != Length) {
        DBG_WARN((DBG_PREFIX "parallel_burst_write_n: returned with error %d", result));
    }

    return result;
}

/*! \brief PARBURST: Read a complete track

 This function is a helper function for parallel burst:
 It reads a complete track from the disk

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \param Buffer
   Pointer to a buffer which will hold the bytes read.

 \param Length
   The length of the Buffer.

 \return
   != 0 on success.

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

int CBMAPIDECL
opencbm_plugin_parallel_burst_read_track(CBM_FILE HandleDevice, __u_char *Buffer, unsigned int Length)
{
    int result;

    result = xum1541_read((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB, Buffer, Length);
    if (result != Length) {
        DBG_WARN((DBG_PREFIX "parallel_burst_read_track: returned with error %d", result));
    }

    return result;
}

/*! \brief PARBURST: Read a variable length track

 This function is a helper function for parallel burst:
 It reads a variable length track from the disk

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \param Buffer
   Pointer to a buffer which will hold the bytes read.

 \param Length
   The length of the Buffer.

 \return
   != 0 on success.

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

int CBMAPIDECL
opencbm_plugin_parallel_burst_read_track_var(CBM_FILE HandleDevice, __u_char *Buffer, unsigned int Length)
{
    int result;

    // Add a flag to indicate this read terminates early after seeing 
    // an 0x55 byte.
    result = xum1541_read((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB, Buffer, Length | XUM1541_NIB_READ_VAR);
    if (result <= 0) {
        DBG_WARN((DBG_PREFIX "parallel_burst_read_track_var: returned with error %d", result));
    }

    return result;
}

/*! \brief PARBURST: Write a complete track

 This function is a helper function for parallel burst:
 It writes a complete track to the disk

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \param Buffer
   Pointer to a buffer which hold the bytes to be written.

 \param Length
   The length of the Buffer.

 \return
   != 0 on success.

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

int CBMAPIDECL
opencbm_plugin_parallel_burst_write_track(CBM_FILE HandleDevice, __u_char *Buffer, unsigned int Length)
{
    int result;

    result = xum1541_write((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB, Buffer, Length);
    if (result != Length) {
        DBG_WARN((DBG_PREFIX "parallel_burst_write_track: returned with error %d", result));
    }

    return result;
}

/********* Fast serial nibbler routines below ********/

/*! \brief SRQBURST: Read from the fast serial port

 This function is a helper function for fast serial burst:
 It reads from the fast serial port.

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \return
   The value read from the fast serial port

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

__u_char CBMAPIDECL
opencbm_plugin_srq_burst_read(CBM_FILE HandleDevice)
{
    __u_char result;

    result = (__u_char)xum1541_ioctl((struct xum1541_usb_handle *)HandleDevice, XUM1541_SRQBURST_READ, 0, 0);
    return result;
}

/*! \brief SRQBURST: Write to the fast serial port

 This function is a helper function for fast serial burst:
 It writes to the fast serial port.

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \param Value
   The value to be written to the fast serial port

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

void CBMAPIDECL
opencbm_plugin_srq_burst_write(CBM_FILE HandleDevice, __u_char Value)
{
    int result;

    result = xum1541_ioctl((struct xum1541_usb_handle *)HandleDevice, XUM1541_SRQBURST_WRITE, Value, 0);
}

int CBMAPIDECL
opencbm_plugin_srq_burst_read_n(CBM_FILE HandleDevice, __u_char *Buffer,
    unsigned int Length)
{
    int result;

    result = xum1541_read((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB_SRQ_COMMAND, Buffer, Length);
    if (result != Length) {
        DBG_WARN((DBG_PREFIX "srq_burst_read_n: returned with error %d", result));
    }

    return result;
}

int CBMAPIDECL
opencbm_plugin_srq_burst_write_n(CBM_FILE HandleDevice, __u_char *Buffer,
    unsigned int Length)
{
    int result;

    result = xum1541_write((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB_SRQ_COMMAND, Buffer, Length);
    if (result != Length) {
        DBG_WARN((DBG_PREFIX "srq_burst_write_n: returned with error %d", result));
    }

    return result;
}

/*! \brief SRQ: Read a complete track

 This function is a helper function for fast serial burst:
 It reads a complete track from the disk

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \param Buffer
   Pointer to a buffer which will hold the bytes read.

 \param Length
   The length of the Buffer.

 \return
   != 0 on success.

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

int CBMAPIDECL
opencbm_plugin_srq_burst_read_track(CBM_FILE HandleDevice, __u_char *Buffer, unsigned int Length)
{
    int result;

    result = xum1541_read((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB_SRQ, Buffer, Length);
    if (result != Length) {
        DBG_WARN((DBG_PREFIX "srq_read_track: returned with error %d", result));
    }

    return result;
}

/*! \brief SRQ: Write a complete track

 This function is a helper function for fast serial burst:
 It writes a complete track to the disk

 \param HandleDevice
   A CBM_FILE which contains the file handle of the driver.

 \param Buffer
   Pointer to a buffer which hold the bytes to be written.

 \param Length
   The length of the Buffer.

 \return
   != 0 on success.

 If cbm_driver_open() did not succeed, it is illegal to 
 call this function.
*/

int CBMAPIDECL
opencbm_plugin_srq_burst_write_track(CBM_FILE HandleDevice, __u_char *Buffer, unsigned int Length)
{
    int result;

    result = xum1541_write((struct xum1541_usb_handle *)HandleDevice, XUM1541_NIB_SRQ, Buffer, Length);
    if (result != Length) {
        DBG_WARN((DBG_PREFIX "srq_write_track: returned with error %d", result));
    }

    return result;
}
