/*
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *  Copyright 2007 Till Harbaum <till@harbaum.org>
 *  Copyright 2009 Spiro Trikaliotis
 *
*/

/*! ************************************************************** 
** \file lib/plugin/xu1541/xu1541.c \n
** \author Till Harbaum \n
** \version $Id: xu1541.c,v 1.8 2010-02-20 20:50:26 strik Exp $ \n
** \n
** \brief libusb based xu1541 access routines
**
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
#include "xu1541.h"

static int debug_level = -10000; /*!< \internal \brief the debugging level for debugging output */

/*! \brief timeout value, used mainly after errors \todo What is the exact purpose of this? */
#define TIMEOUT_DELAY  25000   // 25ms

/*! \internal \brief Output debugging information for the xu1541

 \param level
   The output level; output will only be produced if this level is less or equal the debugging level

 \param msg
   The printf() style message to be output
*/
static void xu1541_dbg(int level, char *msg, ...) 
{
    va_list argp;
    
    /* determine debug mode if not yet known */
    if(debug_level == -10000)
    {
       char *val = getenv("XU1541_DEBUG");
       if(val)
	   debug_level = atoi(val);
    }

    if(level <= debug_level) 
    {
        fprintf(stderr, "[XU1541] ");
        va_start(argp, msg);
	vfprintf(stderr, msg, argp);
	va_end(argp);
	fprintf(stderr, "\n");
    }
}

/*! \brief initialise the xu1541 device

  This function tries to find and identify the xu1541 device.

  \return
    0 on success, -1 on error.

  \remark
    On success, xu1541_handle contains a valid handle to the xu1541 device.
    In this case, the device configuration has been set and the interface
    been claimed.

  \bug
    On some error types, this function might return error, but might
    has opened the xu1541_handle. In this case, the handle is leaked, as
    xu1541_close() is not to be called.
*/
/* try to find a xu1541 cable */
int xu1541_init(struct xu1541_usb_handle **uhp)
{
  struct xu1541_usb_handle *uh;
  libusb_device_handle *xu1541_handle = NULL;
  int len;
  libusb_device **list;
  struct libusb_device_descriptor descriptor;
  ssize_t cnt;
  ssize_t i = 0;
  int err = 0;
  unsigned char version[4], string[256];
  int interface_claimed = 0;
  int success = 0;

  xu1541_dbg(0, "Scanning usb ...");

  *uhp = uh = malloc(sizeof(struct xu1541_usb_handle));
  if (NULL == uh) {
    perror("malloc");
    return -1;
  }

  usb.init(&uh->ctx);

  // discover devices
  uh->devh = NULL;

  cnt = usb.get_device_list(uh->ctx, &list);
  if (cnt < 0) {
    xu1541_dbg(0, "enumeration error: %s", usb.error_name(cnt));
    usb.exit(uh->ctx);
    free(uh);
    return -1;
  }

  for (i = 0; i < cnt; i++) {
    libusb_device *device = list[i];

    if (LIBUSB_SUCCESS != usb.get_device_descriptor(device, &descriptor))
      continue;

    xu1541_dbg(1, "Device %04x:%04x", descriptor.idVendor, descriptor.idProduct);

    // First, find our vendor and product id
    if (descriptor.idVendor != XU1541_VID || descriptor.idProduct != XU1541_PID)
      continue;

    xu1541_dbg(0, "Found xu1541 device on bus %s device %s.",
      usb.get_bus_number(device), usb.get_device_address(device));

    /* open device */
    err = usb.open(device, &xu1541_handle);
    if (LIBUSB_SUCCESS != err) {
      fprintf(stderr, "error: Cannot open USB device: %s\n",
        usb.error_name(err));
      continue;
    }

    /* get device name and make sure the name is "xu1541" meaning */
    /* that the device is not in boot loader mode */
    len = usb.get_string_descriptor_ascii(xu1541_handle, descriptor.iProduct,
                            string, sizeof(string) - 1);
    if (len < 0) {
      fprintf(stderr, "warning: cannot query product "
              "name for device: %s\n", usb.error_name(len));
      if (xu1541_handle)
        usb.close(xu1541_handle);
      xu1541_handle = NULL;
    }

    /* make sure the name matches what we expect */
    if (strcmp((char*)string, "xu1541") != 0) {
      fprintf(stderr, "Error: Found xu1541 in unexpected state,"
              " please make sure device is _not_ in bootloader mode!\n");

      if (xu1541_handle)
        usb.close(xu1541_handle);
      xu1541_handle = NULL;
    }
  }

  uh->devh = xu1541_handle;

  if (uh->devh == NULL) {
	fprintf(stderr, "error: no xu1541 device found\n");
	usb.exit(uh->ctx);
	free(uh);
	return -1;
  }

do {
    // Select first and only device configuration.
    err = usb.set_configuration(uh->devh, 1);
    if (err != LIBUSB_SUCCESS) {
        fprintf(stderr, "USB error: %s\n", usb.error_name(err));
        break;
    }
      
    /* Get exclusive access to interface 0. */
    err = usb.claim_interface(uh->devh, 0);
    if (err != LIBUSB_SUCCESS) {
        fprintf(stderr, "USB error: %s\n", usb.error_name(err));
        break;
    }

    interface_claimed = 1;

    /* check the devices version number as firmware x.06 changed everything */
    len = usb.control_transfer(uh->devh,
           LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
           XU1541_INFO, 0, 0, version, sizeof(version), 1000);

    if (len < 0) {
      fprintf(stderr, "USB request for XU1541 info failed: %s!\n",
        usb.error_name(len));
      break;
    }

    if (len != sizeof(version)) {
      fprintf(stderr, "Unexpected number of bytes (%d) returned\n", len);
      break;
    }

    xu1541_dbg(0, "firmware version %x.%02x", version[0], version[1]);

    if (version[1] < 8) {
      fprintf(stderr, "Device reports firmware version %x.%02x\n", 
  	    version[0], version[1]);
      fprintf(stderr, "but this version of opencbm requires at least "
  	    "version x.08\n");
      break;
    }
    success = 1;
  } while (0);

  /* error cleanup */
  if (!success) {
    if (interface_claimed) usb.release_interface(uh->devh, 0);

    xu1541_close(uh);
  }

  return success ? 0 : -1;
}

