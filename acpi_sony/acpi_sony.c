/*-
 * Copyright (c) 2004 Takanori Watanabe
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
__FBSDID("$FreeBSD: src/sys/dev/acpi_support/acpi_sony.c,v 1.10 2006/11/01 03:45:24 kevlo Exp $");

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/stdint.h>
#include <sys/proc.h>

#include <contrib/dev/acpica/acpi.h>
#include "acpi_if.h"
#include <dev/acpica/acpivar.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/pci_cfgreg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "acpi_sony.h"

/*
 * Declarations
 */
#define _COMPONENT	ACPI_OEM

ACPI_MODULE_NAME("Sony")
ACPI_SERIAL_DECL(sony, "Sony extras");

static int	acpi_sony_probe(device_t dev);
static int	acpi_sony_attach(device_t dev);
static int 	acpi_sony_detach(device_t dev);
static int	acpi_snc_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_spic_sysctl(SYSCTL_HANDLER_ARGS);
static void	acpi_snc_notify_handler(ACPI_HANDLE h, uint32_t notify, void *context);
static void	acpi_spic_intr(void * arg);
/*
static d_open_t		acpi_sony_spic_open;
static d_close_t		acpi_sony_spic_close;
static d_read_t		acpi_sony_spic_read;
static d_ioctl_t		acpi_sony_spic_ioctl;
static d_poll_t		acpi_sony_spic_poll;

static struct cdevsw spic_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	acpi_sony_spic_open,
	.d_close =	acpi_sony_spic_close,
	.d_read =	acpi_sony_spic_read,
	.d_ioctl =	acpi_sony_spic_ioctl,
	.d_poll =	acpi_sony_spic_poll,
	.d_name =	"spic",
};
*/

static device_method_t acpi_sony_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, acpi_sony_probe),
	DEVMETHOD(device_attach, acpi_sony_attach),
	DEVMETHOD(device_detach, acpi_sony_detach),

	{0, 0}
};

static driver_t	acpi_sony_driver = {
	"acpi_sony",
	acpi_sony_methods,
	sizeof(struct acpi_sony_softc),
};

static devclass_t	acpi_sony_devclass;

DRIVER_MODULE(acpi_sony, acpi, acpi_sony_driver, acpi_sony_devclass, 0, 0);
MODULE_DEPEND(acpi_sony, acpi, 1, 1, 1);

static char	*sony_snc_id[] = {"SNY5001", NULL};
static char	*sony_spic_id[] = {"SNY6001", NULL};

/*
 * SPIC related functions
 */

/* acpi_spic_type()	- returns type of SPIC.
 * in:
 *	dev	- SPIC device_t structure
 * out:
 *	one of SPIC_TYPE constants
 */
static enum SPIC_TYPE
acpi_spic_type(device_t dev)
{
	enum SPIC_TYPE			model = SPIC_TYPE2;
    	const struct spic_pciid	*p = spic_pciids;
	device_t				device;
    
	while (p->vid != 0) {
		device = pci_find_device(p->vid, p->did);
		
		if (device != NULL) {
			model = p->type;
			break;
		}
		
		++p;
	}

	device_printf(dev, "assuming type%d model\n", model + 1);

	return (model);
}

/* acpi_spic_write_port1()	- writes value to 1st SPIC port.
 * in:
 *	sc	- driver softc structure
 *	val	- value to write
 * out:
 *	none
 */
static void
acpi_spic_write_port1(struct acpi_sony_softc *sc, uint8_t val)
{
    
	DELAY(10);
    
	outb(sc->port_addr, val);
}

/* acpi_spic_write_port2()	- writes value to 2nd SPIC port.
 * in:
 *	sc	- driver softc structure
 *	val	- value to write
 * out:
 *	none
 */
static void
acpi_spic_write_port2(struct acpi_sony_softc *sc, uint8_t val)
{
    
	DELAY(10);
    
	outb(sc->port_addr + 4, val);
}

/* acpi_spic_read_port1()	- reads value from 1st SPIC port.
 * in:
 *	sc	- driver softc structure
 * out:
 *	readed value
 */
