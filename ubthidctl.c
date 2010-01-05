/*-
 * Copyright (c) 2007 Rui Paulo <rpaulo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Based on bluez-utils hid2hci tool written by
 * Marcel Holtmann <marcel@holtmann.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usbdi.h>

typedef enum {
	HCI = 0,
	HID = 1
} hidmode_t;

typedef struct {
	hidmode_t mode;
	uint16_t  vendor;
	uint16_t  product;
} tblentry_t;

typedef struct {
	int devno;
	int devaddr;
} devinfo_t;


static void 	switchmode(const char *dev, const int devaddr, hidmode_t mode);


static devinfo_t
finddevice(tblentry_t tblentry)
{
	struct usb_device_info usbdev;
	char filename[16];
	devinfo_t dev;
	int i;
	int j;
	int fd;

	dev.devno = -1;
	
	for (i = 0; i < 20; i++) {
		snprintf(filename, sizeof(filename) - 1, "/dev/ugen%d.1", i); 
		fd = open(filename, O_RDONLY);
		if (fd < 0)
			return dev;

		for (j = USB_START_ADDR; j < USB_MAX_DEVICES; j++) {
			memset(&usbdev, 0, sizeof(usbdev));
			usbdev.udi_addr = j;
			ioctl(fd, USB_DEVICEINFO, &usbdev);
			if (tblentry.vendor == usbdev.udi_vendorNo &&
			    tblentry.product == usbdev.udi_productNo) {
				dev.devno = i;
				dev.devaddr = j;
				close(fd);
				
				return dev;
			}
		}

		close(fd);
	}

	return dev;
}

static void
parsetable(const char *filename)
{
	FILE *fp;
	tblentry_t tblentry;
	unsigned int vendor;
	unsigned int product;
	char modestr[4];
	devinfo_t dev;
	char usbname[16];
	char buf[1024];
	char *p;
	
	fp = fopen(filename, "r");
	if (!fp)
		err(1, "fopen %s", filename);

	while (!feof(fp)) {
		fgets(buf, sizeof(buf) - 1, fp);
		for (p = buf; *p == ' '; p++);
		if (*p == '#' || *p == '\n')
			continue;

		sscanf(p, "%3s 0x%x 0x%x\n", modestr, &vendor, &product);

		tblentry.vendor = vendor;
		tblentry.product = product;
		
		if (!strncmp(modestr, "HCI", 3))
			tblentry.mode = HCI;
		else if (!strncmp(modestr, "HID", 3))
			tblentry.mode = HID;
		else {
			warnx("invalid mode: %s", modestr);
			continue;
		}
		dev = finddevice(tblentry);
		if (dev.devno != -1) {
			snprintf(usbname, sizeof(usbname) - 1, "/dev/usb%d",
			    dev.devno);
			switchmode(usbname, dev.devaddr, tblentry.mode);
		}

	}
	
	fclose(fp);

}

static void
switchmode(const char *dev, const int devaddr, hidmode_t mode)
{
	int fd;
	struct usb_ctl_request req;

	fd = open(dev, O_RDWR, 0);
	if (fd < 0)
		err(1, "open %s", dev);

	memset(&req, 0, sizeof(req));
	req.ucr_addr = devaddr;
	USETW(req.ucr_request.wValue, mode);
	USETW(req.ucr_request.wIndex, 0);
	USETW(req.ucr_request.wLength, 0);
	req.ucr_data = NULL;
	req.ucr_flags = USB_SHORT_XFER_OK;
	req.ucr_request.bmRequestType = UT_VENDOR;
	req.ucr_request.bRequest = 0;

	ioctl(fd, USB_REQUEST, &req);

	/*
	 * The return value of ioctl() will always be EIO, so it's up
	 * to the user to check whether the device was switched sucessfuly.
	 */
	 
	close(fd);
}

static void
usage(void)
{
	fprintf(stderr, "usage:\t%s -f device -a addr [-m mode]\n"
	    "\t%s -t tablefile\n", getprogname(), getprogname());

	exit(1);
}

int
main(int argc, char *argv[])
{
	char ch;
	char *dev;
	char *tablefile;
	int devaddr;
	int mode;

	dev       = NULL;
	devaddr   = -1;
	mode      = HCI;
	tablefile = NULL;
	
	while ((ch = getopt(argc, argv, "f:m:a:t:?")) != -1)
		switch (ch) {
		case 'f':
			dev = optarg;
			break;
		case 'm':
			mode = atoi(optarg);
			if (mode < 0 || mode > 1)
				usage();
			break;
		case 'a':
			devaddr = atoi(optarg);
			if (devaddr < 0)
				usage();
			break;
		case 't':
			tablefile = optarg;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;

	if (tablefile) {
		parsetable(tablefile);
		return 0;
	}

	if (dev == NULL || devaddr == -1)
		usage();
	
	switchmode(dev, devaddr, mode);

	return 0;
}
