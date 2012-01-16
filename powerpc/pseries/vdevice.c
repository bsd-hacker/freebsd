/*-
 * Copyright (c) 2011 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

static int	vdevice_probe(device_t);
static int	vdevice_attach(device_t);
static const struct ofw_bus_devinfo *vdevice_get_devinfo(device_t dev,
    device_t child);

static device_method_t vdevice_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vdevice_probe),
	DEVMETHOD(device_attach,	vdevice_attach),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_child_pnpinfo_str, ofw_bus_gen_child_pnpinfo_str),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	vdevice_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

static driver_t vdevice_driver = {
	"vdevice",
	vdevice_methods,
	0
};

static devclass_t vdevice_devclass;

DRIVER_MODULE(vdevice, nexus, vdevice_driver, vdevice_devclass, 0, 0);

static int 
vdevice_probe(device_t dev) 
{
	const char	*name;

	name = ofw_bus_get_name(dev);

	if (name == NULL || strcmp(name, "vdevice") != 0)
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "IBM,vdevice"))
		return (ENXIO);

	device_set_desc(dev, "POWER Hypervisor Virtual Device Root");

	return (0);
}

static int 
vdevice_attach(device_t dev) 
{
	phandle_t root, child;
	device_t cdev;
	struct ofw_bus_devinfo *dinfo;

	root = ofw_bus_get_node(dev);

	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		dinfo = malloc(sizeof(*dinfo), M_DEVBUF, M_WAITOK | M_ZERO);

                if (ofw_bus_gen_setup_devinfo(dinfo, child) != 0) {
                        free(dinfo, M_DEVBUF);
                        continue;
                }
                cdev = device_add_child(dev, NULL, -1);
                if (cdev == NULL) {
                        device_printf(dev, "<%s>: device_add_child failed\n",
                            dinfo->obd_name);
                        ofw_bus_gen_destroy_devinfo(dinfo);
                        free(dinfo, M_DEVBUF);
                        continue;
                }
		device_set_ivars(cdev, dinfo);
	}

	return (bus_generic_attach(dev));
}

static const struct ofw_bus_devinfo *
vdevice_get_devinfo(device_t dev, device_t child) 
{
	return (device_get_ivars(child));	
}

