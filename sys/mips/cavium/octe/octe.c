/*-
 * Copyright (c) 2010 Juli Mallett <jmallett@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

/*
 * Cavium Octeon Ethernet devices.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include "wrapper-cvmx-includes.h"
#include "cavium-ethernet.h"

static int		octe_probe(device_t dev);
static int		octe_attach(device_t dev);
static int		octe_detach(device_t dev);
static int		octe_shutdown(device_t dev);

static device_method_t octe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		octe_probe),
	DEVMETHOD(device_attach,	octe_attach),
	DEVMETHOD(device_detach,	octe_detach),
	DEVMETHOD(device_shutdown,	octe_shutdown),

	{ 0, 0 }
};

static driver_t octe_driver = {
	"octe",
	octe_methods,
	sizeof (cvm_oct_private_t),
};

static devclass_t octe_devclass;

DRIVER_MODULE(octe, octebus, octe_driver, octe_devclass, 0, 0);

static driver_t pow_driver = {
	"pow",
	octe_methods,
	sizeof (cvm_oct_private_t),
};

static devclass_t pow_devclass;

DRIVER_MODULE(pow, octebus, pow_driver, pow_devclass, 0, 0);

static int
octe_probe(device_t dev)
{
	return (0);
}

static int
octe_attach(device_t dev)
{
	return (0);
}

static int
octe_detach(device_t dev)
{
	return (0);
}

static int
octe_shutdown(device_t dev)
{
	return (octe_detach(dev));
}