/*! \brief close the xu1541 device

 \remark
    This function releases the interface and closes the xu1541 handle.
*/
void xu1541_close(struct xu1541_usb_handle *uh)
{
  int ret;

  xu1541_dbg(0, "Closing USB link");

  ret = usb.release_interface(uh->devh, 0);
  if (ret != LIBUSB_SUCCESS)
    fprintf(stderr, "USB release intf error: %s\n", usb.error_name(ret));

  usb.close(uh->devh);
  usb.exit(uh->ctx);
  free(uh);
}

/*! \brief perform an ioctl on the xu1541

 \param cmd
   The IOCTL number

 \param addr
   The (IEC) device to use

 \param secaddr
   The (IEC) secondary address to use

 \return
   Depends upon the IOCTL.

 \todo
   Rework for cleaner structure. Currently, this is a mess!
*/
int xu1541_ioctl(struct xu1541_usb_handle *uh, unsigned int cmd, unsigned int addr, unsigned int secaddr)
{
  int nBytes;
  unsigned char ret[4];

  xu1541_dbg(1, "ioctl %d for device %d, sub %d", cmd, addr, secaddr);

  /* some commands are being handled asynchronously, namely the ones that */
  /* send a iec byte. These need to ask for the result with a seperate */
  /* command */
  if((cmd == XU1541_TALK)   || (cmd == XU1541_UNTALK) ||
     (cmd == XU1541_LISTEN) || (cmd == XU1541_UNLISTEN) ||
     (cmd == XU1541_OPEN)   || (cmd == XU1541_CLOSE)) 
  {
      int link_ok = 0, err = 0;

      /* USB_TIMEOUT msec timeout required for reset */
      if ((nBytes = usb.control_transfer(uh->devh,
				   LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
				   cmd, (secaddr << 8) + addr, 0, 
				   NULL, 0, 
				   1000)) < 0) 
      {
	  fprintf(stderr, "USB error in xu1541_ioctl(async): %s\n", 
		  usb.error_name(nBytes));
	  exit(-1);
	  return -1;
      }

      /* wait for USB to become available again by requesting the result */
      do 
      {
	  unsigned char rv[2];
	  
	  /* request async result code */
	  if (usb.control_transfer(uh->devh,
			     LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
			     XU1541_GET_RESULT, 0, 0, 
			     rv, sizeof(rv),
			     1000) == sizeof(rv)) 
	  {
	      /* we got a valid result */
	      if(rv[0] == XU1541_IO_RESULT) 
	      {
		  /* use that result */
		  nBytes = sizeof(rv)-1;
		  ret[0] = rv[1];
		  /* a returned byte means that the USB link is fine */
		  link_ok = 1;
		  errno = 0;
	      } 
	      else 
	      {
		  xu1541_dbg(3, "unexpected result (%d/%d)", rv[0], rv[1]);

		  /* not the expected result */
		  arch_usleep(TIMEOUT_DELAY);
	      }
	  } 
	  else 
	  {
	      xu1541_dbg(3, "usb timeout");

	      /* count the error states (just out of couriosity) */
	      err++;

	      arch_usleep(TIMEOUT_DELAY);
	  }
      } 
      while(!link_ok);
  } 
  else 
  {

      /* sync transfer, read result directly */
      if ((nBytes = usb.control_transfer(uh->devh,
		   LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
		   cmd, (secaddr << 8) + addr, 0, 
		   ret, sizeof(ret), 
		   USB_TIMEOUT)) < 0) 
      {
	  fprintf(stderr, "USB error in xu1541_ioctl(sync): %s\n", 
		  usb.error_name(nBytes));
	  exit(-1);
	  return -1;
      }
  }

  xu1541_dbg(2, "returned %d bytes", nBytes);

  /* return ok(0) if command does not have a return value */
  if(nBytes == 0) 
      return 0;

  xu1541_dbg(2, "return val = %x", ret[0]);
  return ret[0];
}