static uint8_t
acpi_spic_read_port1(struct acpi_sony_softc *sc)
{
    
	DELAY(10);
    
	return (inb(sc->port_addr));
}

/* acpi_spic_read_port2()	- reads value from 2nd SPIC port.
 * in:
 *	sc	- driver softc structure
 * out:
 *	readed value
 */
static uint8_t
acpi_spic_read_port2(struct acpi_sony_softc *sc)
{
    
	DELAY(10);
    
	return (inb(sc->port_addr + 4));
}

/* acpi_spic_busy_wait() - synching with port
 * in:
 *	sc	- driver softc structure
 * out:
 *	none
 */
static void
acpi_spic_busy_wait(struct acpi_sony_softc *sc)
{
	int i= 0;

	while (acpi_spic_read_port2(sc) & 2) {
		DELAY(10);
	    
		if (++i > 10000) {
			printf("acpi_spic_busy_wait() abort\n");
			return;
		}
	}
}

/* acpi_spic_call1() - first variant of call
 * in:
 *	sc	- driver softc structure
 *	dev	- which device to query
 * out:
 *	returned value
 */
static uint8_t
acpi_spic_call1(struct acpi_sony_softc *sc, uint8_t dev)
{
    
	acpi_spic_busy_wait(sc);
	acpi_spic_write_port2(sc, dev);
	acpi_spic_read_port2(sc);
    
	return (acpi_spic_read_port1(sc));
}

/* acpi_spic_call1() - second variant of call
 * in:
 *	sc	- driver softc structure
 *	dev	- which device to query
 *	fn	- function to call
 * out:
 *	returned value
 */
static uint8_t
acpi_spic_call2(struct acpi_sony_softc *sc, uint8_t dev,
	uint8_t fn)
{
    
	acpi_spic_busy_wait(sc);
	acpi_spic_write_port2(sc, dev);
	acpi_spic_busy_wait(sc);
	acpi_spic_write_port1(sc, fn);
    
	return (acpi_spic_read_port1(sc));
}

#ifdef code_for_camera_works
static uint8_t
acpi_spic_call3(struct acpi_sony_softc *sc, uint8_t dev, uint8_t fn,
	uint8_t v)
{
	acpi_spic_busy_wait(sc);
	acpi_spic_write_port2(sc, dev);
	acpi_spic_busy_wait(sc);
	acpi_spic_write_port1(sc, fn);
	acpi_spic_busy_wait(sc);
	acpi_spic_write_port1(sc, v);
    
	return (acpi_spic_read_port1(sc));
}
#endif

/* acpi_spic_crs() - dumps currently used by device resources
 * in:
 *	sc	- driver softc structure
 * out:
 *	none
 */
static void
acpi_spic_crs(struct acpi_sony_softc *sc)
{
    	ACPI_STATUS	status;
	ACPI_BUFFER	buffer;
 
	struct {
		ACPI_RESOURCE io;
		ACPI_RESOURCE intr;
		ACPI_RESOURCE endtag;
	} *resource;	

	/* init acpi_buffer */
	resource = AcpiOsAllocate(sizeof(*resource) );
    
	if (resource == NULL) /* failed to alllocate resource structure */
		return;

	buffer.Length = sizeof(*resource) ;
	buffer.Pointer = resource;
    
	status = AcpiGetCurrentResources(sc->handle, &buffer);

	/* check for total failure */
	if (ACPI_FAILURE(status)) {
		device_printf(sc->dev, "failed to get current resources.\n");
		return;
	}

	device_printf(sc->dev, "Current resources = IO: 0x%x/%d, IRQ: 0x%x\n",
		      resource->io.Data.Io.Minimum, resource->io.Data.Io.AddressLength,
		      resource->intr.Data.Irq.Interrupts[0]);
}

/* acpi_spic_dis() - disables SPIC by calling of _DIS method
 * in:
 *	sc	- driver softc structure
 * out:
 *	none
 */
