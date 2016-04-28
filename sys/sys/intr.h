/*-
 * Copyright (c) 2015-2016 Svatopluk Kraus
 * Copyright (c) 2015-2016 Michal Meloun
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

#ifndef _SYS_INTR_H_
#define _SYS_INTR_H_

#include <sys/systm.h>

#define	INTR_IRQ_INVALID	0xFFFFFFFF

enum intr_map_data_type {
	INTR_MAP_DATA_ACPI,
	INTR_MAP_DATA_FDT,
	INTR_MAP_DATA_GPIO,
};

#ifdef DEV_ACPI
struct intr_map_data_acpi {
	u_int			irq;
	enum intr_polarity	pol;
	enum intr_trigger	trig;
};
#endif
#ifdef FDT
struct intr_map_data_fdt {
	u_int	ncells;
	pcell_t	*cells;
};
#endif

struct intr_map_data_gpio {
	u_int			gpio_pin_num;
	u_int			gpio_pin_flags;
	u_int		 	gpio_intr_mode;
};

struct intr_map_data {
	enum intr_map_data_type	type;
	union {
#ifdef DEV_ACPI
		struct intr_map_data_acpi	acpi;
#endif
#ifdef FDT
		struct intr_map_data_fdt	fdt;
#endif
		struct intr_map_data_gpio	gpio;
	};
};

#ifdef notyet
#define	INTR_SOLO	INTR_MD1
typedef int intr_irq_filter_t(void *arg, struct trapframe *tf);
#else
typedef int intr_irq_filter_t(void *arg);
#endif

#define INTR_ISRC_NAMELEN	(MAXCOMLEN + 1)

#define INTR_ISRCF_IPI		0x01	/* IPI interrupt */
#define INTR_ISRCF_PPI		0x02	/* PPI interrupt */
#define INTR_ISRCF_BOUND	0x04	/* bound to a CPU */

/* Interrupt source definition. */
struct intr_irqsrc {
	device_t		isrc_dev;	/* where isrc is mapped */
	u_int			isrc_irq;	/* unique identificator */
	u_int			isrc_flags;
	char			isrc_name[INTR_ISRC_NAMELEN];
	cpuset_t		isrc_cpu;	/* on which CPUs is enabled */
	u_int			isrc_index;
	u_long *		isrc_count;
	u_int			isrc_handlers;
	struct intr_event *	isrc_event;
#ifdef INTR_SOLO
	intr_irq_filter_t *	isrc_filter;
	void *			isrc_arg;
#endif
};

/* Intr interface for PIC. */
int intr_isrc_deregister(struct intr_irqsrc *);
int intr_isrc_register(struct intr_irqsrc *, device_t, u_int, const char *, ...)
    __printflike(4, 5);

#ifdef SMP
bool intr_isrc_init_on_cpu(struct intr_irqsrc *isrc, u_int cpu);
#endif

int intr_isrc_dispatch(struct intr_irqsrc *, struct trapframe *);
u_int intr_irq_next_cpu(u_int current_cpu, cpuset_t *cpumask);

int intr_pic_register(device_t, intptr_t);
int intr_pic_deregister(device_t, intptr_t);
int intr_pic_claim_root(device_t, intptr_t, intr_irq_filter_t *, void *, u_int);

extern device_t intr_irq_root_dev;

/* Intr interface for BUS. */
int intr_map_irq(device_t, intptr_t, struct intr_map_data *, u_int *);

int intr_alloc_irq(device_t, struct resource *);
int intr_release_irq(device_t, struct resource *);

int intr_setup_irq(device_t, struct resource *, driver_filter_t, driver_intr_t,
    void *, int, void **);
int intr_teardown_irq(device_t, struct resource *, void *);

int intr_describe_irq(device_t, struct resource *, void *, const char *);

#ifdef DEV_ACPI
u_int intr_acpi_map_irq(device_t, u_int, enum intr_polarity,
    enum intr_trigger);
#endif
#ifdef FDT
u_int intr_fdt_map_irq(phandle_t, pcell_t *, u_int);
#endif
u_int intr_gpio_map_irq(device_t dev, u_int pin_num, u_int pin_flags,
    u_int intr_mode);

#ifdef SMP
int intr_bind_irq(device_t, struct resource *, int);

void intr_pic_init_secondary(void);

/* Virtualization for interrupt source IPI counter increment. */
static inline void
intr_ipi_increment_count(u_long *counter, u_int cpu)
{

	KASSERT(cpu < MAXCPU, ("%s: too big cpu %u", __func__, cpu));
	counter[cpu]++;
}

/* Virtualization for interrupt source IPI counters setup. */
u_long * intr_ipi_setup_counters(const char *name);

#endif
#endif	/* _SYS_INTR_H */