/*! \brief write data to the xu1541 device

 \param data
    Pointer to buffer which contains the data to be written to the xu1541

 \param len
    The length of the data buffer to be written to the xu1541

 \return
    The number of bytes written
*/
int xu1541_write(struct xu1541_usb_handle *uh, const __u_char *data, size_t len)
{
    int bytesWritten = 0;

    xu1541_dbg(1, "write %d bytes from address %p", len, data);

    while(len) 
    {
        int link_ok = 0, err = 0;
        int wr, bytes2write;
	bytes2write = (len > XU1541_IO_BUFFER_SIZE)?XU1541_IO_BUFFER_SIZE:len;
	
	/* the write itself moved the data into the buffer, the actual */
	/* iec write is triggered _after_ this USB write is done */
	if ((wr = usb.control_transfer(uh->devh,
				 LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_OUT,
				 XU1541_WRITE, bytes2write, 0, 
				 (unsigned char*)data, bytes2write,
				 USB_TIMEOUT)) < 0) 
	{
	    fprintf(stderr, "USB error xu1541_write(): %s\n", usb.error_name(wr));
	    exit(-1);
	    return -1;
	}
	
	len -= wr;
	data += wr;
	bytesWritten += wr;

	xu1541_dbg(2, "wrote chunk of %d bytes, total %d, left %d", 
		   wr, bytesWritten, len);
 
	/* wait for USB to become available again by requesting the result */
	do 
	{
	    unsigned char rv[2];

	    /* request async result */
	    if (usb.control_transfer(uh->devh,
			       LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
			       XU1541_GET_RESULT, 0, 0, 
			       rv, sizeof(rv),
			       1000) == sizeof(rv)) 
	    {
	        /* the USB link is available again if we got a valid result */
	        if(rv[0] == XU1541_IO_RESULT) {
		    /* device reports failure, stop writing */
		    if(!rv[1])
		        len = 0;

		    link_ok = 1;
		    errno = 0;
		} 
		else
		{
		    xu1541_dbg(3, "unexpected result (%d/%d)", rv[0], rv[1]);
		    arch_usleep(TIMEOUT_DELAY);
		}
	    } 
	    else 
	    {
	        xu1541_dbg(3, "usb timeout");
	        /* count the error states (just out of couriosity) */
	        err++;
	    }
	} 
	while(!link_ok);
    }
    return bytesWritten;
}