static void
acpi_spic_dis(struct acpi_sony_softc *sc)
{
    
	if (ACPI_FAILURE(AcpiEvaluateObject(sc->handle,
			"_DIS", NULL, NULL)))
	{
	    	device_printf(sc->dev, "failed to disable\n");
		return;
	}

	device_printf(sc->dev, "device is disabled\n");
}

/* acpi_spic_srs() - calls _SRC method to set allocated resources
 * in:
 *	sc	- driver softc structure
 * out:
 *	0		- all is ok
 *	not 0	- some error occured
 */
static int
acpi_spic_srs(struct acpi_sony_softc *sc)
{
	ACPI_STATUS	status;
	ACPI_BUFFER	buffer;
	int			result = 0;
 
	struct {
		ACPI_RESOURCE io;
		ACPI_RESOURCE intr;
		ACPI_RESOURCE endtag;
	} *resource;	

	/* init acpi_buffer */
	resource = AcpiOsAllocate(sizeof(*resource));
    
	if (resource == NULL) /* failed to alllocate resource structure */
		return (ENOMEM);

	buffer.Length = sizeof(*resource) ;
	buffer.Pointer = resource;

	/* setup io resource */
	resource->io.Type = ACPI_RESOURCE_TYPE_IO;
	resource->io.Length = sizeof(ACPI_RESOURCE);
	memcpy(&(resource->io.Data.Io), &(sc->io), sizeof(ACPI_RESOURCE_IO));

	/* setup irq resource */
	resource->intr.Type = ACPI_RESOURCE_TYPE_IRQ;
	resource->intr.Length = sizeof(ACPI_RESOURCE);
	memcpy(&resource->intr.Data.Irq, &(sc->interrupt),
			sizeof(ACPI_RESOURCE_IRQ));
    
	resource->endtag.Type = ACPI_RESOURCE_TYPE_END_TAG;

	/* Attempt to set the resource */
	status = AcpiSetCurrentResources(sc->handle, &buffer);

	/* check for total failure */
	if (ACPI_FAILURE(status)) {
		device_printf(sc->dev, "failed to set resources.\n");
		result = ENODEV;
	}

end:
	AcpiOsFree(resource);
    
	return (result);
}

/* acpi_spic_try_irq() - trys to allocate irq, found while walking
 *		ACPI resources for SPIC
 * in:
 *	sc			- driver softc structure
 *	intn		- irq index in provided resource
 *	resources	- ACPI resource containing IRQ data 
 * out:
 *	1	- all is ok
 *	0	- some error occured
 */
static int
acpi_spic_try_irq(struct acpi_sony_softc *sc, uint8_t intn,
	ACPI_RESOURCE *resource)
{
   
	if (!(sc->intr_res = bus_alloc_resource(sc->dev, SYS_RES_IRQ,
						&sc->intr_rid,
						resource->Data.Irq.Interrupts[intn],
						resource->Data.Irq.Interrupts[intn], 1,
						RF_ACTIVE | RF_SHAREABLE)))
	{
		return (0);
	}
	
	sc->intr = (uint16_t)rman_get_start(sc->intr_res);
    
	memcpy(&(sc->interrupt), &(resource->Data.Irq),
	       sizeof(ACPI_RESOURCE_IRQ));
    
	sc->interrupt.Interrupts[0] = sc->intr;
	sc->interrupt.InterruptCount = 1;
        
    	device_printf(sc->dev,"using IRQ 0x%x\n", sc->intr);
    
	return (1);
}

/* acpi_spic_try_io() - trys to allocate io resource, found while walking
 *		ACPI resources for SPIC
 * in:
 *	sc			- driver softc structure
  *	resources	- ACPI resource containing IO data 
 * out:
 *	1	- all is ok
 *	0	- some error occured
 */
static int
acpi_spic_try_io(struct acpi_sony_softc *sc, ACPI_RESOURCE *resource)
{
	if (!(sc->port_res = bus_alloc_resource(sc->dev, SYS_RES_IOPORT,
						&sc->port_rid,
						resource->Data.Io.Minimum,
						resource->Data.Io.Maximum,
						resource->Data.Io.AddressLength,
						RF_ACTIVE | RF_SHAREABLE)))
	{
		return (0);
	}
	
	device_printf(sc->dev, "using IO ports 0x%x-0x%x/%d\n",
                resource->Data.Io.Minimum,
		resource->Data.Io.Maximum,
		resource->Data.Io.AddressLength);
    
	sc->port_addr = (uint16_t)rman_get_start(sc->port_res);
	
	memcpy(&(sc->io), &(resource->Data.Io), sizeof(ACPI_RESOURCE_IO));
        
	return (1);
}

