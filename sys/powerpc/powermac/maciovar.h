/*-
 * Copyright 2002 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACIO_MACIOVAR_H_
#define _MACIO_MACIOVAR_H_

/*
 * The addr space size
 * XXX it would be better if this could be determined by querying the
 *     PCI device, but there isn't an access method for this
 */
#define MACIO_REG_SIZE  0x7ffff

/*
 * Feature Control Registers (FCR)
 */
#define HEATHROW_FCR	0x38
#define KEYLARGO_FCR0	0x38
#define KEYLARGO_FCR1	0x3c
#define KEYLARGO_FCR2	0x40
#define KEYLARGO_FCR3	0x44
#define KEYLARGO_FCR4	0x48
#define KEYLARGO_FCR5	0x4c

#define FCR_ENET_ENABLE	0x60000000
#define FCR_ENET_RESET	0x80000000

/* Used only by macio_enable_wireless() for now. */
#define KEYLARGO_GPIO_BASE	0x6a
#define KEYLARGO_EXTINT_GPIO_REG_BASE	0x58

#define KEYLARGO_MEDIABAY	0x34
#define KEYLARGO_MB0_DEV_ENABLE	0x00001000
#define KEYLARGO_MB0_DEV_POWER	0x00000400
#define KEYLARGO_MB0_DEV_RESET	0x00000200
#define KEYLARGO_MB0_ENABLE	0x00000100
#define KEYLARGO_MB1_DEV_ENABLE	0x10000000
#define KEYLARGO_MB1_DEV_POWER	0x04000000
#define KEYLARGO_MB1_DEV_RESET	0x02000000
#define KEYLARGO_MB1_ENABLE	0x01000000

#define FCR0_CHOOSE_SCCB	0x00000001
#define FCR0_CHOOSE_SCCA	0x00000002
#define FCR0_SLOW_SCC_PCLK	0x00000004
#define FCR0_RESET_SCC		0x00000008
#define FCR0_SCCA_ENABLE	0x00000010
#define FCR0_SCCB_ENABLE	0x00000020
#define FCR0_SCC_CELL_ENABLE	0x00000040
#define FCR0_IRDA_ENABLE	0x00008000
#define FCR0_IRDA_CLK32_ENABLE	0x00010000
#define FCR0_IRDA_CLK19_ENABLE	0x00020000

#define FCR0_USB_REF_SUSPEND	0x10000000

#define FCR1_AUDIO_SEL_22MCLK	0x00000002
#define FCR1_AUDIO_CLK_ENABLE	0x00000008
#define FCR1_AUDIO_CLKOUT_ENABLE	0x00000020
#define FCR1_AUDIO_CELL_ENABLE	0x00000040
#define FCR1_I2S0_CELL_ENABLE	0x00000400
#define FCR1_I2S0_CLK_ENABLE	0x00001000
#define FCR1_I2S0_ENABLE	0x00002000
#define FCR1_I2S1_CELL_ENABLE	0x00020000
#define FCR1_I2S1_CLK_ENABLE	0x00080000
#define FCR1_I2S1_ENABLE	0x00100000
#define FCR1_EIDE0_ENABLE	0x00800000
#define FCR1_EIDE0_RESET	0x01000000
#define FCR1_EIDE1_ENABLE	0x04000000
#define FCR1_EIDE1_RESET	0x08000000
#define FCR1_UIDE_ENABLE	0x20000000
#define FCR1_UIDE_RESET		0x40000000

#define FCR2_IOBUS_ENABLE	0x00000002

#define FCR3_SHUTDOWN_PLL_TOTAL	0x00000001
#define FCR3_SHUTDOWN_PLL_KW6	0x00000002
#define FCR3_SHUTDOWN_PLL_KW4	0x00000004
#define FCR3_SHUTDOWN_PLL_KW35	0x00000008
#define FCR3_SHUTDOWN_PLL_KW12	0x00000010
#define FCR3_SHUTDOWN_PLL_2X	0x00000080
#define FCR3_CLK_66_ENABLE	0x00000100
#define FCR3_CLK_49_ENABLE	0x00000200
#define FCR3_CLK_45_ENABLE	0x00000400
#define FCR3_CLK_31_ENABLE	0x00000800
#define FCR3_TMR_CLK18_ENABLE	0x00001000
#define FCR3_I2S1_CLK18_ENABLE	0x00002000
#define FCR3_I2S0_CLK18_ENABLE	0x00004000
#define FCR3_VIA_CLK16_ENABLE	0x00008000

/*
 * Format of a macio reg property entry.
 */
struct macio_reg {
	u_int32_t	mr_base;
	u_int32_t	mr_size;
};

/*
 * Per macio device structure.
 */
struct macio_devinfo {
	int        mdi_interrupts[6];
	int	   mdi_ninterrupts;
	int        mdi_base;
	struct ofw_bus_devinfo mdi_obdinfo;
	struct resource_list mdi_resources;
};

extern int macio_enable_wireless(device_t dev, bool enable);

#endif /* _MACIO_MACIOVAR_H_ */