/*! \brief read data from the xu1541 device

 \param data
    Pointer to a buffer which will contain the data read from the xu1541

 \param len
    The number of bytes to read from the xu1541

 \return
    The number of bytes read
*/
int xu1541_read(struct xu1541_usb_handle *uh, __u_char *data, size_t len)
{
    int bytesRead = 0;
    
    xu1541_dbg(1, "read %d bytes to address %p", len, data);
    
    while(len > 0) 
    {
	int rd, bytes2read;
	int link_ok = 0, err = 0;
	unsigned char rv[2];
	  
	/* limit transfer size */
	bytes2read = (len > XU1541_IO_BUFFER_SIZE)?XU1541_IO_BUFFER_SIZE:len;

	/* request async read, ignore errors as they happen due to */
	/* link being disabled */
	rd = usb.control_transfer(uh->devh,
			LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
			XU1541_REQUEST_READ, bytes2read, 0, 
			NULL, 0,
			1000);
	
	if(rd < 0) {
	    fprintf(stderr, "USB error in xu1541_request_read(): %s\n", 
		    usb.error_name(rd));
	    exit(-1);
	    return -1;
	}

	/* since the xu1541 may disable interrupts and wouldn't be able */
	/* to do proper USB communication we'd have to expect USB errors */

	/* try to get result, to make sure usb link is working again */
	/* we can't do this in the read itself since windows returns */
	/* just 0 bytes while the USB link is down which we can't */
	/* distinguish from a real 0 byte read event */

	xu1541_dbg(2, "sent request for %d bytes, waiting for result", 
		   bytes2read);

	do 
	{
	    /* get the result code which also contains the current state */
	    /* the xu1541 is in so we know when it's done reading on IEC */
	    if ((rd = usb.control_transfer(uh->devh,
				     LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
				     XU1541_GET_RESULT, 0, 0, 
				     rv, sizeof(rv),
				     1000)) == sizeof(rv)) 
	    {
	        xu1541_dbg(2, "got result %d/%d", rv[0], rv[1]);
	      
		// if the first byte is still not XU1541_IO_READ_DONE,
	        // then the device hasn't even entered the copy routine yet, 
	        // so sleep to not slow it down by overloading it with USB 
	        // requests
	        if(rv[0] != XU1541_IO_READ_DONE) 
		{
		    xu1541_dbg(3, "unexpected result");
		    arch_usleep(TIMEOUT_DELAY);
		} 
		else
		{
		    xu1541_dbg(3, "link ok");

	            link_ok = 1;
		    errno = 0;
		}
	    } 
	    else 
	    {
		xu1541_dbg(3, "usb timeout");

		/* count the error states (just out of couriosity) */
		err++;
	    }
	} 
	while(!link_ok);
	
	/* finally read data itself */
	if ((rd = usb.control_transfer(uh->devh,
				 LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
				 XU1541_READ, bytes2read, 0, 
				 data, bytes2read, 1000)) < 0)
	{
	    fprintf(stderr, "USB error in xu1541_read(): %s\n", 
		    usb.error_name(rd));
	    return -1;
	}
	
	len -= rd;
	data += rd;
	bytesRead += rd;

	xu1541_dbg(2, "received chunk of %d bytes, total %d, left %d", 
		   rd, bytesRead, len);
	
	/* force end of read */
	if(rd < bytes2read) 
	    len = 0;
    }
    return bytesRead;
}

/*! \brief "special" write data to the xu1541 device

 \todo
    What is so special?

 \param mode
    \todo ???

 \param data
    Pointer to buffer which contains the data to be written to the xu1541

 \param size
    The length of the data buffer to be written to the xu1541

 \return
    The number of bytes written

 \remark
     current all special mode are able to work asynchronously. this means
     that we can just handle them in the device at the same time as the USB
     transfers.
*/
int xu1541_special_write(struct xu1541_usb_handle *uh, int mode, const __u_char *data, size_t size)
{
    int bytesWritten = 0;

    xu1541_dbg(1, "special write %d %d bytes from address %p", 
	       mode, size, data);

    while(size > 0) 
    {
	int wr, bytes2write = (size>128)?128:size;

	if ((wr = usb.control_transfer(uh->devh,
				 LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_OUT,
				 mode, XU1541_WRITE, bytes2write, 
				 (unsigned char*)data, bytes2write, 1000)) < 0)
	{
	    fprintf(stderr, "USB error in xu1541_special_write(): %s\n", 
		    usb.error_name(wr));
	    return -1;
	}
	
	xu1541_dbg(2, "special wrote %d bytes", wr);

	size -= wr;
	data += wr;
	bytesWritten += wr;
    }

    return bytesWritten;
}

/*! \brief "special" read data from the xu1541 device

 \todo
    What is so special?

 \param mode
    \todo ???

 \param data
    Pointer to a buffer which will contain the data read from the xu1541

 \param size
    The number of bytes to read from the xu1541

 \return
    The number of bytes read
*/
int xu1541_special_read(struct xu1541_usb_handle *uh, int mode, __u_char *data, size_t size)
{
    int bytesRead = 0;

    xu1541_dbg(1, "special read %d %d bytes to address %p", 
	       mode, size, data);

    while(size > 0) 
    {
	int rd, bytes2read = (size>128)?128:size;
	
	if ((rd = usb.control_transfer(uh->devh,
				 LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN,
				 mode, XU1541_READ, bytes2read, 
				 data, bytes2read,
				 USB_TIMEOUT)) < 0) 
	{
	    fprintf(stderr, "USB error in xu1541_special_read(): %s\n", 
		    usb.error_name(rd));
	    return -1;
	}
	
	xu1541_dbg(2, "special read %d bytes", rd);

	size -= rd;
	data += rd;
	bytesRead += rd;

	/* stop if there's nothing more to read */
	if(rd < bytes2read) 
	  size = 0;
    }
    
    return bytesRead;
}