/* acpi_spic_walk_prs() - callback function used in walking of
 *		ACPI resources for SPIC
 * in:
 *	resource	- found ACPI resource
  *	context		- context (driver softc structure) 
 * out:
 *	AE_CTRL_TERMINATE	- if found, or any error
 *	AE_OK				- processed resource
 */
static ACPI_STATUS
acpi_spic_walk_prs(ACPI_RESOURCE *resource, void *context)
{
	struct acpi_sony_softc *sc = (struct acpi_sony_softc *)context;
    
	if (sc->port_res && sc->intr_res)
		return (AE_CTRL_TERMINATE);
	
	switch (resource->Type) {
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
		return AE_OK;
	    
	case ACPI_RESOURCE_TYPE_IRQ:
		
		/* already allocated */
		if (sc->intr_res)
			return AE_OK;
	    
		for (int i = 0; i < resource->Data.Irq.InterruptCount; ++i)
			if (acpi_spic_try_irq(sc, i, resource))
				break;
				
                return (AE_OK);
	    
	case ACPI_RESOURCE_TYPE_IO:
		
		if (sc->port_res)
			return (AE_OK);
	    
 		if (resource->Data.Io.AddressLength <= 0)
			return (AE_OK);
			     
		acpi_spic_try_io(sc, resource);
	    
		return (AE_OK);
	
	case ACPI_RESOURCE_TYPE_END_TAG:
		return (AE_OK);
	}
    
	return (AE_CTRL_TERMINATE);
}

/* acpi_spic_attach() - attaches driver for SPIC
 * in:
 *	sc			- driver softc structure
* out:
 *	0		- all is ok
 *	not 0	- some error
 */
static int
acpi_spic_attach(struct acpi_sony_softc *sc)
{
	int result = 0;
	int i = 0;
	
	ACPI_STATUS	status;
    
	devclass_t	ec_devclass;

	sc->model = acpi_spic_type(sc->dev);
	sc->evport_offset = spic_types[sc->model].evport_offset;
	sc->events = spic_types[sc->model].events;

	if (!(ec_devclass = devclass_find ("acpi_ec"))) {
		device_printf(sc->dev, "Couldn't find acpi_ec devclass\n");
		return (EINVAL);
	}
	
	if (!(sc->ec_dev = devclass_get_device(ec_devclass, 0))) {
		device_printf(sc->dev, "Couldn't find acpi_ec device\n");
		return (EINVAL);
	}
	
	sc->ec_handle = acpi_get_handle(sc->ec_dev);

    	for (i = 0 ; acpi_spic_oids[i].name != NULL; i++){
		SYSCTL_ADD_PROC(&(sc->sysctl_ctx),
			SYSCTL_CHILDREN(sc->sysctl_tree),
			OID_AUTO, acpi_spic_oids[i].name,
			acpi_spic_oids[i].access,
			sc, i, acpi_spic_sysctl, "I",
			acpi_spic_oids[i].comment);
	}

	acpi_spic_crs(sc);
    
	status = AcpiWalkResources(sc->handle, "_PRS", acpi_spic_walk_prs, sc);

	if (	(sc->port_res == NULL) ||
		(sc->intr_res == NULL))
	{
		device_printf(sc->dev, "failed to allocate resources\n");
		return (ENXIO);
	}

	if ( (result = acpi_spic_srs(sc))  == 0) {
    
		result = bus_setup_intr(sc->dev, sc->intr_res, INTR_TYPE_MISC, NULL,
					acpi_spic_intr, sc, &sc->icookie);

		acpi_spic_call1(sc, 0x82);
		acpi_spic_call2(sc, 0x81, 0xff);
		acpi_spic_call1(sc, 0x82);
	    
		//make_dev(&spic_cdevsw, 0, 0, 0, 0600, "jogdial");
	}

	return (result);
}

