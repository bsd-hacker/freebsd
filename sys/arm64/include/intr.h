/*-
 * Copyright (c) 2014 Andrew Turner <andrew@FreeBSD.org>
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

#ifndef _MACHINE_INTR_H_
#define	_MACHINE_INTR_H_

int	arm_config_intr(u_int, enum intr_trigger, enum intr_polarity);
void	arm_cpu_intr(struct trapframe *);
void	arm_dispatch_intr(u_int, struct trapframe *);
int	arm_enable_intr(void);
void	arm_mask_irq(u_int);
void	arm_register_root_pic(device_t, u_int);
void	arm_register_msi_pic(device_t);
int	arm_alloc_msi(device_t, device_t, int, int, int *);
int	arm_release_msi(device_t, device_t, int, int *);
int	arm_alloc_msix(device_t, device_t, int *);
int	arm_release_msix(device_t, device_t, int);
int	arm_map_msi(device_t, device_t, int, uint64_t *, uint32_t *);
int	arm_map_msix(device_t, device_t, int, uint64_t *, uint32_t *);
int	arm_setup_intr(const char *, driver_filter_t *, driver_intr_t,
				void *, u_int, enum intr_type, void **);
int	arm_teardown_intr(void *);
void	arm_unmask_irq(u_int);

#ifdef SMP
void	arm_init_secondary(void);
void	arm_setup_ipihandler(driver_filter_t *, u_int);
void	arm_unmask_ipi(u_int);
#endif

#endif	/* _MACHINE_INTR_H */