/* acpi_spic_sysctl_get() - used to get values for SPIC oids
 * in:
 *	sc			- driver softc structure
 *	method		- method code
* out:
 *	integer value of oid
 */
static int
acpi_spic_sysctl_get(struct acpi_sony_softc *sc, int method)
{
	ACPI_INTEGER    val_ec;
	int             val = 0;
    
    	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	switch (method) {
		case ACPI_SONY_METHOD_BLUETOOTH_POWER:
			val = sc->power_bluetooth;
			break;
	    
		case ACPI_SONY_METHOD_FAN:
			ACPI_EC_READ(sc->ec_dev, SONY_EC_FANSPEED, &val_ec, 1);
			val = val_ec;
			break;

		default:
			break;
	}

	return (val);
}

/* acpi_spic_sysctl_set() - used to set values for SPIC oids
 * in:
 *	sc			- driver softc structure
 *	method		- method code
 *	arg 		- value to set
* out:
 *	0		- all is ok
 *	not 0	- some error
 */
static int
acpi_spic_sysctl_set(struct acpi_sony_softc *sc, int method, int arg)
{
	ACPI_INTEGER		val_ec;
	
	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	switch (method) {
	    case ACPI_SONY_METHOD_FAN:
			val_ec = arg;
			ACPI_EC_WRITE(sc->ec_dev, SONY_EC_FANSPEED, val_ec, 1);
			break;
	
	case ACPI_SONY_METHOD_BLUETOOTH_POWER:
	 	if (sc->power_bluetooth == arg)
			return (0);

		acpi_spic_call2(sc, 0x96, arg);
		acpi_spic_call1(sc, 0x82);
		sc->power_bluetooth = arg;
	    
		break;
	    
	default:
		break;
	}

	return (0);
}

/* acpi_spic_sysctl() - called when needed to set or get
 *				values for SPIC oids
 * in:
 *	SYSCTL_HANDLER_ARGS
* out:
 *	0		- success
 *	not 0	- some error
 */
static int
acpi_spic_sysctl(SYSCTL_HANDLER_ARGS)
{
        struct acpi_sony_softc	*sc;
        int						arg;
	int						error = 0;
	int						function;
	int						method;

        sc = (struct acpi_sony_softc *)oidp->oid_arg1;
        function = oidp->oid_arg2;
        method = acpi_spic_oids[function].method;

	arg = acpi_spic_sysctl_get(sc, method);
	error = sysctl_handle_int(oidp, &arg, 0, req);
	
	if (error != 0 || req->newptr == NULL)
		goto out;

	error = acpi_spic_sysctl_set(sc, method, arg);

out:
	return (error);
}

/* acpi_spic_report_event() - used to report about occured SPIC event
 * in:
 *	sc			- driver softc structure
 *	event		- SPIC event code
 *	mask		- mask of event
 *	mapped_event- one of SPIC_EVENT_.. codes
* out:
 *	none
 */
static void
acpi_spic_report_event(struct acpi_sony_softc *sc, uint8_t event,
	uint8_t mask, uint8_t mapped_event)
{
#ifdef ACPI_SONY_VERBOSE
	if (mapped_event != SPIC_EVENT_NONE)
		device_printf(sc->dev, "event 0x%x/0x%x => 0x%x (%s)\n",
			event, mask, mapped_event, event_descriptors[mapped_event - 1].name);
#endif
    	
	/* notify devd, system "ACPI", subsystem "SPIC" */
	acpi_UserNotify("SPIC", sc->handle, mapped_event);
    
	/* clear event */
	acpi_spic_call2(sc, 0x81, 0xff);
}

/* acpi_spic4_pkey_handler() - special programmable key handling for type4
 *		cause of stamina/speed switch generates same event number as
 *		programmable key 2 in second turn
 * in:
 *	sc			- driver softc structure
 *	group		- event group
 *	mask		- mask of event
 *	event		- SPIC event code
* out:
 *	1			- event handled
 *	0			- event not handled
 */
static int
acpi_spic4_pkey_handler(struct acpi_sony_softc *sc,
	struct spic_event_group *group, uint8_t mask, uint8_t event)
{
	
	switch (sc->prev_event) {
	case 0:
	case 0x5c:
		break;
	    
	case 0x61:
		if (event == 0x02)
			acpi_spic_report_event(sc, event, mask,
				SPIC_EVENT_TOGGLE_SPEED);
	    
		return (1);
	
	case 0x5f:
		/* i have not received such mask on my laptop */
		
	default:
		device_printf(sc->dev,
					"pkey_handler: unknown event 0x%x/0x%x "
					"(prev: 0x%x)\n",
					event, mask, sc->prev_event);
	    
		return (0);
	}
    
	return acpi_spic_default_handler(sc, group, mask, event);
}

/* acpi_spic4_extra_handler() - special handling for type4 model
 * in:
 *	sc			- driver softc structure
 *	group		- event group
 *	mask		- mask of event
 *	event		- SPIC event code
* out:
*	1			- event handled
 *	0			- event not handled
 */
static int
acpi_spic4_extra_handler(struct acpi_sony_softc *sc,
	struct spic_event_group *group, uint8_t mask, uint8_t event)
{
	switch (event) {
		case 0x5c:
		case 0x5f:
			acpi_spic_call1(sc, 0xA0);
			break;

		case 0x61:
			acpi_spic_call1(sc, 0xB3);
			break;

		default:	/* event was unhandled */
			device_printf(sc->dev, "type4 unhandled 0x%x/0x%x\n",
				event, mask);
			return (0);
	}

	if (sc->prev_event == 0) {
		/*device_printf(sc->dev, "type4 special event 0x%x/0x%x\n",
		      event, mask);
		*/
		sc->prev_event = event;
	} else {
		sc->prev_event = 0;
	}
    
	return (1);
}

/* acpi_spic_default_handler() - default handler for all event groups
 * in:
 *	sc			- driver softc structure
 *	group		- event group
 *	mask		- mask of event
 *	event		- SPIC event code
 * out:
 *	1			- event handled
 *	0			- event not handled
 */
static int
acpi_spic_default_handler(struct acpi_sony_softc *sc,
	struct spic_event_group *group, uint8_t mask, uint8_t event)
{
	struct spic_event	*event_entry = group->events;
		    
	while (event_entry->event != 0) {
		
		if (event_entry->code > event)
			break;	/* no event with needed code */
	    
		if (event_entry->code == event) {
			/* found needed code */
			acpi_spic_report_event(sc, event, mask, event_entry->event);
			
			return (1);
		}
		
		++event_entry;
	}
    
	return (0);
}

/* acpi_spic_intr() - SPIC ISR
 * in:
 *	arg		- provided argument (driver softc structure)
 * out:
 *	none
 */
static void
acpi_spic_intr(void * arg)
{
	struct acpi_sony_softc *sc = (struct acpi_sony_softc *)arg;
    
	if (sc == NULL)
		return;
    
	uint8_t event = inb(sc->port_addr);
	uint8_t mask = inb(sc->port_addr + sc->evport_offset);
    
	if ( (event == 0x00) || (event == 0xff))
		return;		/* ignore */

	struct spic_event_group	*group = sc->events;
	
	/* searching needed group by mask */
	while (group->events != NULL) {
	    
		if (group->mask > mask) {
			break;	/* no group with needed mask */
		}
	    
		if (group->mask == mask) {	/* processing */
			if (	(group->handler != NULL) &&
				(group->handler(sc, group, mask, event)))
				return;		/* handler processed event */
		    
			if (acpi_spic_default_handler(sc, group, mask, event))
				return;		/* default handler processed event */	
		}
		
		++group;
	}
    
	device_printf(sc->dev, "event 0x%x/0x%x unhandled\n", event, mask);
	
	/*clear event */
	acpi_spic_call2(sc, 0x81, 0xff);
    
	return;
}

/*
 * SNC related
 */

/* acpi_snc_attach() - attaches driver for SNC
 * in:
 *	sc			- driver softc structure
 * out:
*	not 0		- failed
 *	0			- success
 */
static int
acpi_snc_attach(struct acpi_sony_softc *sc)
{
	int			i;
	int			tst;
	ACPI_HANDLE	tst_handle;
	
	/* adding oids */
	for (i = 0 ; acpi_snc_oids[i].nodename != NULL; i++){
	    
		/* skip if SNC not supports get method */
		if (ACPI_FAILURE(acpi_GetInteger(sc->handle,
			acpi_snc_oids[i].getmethod, &tst)))
			continue;
	    
		/* skip if SNC not supports set method */
		if (acpi_snc_oids[i].setmethod &&
			ACPI_FAILURE(acpi_GetHandleInScope(sc->handle,
				acpi_snc_oids[i].setmethod, &tst_handle)))
			continue;

		SYSCTL_ADD_PROC(&(sc->sysctl_ctx),
			SYSCTL_CHILDREN(sc->sysctl_tree),
			OID_AUTO, acpi_snc_oids[i].nodename,
			CTLTYPE_INT | ((acpi_snc_oids[i].setmethod != NULL) ?
				CTLFLAG_RW : CTLFLAG_RD),
			sc->dev, i, acpi_snc_sysctl, "I",
			acpi_snc_oids[i].comment);
	}
  
	/* initialize special machine types (C, FZ, etc) */
    
	if (ACPI_SUCCESS(acpi_GetHandleInScope(sc->handle,
				"SN02", &tst_handle)))
	{
		/* seems model has needed methods */
		if (	ACPI_FAILED(acpi_SetInteger(sc->handle, "SN02", 0x4)) ||
			ACPI_FAILED(acpi_SetInteger(sc->handle,  "SN07", 0x2))||
			ACPI_FAILED(acpi_SetInteger(sc->handle, "SN02", 0x10)) ||
			ACPI_FAILED(acpi_SetInteger(sc->handle, "SN07", 0x0)) ||
			ACPI_FAILED(acpi_SetInteger(sc->handle, "SN03", 0x2)) ||
			ACPI_FAILED(acpi_SetInteger(sc->handle, "SN07", 0x101)))
		{
			device_printf(sc->dev, "failed to perform additional initialization\n");;
		}
	}
    
    	AcpiInstallNotifyHandler(sc->handle, ACPI_DEVICE_NOTIFY,
		acpi_snc_notify_handler, sc);
 
	return (0);
}

/* acpi_snc_sysctl() - called when needed to set or get
 *				values for SNC oids
 * in:
 *	SYSCTL_HANDLER_ARGS
* out:
 *	0		- success
 *	not 0	- some error
 */
static int 
acpi_snc_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t	dev = arg1;
	int 		function = oidp->oid_arg2;
	int		error = 0, val;

	acpi_GetInteger(acpi_get_handle(dev),
	    acpi_snc_oids[function].getmethod, &val);

	error = sysctl_handle_int(oidp, &val, 0, req);

	if (error || !req->newptr || !acpi_snc_oids[function].setmethod)
		return (error);

	acpi_SetInteger(acpi_get_handle(dev),
	    acpi_snc_oids[function].setmethod, val);

	return (0);
}

static int
acpi_snc_setinteger(acpi_sony_softc *sc, char* method, int32_t val, int32_t result)
{
	ACPI_OBJECT_LIST params;
	ACPI_OBJECT in_obj;
	ACPI_BUFFER output;
	ACPI_OBJECT out_obj;
	ACPI_STATUS status;

	params.Count = 1;
	params.Pointer = &in_obj;

	in_obj.Type = ACPI_TYPE_INTEGER;
	in_obj.Integer.Value = val;

	output.Length = sizeof(out_obj);
	output.Pointer = &out_obj;

	status = AcpiEvaluateObject(handle, method, &params, &output);

        if (status == AE_OK) {
		if (result != NULL) {
			if (out_obj.type != ACPI_TYPE_INTEGER) {
				device_printf(sc->dev,  "method returned gibberish\n");
				return (0);
			}
			
			*result = out_obj.integer.value;
		}
		return (1);
	}

	device_printf(sc->dev, "evaluation failed\n");

	return (0);
}

static void
acpi_snc_notify_handler(ACPI_HANDLE h, uint32_t notify, void *context)
{
	ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, notify);
    
	struct acpi_sony_softc	*sc = context;
    
	if (notify == 0x92) {
		if (ACPI_FAILURE(acpi_SetInteger(handle, "SN07", 0x0202)))
			device_printf(sc->dev, "unable to decode event 0x%.2x\n", notify);
		else {}
			/* ev = result & 0xFF; */
	}

}

/*
 * Common
 */

/* acpi_sony_probe() - probing for device
 * in:
 *	dev	- probed device
* out:
 *	0	- wanna attach
 *	<0	- some error
 */
static int
acpi_sony_probe(device_t dev)
{
	struct acpi_sony_softc *sc;

	sc = device_get_softc(dev);
    
	if (acpi_disabled("sony") || device_get_unit(dev) > 1)
		return (ENXIO);

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, sony_snc_id)) {
		device_set_desc(dev, "Sony notebook controller");
		return (0);
	}
    
    	if (ACPI_ID_PROBE(device_get_parent(dev), dev, sony_spic_id)) {
		device_set_desc(dev, "Sony Programable I/O controller");
		return (0);
	}

	return (ENXIO);
}

/* acpi_sony_attach() - attaches driver to device
 * in:
 *	dev	- device to attach to
* out:
 *	0	- all is ok
 *	<0	- some error
 */
static int
acpi_sony_attach(device_t dev)
{
	struct acpi_sony_softc	*sc;
        	
	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);
	bzero(sc, sizeof(struct acpi_sony_softc));
	sc->dev = dev;
	sc->handle = acpi_get_handle(sc->dev);
    
    	if (ACPI_ID_PROBE(device_get_parent(dev), dev, sony_spic_id)) {
		sc->is_spic = 1;
	}

	ACPI_SERIAL_BEGIN(sony);
    
	/* sc->sysctl_ctx = device_get_sysctl_ctx(dev); */
	sysctl_ctx_init(&(sc->sysctl_ctx));
	sc->sysctl_tree = SYSCTL_ADD_NODE( &(sc->sysctl_ctx), SYSCTL_STATIC_CHILDREN(_hw),
		OID_AUTO, "vaio", CTLFLAG_RW, 0, "VAIO notebook control");
    
	//sc->sysctl_tree = device_get_sysctl_tree(dev);

	if (sc->is_spic == 0) {	/* attaching SNC driver */
		acpi_snc_attach(sc);
	} else {					/* attaching SPIC driver */
		acpi_spic_attach(sc);
	}
    
	ACPI_SERIAL_END(sony);
	
	return (0);
}

/* acpi_sony_detach() - detaches from device
 * in:
 *	dev	- device to detach from
* out:
 *	0	- wanna attach
 *	<0	- some error
 */
static int 
acpi_sony_detach(device_t dev)
{
	struct acpi_sony_softc *sc;

	sc = device_get_softc(dev);
    
	if (sc->is_spic == 0) {
		AcpiRemoveNotifyHandler(sc->handle, ACPI_DEVICE_NOTIFY,   acpi_snc_notify_handler);
	} else {
		
		acpi_spic_call2(sc, 0x81, 0);	/* disabling events */
		
		acpi_spic_dis(sc);			/* disabling device */

#ifdef ACPI_SONY_VERBOSE
		acpi_spic_crs(sc);
#endif

		if (sc->intr_res != NULL) {
			bus_teardown_intr(sc->dev, sc->intr_res, sc->icookie);
	    
			bus_release_resource(dev, SYS_RES_IRQ,
			    sc->intr_rid, sc->intr_res); 
		}
	    
		if (sc->port_res != NULL)
			bus_release_resource(dev, SYS_RES_IOPORT,
			    sc->port_rid, sc->port_res);
	
	//	destroy_dev(&spic_cdevsw);
	}

	sysctl_ctx_free(&(sc->sysctl_ctx));
    
	return (0);
}
