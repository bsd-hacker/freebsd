/*-
 * Copyright (c) 2008 TAKAHASHI Yoshihiro
 * Copyright (c) 2003 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/timerreg.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_dev_i8251.h>

#include <dev/ic/i8251.h>
#include <dev/ic/i8255.h>

#include <pc98/pc98/pc98_machdep.h>

#include "uart_if.h"

/*
 * I/O address table
 */
/* Internal i8251 / i8251F */
static bus_addr_t i8251_iat[] =
    { 0, 2, 2, 2, 3, 5, 0, 0, 0x100, 0x102, 0x104, 0x106, 0x108, 0x10a };
/* PC-9861K */
static bus_addr_t pc9861k_ext1_iat[] = { 1, 3, 3, 3, 0, 0 };
static bus_addr_t pc9861k_ext2_iat[] = { 7, 9, 9, 9, 0, 0 };
/* IND-SS / IND-SP */
static bus_addr_t indss_ext1_iat[] = { 1, 3, 3, 3, 0, 0, 3 };
static bus_addr_t indss_ext2_iat[] = { 7, 9, 9, 9, 0, 0, 9 };
/* PIO-9032B */
static bus_addr_t pio9032b_ext1_iat[] = { 1, 3, 3, 3, 0, 0, 8 };
static bus_addr_t pio9032b_ext2_iat[] = { 7, 9, 9, 9, 0, 0, 8 };
/* B98-01 */
static bus_addr_t b9801_ext1_iat[] = { 1, 3, 3, 3, 0, 0, 0x21, 0x23 };
static bus_addr_t b9801_ext2_iat[] = { 7, 9, 9, 9, 0, 0, 0x23, 0x25 };

/*
 * Baudrate table
 */
static struct i8251_speedtab sptab_vfast[] = {
	{ 9600,		12, },
	{ 14400,	8, },
	{ 19200,	6, },
	{ 28800,	4, },
	{ 38400,	3, },
	{ 57600,	2, },
	{ 115200,	1, },
	{ -1,		-1 }
};
static struct i8251_speedtab sptab_pio9032b[] = {
	{ 300,		6, },
	{ 600,		5, },
	{ 1200,		4, },
	{ 2400,		3, },
	{ 4800,		2, },
	{ 9600,		1, },
	{ 19200,	0, },
	{ 38400,	7, },
	{ -1,		-1 }
};
static struct i8251_speedtab sptab_b9801[] = {
	{ 75,		11, },
	{ 150,		10, },
	{ 300,		9, },
	{ 600,		8, },
	{ 1200,		7, },
	{ 2400,		6, },
	{ 4800,		5, },
	{ 9600,		4, },
	{ 19200,	3, },
	{ 38400,	2, },
	{ 76800,	1, },
	{ 153600,	0, },
	{ -1,		-1 }
};

/*
 * Hardware specific table
 */
struct i8251_hw_table i8251_hw[] = {
	{ "internal", i8251_iat, BUS_SPACE_IAT_SZ(i8251_iat),
	  sptab_vfast },
	{ "PC-9861K", pc9861k_ext1_iat, BUS_SPACE_IAT_SZ(pc9861k_ext1_iat),
	  NULL },
	{ "PC-9861K", pc9861k_ext2_iat, BUS_SPACE_IAT_SZ(pc9861k_ext2_iat),
	  NULL },
	{ "IND-SS / IND-SP", indss_ext1_iat, BUS_SPACE_IAT_SZ(indss_ext1_iat),
	  NULL },
	{ "IND-SS / IND-SP", indss_ext2_iat, BUS_SPACE_IAT_SZ(indss_ext2_iat),
	  NULL },
	{ "PIO-9032B", pio9032b_ext1_iat, BUS_SPACE_IAT_SZ(pio9032b_ext1_iat),
	  sptab_pio9032b },
	{ "PIO-9032B", pio9032b_ext2_iat, BUS_SPACE_IAT_SZ(pio9032b_ext2_iat),
	  sptab_pio9032b },
	{ "B98-01", b9801_ext1_iat, BUS_SPACE_IAT_SZ(b9801_ext1_iat),
	  sptab_b9801 },
	{ "B98-01", b9801_ext2_iat, BUS_SPACE_IAT_SZ(b9801_ext2_iat),
	  sptab_b9801 },
};


static void
i8251_probe_fifo(struct uart_bas *bas)
{
	u_int8_t t1, t2;

	t1 = uart_getreg(bas, serf_iir);
	DELAY(10);
	t2 = uart_getreg(bas, serf_iir);

	if ((t1 & IIR_FIFO_CK1) == (t2 & IIR_FIFO_CK1))
		return;
	if ((t1 & IIR_FIFO_CK2) != 0 || (t2 & IIR_FIFO_CK2) != 0)
		return;

#ifndef	I8251_DISABLE_FIFO
	SET_TYPE(bas, COM_SUB_I8251F);
#endif
}

static void
i8251_probe_vfast(struct uart_bas *bas)
{

	uart_setreg(bas, serf_div, 0);
	if (uart_getreg(bas, serf_div) & 0x80)
		return;

#ifndef	I8251_DISABLE_FIFO
	SET_TYPE(bas, COM_SUB_I8251VFAST);
#endif
}

static void
i8251_reset(struct uart_bas *bas, u_int8_t mode, int force)
{

	if (force) {
		uart_setreg(bas, seri_cmd, 0);
		DELAY(30);
		uart_setreg(bas, seri_cmd, 0);
		DELAY(30);
		uart_setreg(bas, seri_cmd, 0);
		DELAY(30);
	}

	uart_setreg(bas, seri_cmd, CMD8251_RESET);
	DELAY(100);
	uart_setreg(bas, seri_mod, mode);
	DELAY(100);
}

static __inline void
i8251_write_cmd(struct uart_bas *bas, u_int8_t cmd)
{

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F) {
		uart_setreg(bas, serf_fcr, 0);
		uart_setreg(bas, seri_cmd, cmd);
		uart_setreg(bas, serf_fcr, I8251F_DEF_FIFO);
	} else
		uart_setreg(bas, seri_cmd, cmd);
}

static __inline void
i8251_init_func(struct uart_bas *bas)
{

	uart_setreg(bas, seri_func, 0xf2);
}

static __inline void
i8251_enable_fifo(struct uart_bas *bas)
{

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F)
		uart_setreg(bas, serf_fcr,
			    I8251F_DEF_FIFO | FIFO_XMT_RST | FIFO_RCV_RST);
}

static __inline void
i8251_disable_fifo(struct uart_bas *bas)
{

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F)
		uart_setreg(bas, serf_fcr, 0);
}

static __inline void
i8251_data_putc(struct uart_bas *bas, u_int8_t c)
{

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F)
		uart_setreg(bas, serf_data, c);
	else
		uart_setreg(bas, seri_data, c);
}

static __inline u_int8_t
i8251_data_getc(struct uart_bas *bas)
{
	u_int8_t c;

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F)
		c = uart_getreg(bas, serf_data);
	else
		c = uart_getreg(bas, seri_data);

	return (c);
}

static __inline int
i8251_check_rxready(struct uart_bas *bas)
{

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F)
		return (uart_getreg(bas, serf_lsr) & FLSR_RxRDY);
	else
		return (uart_getreg(bas, seri_lsr) & STS8251_RxRDY);
}

static __inline int
i8251_check_txready(struct uart_bas *bas)
{

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F)
		return (uart_getreg(bas, serf_lsr) & FLSR_TxRDY);
	else
		return (uart_getreg(bas, seri_lsr) & STS8251_TxRDY);
}

static __inline int
i8251_check_txempty(struct uart_bas *bas)
{

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F)
		return (uart_getreg(bas, serf_lsr) & FLSR_TxEMP);
	else
		return (uart_getreg(bas, seri_lsr) & STS8251_TxEMP);
}

static u_int8_t
i8251_read_lsr(struct uart_bas *bas)
{
	u_int8_t stat, lsr;

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F) {
		stat = uart_getreg(bas, serf_lsr);
		lsr = 0;
		if (stat & FLSR_TxEMP)
			lsr |= STS8251_TxEMP;
		if (stat & FLSR_TxRDY)
			lsr |= STS8251_TxRDY;
		if (stat & FLSR_RxRDY)
			lsr |= STS8251_RxRDY;
		if (stat & FLSR_OE)
			lsr |= STS8251_OE;
		if (stat & FLSR_PE)
			lsr |= STS8251_PE;
		if (stat & FLSR_BI)
			lsr |= STS8251_BI;
	} else {
		lsr = uart_getreg(bas, seri_lsr);
	}

	return (lsr);
}

static u_int8_t
i8251_read_msr(struct uart_bas *bas)
{
	static u_int8_t msr_translate_tbl[] = {
		0,
		MSR_DCD,
		MSR_CTS,
		MSR_DCD | MSR_CTS,
		MSR_RI,
		MSR_RI | MSR_DCD,
		MSR_RI | MSR_CTS,
		MSR_RI | MSR_CTS | MSR_DCD
	};
	u_int8_t stat, msr;

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F)
		return (uart_getreg(bas, serf_msr));

	stat = (uart_getreg(bas, seri_msr) ^ 0xff) >> 5;
	msr = msr_translate_tbl[stat];

	stat = uart_getreg(bas, seri_lsr);
	if (stat & STS8251_DSR)
		msr |= MSR_DSR;

	return (msr);
}

static void
i8251_set_icr(struct uart_bas *bas, u_int8_t icr)
{
	u_int8_t tmp;

	tmp = 0;
	if (GET_IFTYPE(bas) == COM_IF_INTERNAL) {
		tmp = uart_getreg(bas, seri_icr);
		tmp &= ~(IEN_Rx | IEN_TxEMP | IEN_Tx);
	}
	tmp |= icr & (IEN_Rx | IEN_TxEMP | IEN_Tx);

	uart_setreg(bas, seri_icr, tmp);
}


/*
 * Clear pending interrupts. THRE is cleared by reading IIR. Data
 * that may have been received gets lost here.
 */
static void
i8251_clrint(struct uart_bas *bas)
{
	u_int8_t iir, lsr;

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F) {
		iir = uart_getreg(bas, serf_iir);
		while ((iir & IIR_NOPEND) == 0) {
			iir &= IIR_IMASK;
			if (iir == IIR_RLS) {
				lsr = uart_getreg(bas, serf_lsr);
				if (lsr & (FLSR_BI | FLSR_PE))
					(void)uart_getreg(bas, serf_data);
			} else if (iir == IIR_RXRDY || iir == IIR_RXTOUT)
				(void)uart_getreg(bas, serf_data);
			else if (iir == IIR_MLSC)
				(void)uart_getreg(bas, serf_msr);
			iir = uart_getreg(bas, serf_iir);
		}
	} else {
		if (uart_getreg(bas, seri_lsr) & STS8251_RxRDY)
			(void)uart_getreg(bas, seri_data);
	}
}

static int
i8251_drain(struct uart_bas *bas, int what)
{
	int delay, limit;

	delay = 100;

	if (what & UART_DRAIN_TRANSMITTER) {
		/*
		 * Pick an arbitrary high limit to avoid getting stuck in
		 * an infinite loop when the hardware is broken. Make the
		 * limit high enough to handle large FIFOs.
		 */
		limit = 10*256;
		while ((uart_getreg(bas, seri_lsr) & STS8251_TxEMP) == 0 &&
		       --limit)
			DELAY(delay);
		if (limit == 0) {
			/* printf("i8251: transmitter appears stuck... "); */
			return (EIO);
		}
	}

	if (what & UART_DRAIN_RECEIVER) {
		/*
		 * Pick an arbitrary high limit to avoid getting stuck in
		 * an infinite loop when the hardware is broken. Make the
		 * limit high enough to handle large FIFOs and integrated
		 * UARTs.
		 */
		limit=10*1024;
		while ((uart_getreg(bas, seri_lsr) & STS8251_RxRDY) &&
		       --limit) {
			(void)uart_getreg(bas, seri_data);
			DELAY(delay << 2);
		}
		if (limit == 0) {
			/* printf("i8251: receiver appears broken... "); */
			return (EIO);
		}
	}

	return (0);
}

/*
 * We can only flush UARTs with FIFOs. UARTs without FIFOs should be
 * drained. WARNING: this function clobbers the FIFO setting!
 */
static void
i8251_flush(struct uart_bas *bas, int what)
{
	u_int8_t fcr;

	fcr = FIFO_ENABLE;
	if (what & UART_FLUSH_TRANSMITTER)
		fcr |= FIFO_XMT_RST;
	if (what & UART_FLUSH_RECEIVER)
		fcr |= FIFO_RCV_RST;
	uart_setreg(bas, serf_fcr, fcr);
}

static int
i8251_divisor(int rclk, int baudrate)
{
	int actual_baud, divisor;
	int error;

	if (baudrate == 0)
		return (0);

	divisor = (rclk / (baudrate << 3) + 1) >> 1;
	if (divisor == 0 || divisor >= 65536)
		return (0);
	actual_baud = rclk / (divisor << 4);

	/* 10 times error in percent: */
	error = ((actual_baud - baudrate) * 2000 / baudrate + 1) >> 1;

	/* 3.0% maximum error tolerance: */
	if (error < -30 || error > 30)
		return (0);

	return (divisor);
}

static int
i8251_ttspeedtab(int speed, struct i8251_speedtab *table)
{

	if (table == NULL)
		return (-1);

	for (; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);

	return (-1);
}

static int
i8251_set_baudrate(struct uart_bas *bas, int baudrate)
{
	int type, divisor, rclk;

	if (baudrate == 0)
		return (EINVAL);

	type = GET_IFTYPE(bas);

	switch (type) {
	case COM_IF_INTERNAL:
		if (GET_SUBTYPE(bas) & COM_SUB_I8251VFAST) {
			divisor = i8251_ttspeedtab(baudrate,
						   i8251_hw[type].sp_tab);
			if (divisor != -1) {
				divisor |= 0x80;
				uart_setreg(bas, serf_div, divisor);
				return (0);
			} else {
				/* Set compatible mode. */
				uart_setreg(bas, serf_div, 0);
			}
		}

		/* Check system clock. */
		if (pc98_machine_type & M_8M)
			rclk = 1996800;		/* 8 MHz system */
		else
			rclk = 2457600;		/* 5 MHz system */

		divisor = i8251_divisor(rclk, baudrate);
		if (divisor < 2)
			return (EINVAL);
		if (divisor == 3)
			outb(TIMER_MODE,
			     TIMER_SEL2 | TIMER_16BIT | TIMER_RATEGEN);
		else
			outb(TIMER_MODE,
			     TIMER_SEL2 | TIMER_16BIT | TIMER_SQWAVE);
		DELAY(10);
		outb(TIMER_CNTR2, divisor & 0xff);
		DELAY(10);
		outb(TIMER_CNTR2, (divisor >> 8) & 0xff);
		break;
	case COM_IF_PC9861K_1:
	case COM_IF_PC9861K_2:
		/* Cannot set a baudrate. */
		break;
	case COM_IF_IND_SS_1:
	case COM_IF_IND_SS_2:
		divisor = i8251_divisor(460800 * 16, baudrate);
		if (divisor == 0)
			return (EINVAL);
		divisor |= 0x8000;
#if 0
		i8251_reset(bas, I8251_DEF_MODE, 1);
#else
		uart_setreg(bas, seri_icr, 0);
		uart_setreg(bas, seri_div, 0);
#endif
		uart_setreg(bas, seri_cmd, CMD8251_RESET | CMD8251_EH);
		uart_setreg(bas, seri_div, (divisor >> 8) & 0xff);
		uart_setreg(bas, seri_div, divisor & 0xff);
		break;
	case COM_IF_PIO9032B_1:
	case COM_IF_PIO9032B_2:
	case COM_IF_B98_01_1:
	case COM_IF_B98_01_2:
		divisor = i8251_ttspeedtab(baudrate, i8251_hw[type].sp_tab);
		if (divisor == -1)
			return (EINVAL);
		uart_setreg(bas, seri_div, divisor);
		break;
	}

	return (0);
}

static int
i8251_get_baudrate(struct uart_bas *bas)
{
	static int vfast_translate_tbl[] = {
		0, 115200, 57600, 38400, 28800, 0, 19200, 0,
		14400, 0, 0, 0, 9600, 0, 0, 0
	};
	int divisor, rclk;

	switch (GET_IFTYPE(bas)) {
	case COM_IF_INTERNAL:
		if (GET_SUBTYPE(bas) & COM_SUB_I8251VFAST) {
			divisor = uart_getreg(bas, serf_div);
			if (divisor & 0x80)
				return (vfast_translate_tbl[divisor & 0x0f]);
		}

		/* Check system clock */
		if (pc98_machine_type & M_8M)
			rclk = 1996800;		/* 8 MHz system */
		else
			rclk = 2457600;		/* 5 MHz system */

		/* XXX Always set mode3 */
		outb(TIMER_MODE, TIMER_SEL2 | TIMER_16BIT | TIMER_SQWAVE);
		DELAY(10);
		divisor = inb(TIMER_CNTR2);
		DELAY(10);
		divisor |= inb(TIMER_CNTR2) << 8;
		if (divisor != 0)
			return (rclk / divisor / 16);
		break;
	case COM_IF_PC9861K_1:
	case COM_IF_PC9861K_2:
	case COM_IF_IND_SS_1:
	case COM_IF_IND_SS_2:
	case COM_IF_PIO9032B_1:
	case COM_IF_PIO9032B_2:
	case COM_IF_B98_01_1:
	case COM_IF_B98_01_2:
		break;
	}

	return (0);
}

static int
i8251_param(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{
	u_int8_t mod;
	int error;

	mod = 0;
	if (databits >= 8)
		mod |= MOD8251_8BITS;
	else if (databits == 7)
		mod |= MOD8251_7BITS;
	else if (databits == 6)
		mod |= MOD8251_6BITS;
	else
		mod |= MOD8251_5BITS;
	if (stopbits >= 2)
		mod |= MOD8251_STOP2;
	else if (stopbits == 1)
		mod |= MOD8251_STOP1;
	if (parity == UART_PARITY_ODD)
		mod |= MOD8251_PENAB;
	else if (parity == UART_PARITY_EVEN)
		mod |= MOD8251_PENAB | MOD8251_PEVEN;

	/* Set baudrate. */
	if (baudrate > 0) {
		if ((error = i8251_set_baudrate(bas, baudrate)) != 0)
			return (error);
	}

	/* Set mode and command register. */
	i8251_disable_fifo(bas);
	i8251_reset(bas, mod, 1);
	uart_setreg(bas, seri_cmd, I8251_DEF_CMD | CMD8251_ER);
	i8251_enable_fifo(bas);

	return (0);
}

/*
 * Low-level UART interface.
 */
static int i8251_probe(struct uart_bas *bas);
static void i8251_init(struct uart_bas *bas, int, int, int, int);
static void i8251_term(struct uart_bas *bas);
static void i8251_putc(struct uart_bas *bas, int);
static int i8251_rxready(struct uart_bas *bas);
static int i8251_getc(struct uart_bas *bas, struct mtx *);

static struct uart_ops uart_i8251_ops = {
	.probe = i8251_probe,
	.init = i8251_init,
	.term = i8251_term,
	.putc = i8251_putc,
	.rxready = i8251_rxready,
	.getc = i8251_getc,
};

static int
i8251_probe(struct uart_bas *bas)
{
	int error;
	u_int type;
	u_int8_t lsr;

	type = GET_IFTYPE(bas);

	/* Load I/O address table. */
	error = bus_space_map_load(bas->bst, bas->bsh,
	    i8251_hw[type].iatsz, i8251_hw[type].iat, 0);
	if (error)
		return (error);

	/* Probe i8251F and V-FAST. */
	if (type == COM_IF_INTERNAL) {
		i8251_probe_fifo(bas);
		i8251_probe_vfast(bas);
	}

	/*
	 * Clear fifo advanced mode, because line status register has
	 * no response under the i8251F mode. 
	 */
	i8251_disable_fifo(bas);

	/* Reset i8251. */
	i8251_reset(bas, I8251_DEF_MODE, 1);

	/* Initialize function regsiter for B98-01. */
	if (type ==  COM_IF_B98_01_1 || type ==  COM_IF_B98_01_2)
		i8251_init_func(bas);

	/* Disable transmit. */
	uart_setreg(bas, seri_cmd, CMD8251_DTR | CMD8251_RTS);
	DELAY(100);

	/* Check tx buffer empty. */
	uart_setreg(bas, seri_cmd, CMD8251_DTR | CMD8251_RTS);
	lsr = uart_getreg(bas, seri_lsr);
	if ((lsr & STS8251_TxRDY) == 0)
		return (ENXIO);

	/* Write 2 bytes. */
	uart_setreg(bas, seri_data, ' ');
	DELAY(100);
	uart_setreg(bas, seri_data, ' ');
	DELAY(100);

	/* Check tx buffer non empty. */
	lsr = uart_getreg(bas, seri_lsr);
	if ((lsr & STS8251_TxRDY) != 0)
		return (ENXIO);

	/* Clear tx buffer. */
	i8251_reset(bas, I8251_DEF_MODE, 0);

	return (0);
}

static void
i8251_init(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{

	/* Disable the FIFO (if present). */
	i8251_disable_fifo(bas);

	/* Reset i8251. */
	i8251_reset(bas, I8251_DEF_MODE, 1);

	i8251_param(bas, baudrate, databits, stopbits, parity);

	/* Disable the FIFO again. */
	i8251_disable_fifo(bas);

        /* Disable all interrupt sources. */
	i8251_set_icr(bas, 0);

	/* Set RTS & DTR. */
	uart_setreg(bas, seri_cmd, I8251_DEF_CMD);

	i8251_drain(bas, UART_DRAIN_RECEIVER | UART_DRAIN_TRANSMITTER);

	/* Reset and enable FIFO. */
	i8251_enable_fifo(bas);
}

static void
i8251_term(struct uart_bas *bas)
{

	/* Clear DTR & RTS. */
	i8251_write_cmd(bas, I8251_DEF_CMD & ~(CMD8251_DTR | CMD8251_RTS));
}

static void
i8251_putc(struct uart_bas *bas, int c)
{
	int limit;

	limit = 250000;
	while (i8251_check_txready(bas) == 0 && --limit)
		DELAY(4);
	i8251_data_putc(bas, c);
	limit = 250000;
	while (i8251_check_txempty(bas) == 0 && --limit)
		DELAY(4);
}

static int
i8251_rxready(struct uart_bas *bas)
{

	return (i8251_check_rxready(bas) != 0 ? 1 : 0);
}

static int
i8251_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	int c;

	uart_lock(hwmtx);

	while (i8251_check_rxready(bas) == 0) {
		uart_unlock(hwmtx);
		DELAY(4);
		uart_lock(hwmtx);
	}

	c = i8251_data_getc(bas);

	uart_unlock(hwmtx);

	return (c);
}

/*
 * High-level UART interface.
 */
struct i8251_softc {
	struct uart_softc base;
	u_int8_t icr;
	u_int8_t cmd;
	u_int8_t msr;
	struct callout_handle timeout_msr;
};

static void i8251_enable_msrintr(struct uart_softc *);
static void i8251_disable_msrintr(struct uart_softc *);

static int i8251_bus_attach(struct uart_softc *);
static int i8251_bus_detach(struct uart_softc *);
static int i8251_bus_flush(struct uart_softc *, int);
static int i8251_bus_getsig(struct uart_softc *);
static int i8251_bus_ioctl(struct uart_softc *, int, intptr_t);
static int i8251_bus_ipend(struct uart_softc *);
static int i8251_bus_param(struct uart_softc *, int, int, int, int);
static int i8251_bus_probe(struct uart_softc *);
static int i8251_bus_receive(struct uart_softc *);
static int i8251_bus_setsig(struct uart_softc *, int);
static int i8251_bus_transmit(struct uart_softc *);

static kobj_method_t i8251_methods[] = {
	KOBJMETHOD(uart_attach,		i8251_bus_attach),
	KOBJMETHOD(uart_detach,		i8251_bus_detach),
	KOBJMETHOD(uart_flush,		i8251_bus_flush),
	KOBJMETHOD(uart_getsig,		i8251_bus_getsig),
	KOBJMETHOD(uart_ioctl,		i8251_bus_ioctl),
	KOBJMETHOD(uart_ipend,		i8251_bus_ipend),
	KOBJMETHOD(uart_param,		i8251_bus_param),
	KOBJMETHOD(uart_probe,		i8251_bus_probe),
	KOBJMETHOD(uart_receive,	i8251_bus_receive),
	KOBJMETHOD(uart_setsig,		i8251_bus_setsig),
	KOBJMETHOD(uart_transmit,	i8251_bus_transmit),
	{ 0, 0 }
};

struct uart_class uart_i8251_class = {
	"i8251",
	i8251_methods,
	sizeof(struct i8251_softc),
	.uc_ops = &uart_i8251_ops,
	.uc_range = 1,
	.uc_rclk = 0
};

#define	SIGCHG(c, i, s, d)				\
	if (c) {					\
		i |= (i & s) ? s : s | d;		\
	} else {					\
		i = (i & s) ? (i & ~s) | d : i;		\
	}

static int
i8251_bus_attach(struct uart_softc *sc)
{
	struct i8251_softc *i8251;
	struct uart_bas *bas;
	int type, error;

	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;
	type = GET_IFTYPE(bas);

	/* Load I/O address table. */
	error = bus_space_map_load(bas->bst, bas->bsh,
	    i8251_hw[type].iatsz, i8251_hw[type].iat, 0);
	if (error)
		return (error);

	i8251_bus_flush(sc, UART_FLUSH_RECEIVER | UART_FLUSH_TRANSMITTER);

	i8251->cmd = I8251_DEF_CMD;
	if (i8251->cmd & CMD8251_DTR)
		sc->sc_hwsig |= SER_DTR;
	if (i8251->cmd & CMD8251_RTS)
		sc->sc_hwsig |= SER_RTS;
	i8251_bus_getsig(sc);

	i8251_clrint(bas);
	i8251->icr = IEN_Rx;
	i8251_set_icr(bas, i8251->icr);

	i8251_enable_msrintr(sc);

	return (0);
}

static int
i8251_bus_detach(struct uart_softc *sc)
{
	struct uart_bas *bas;

	bas = &sc->sc_bas;

	i8251_disable_msrintr(sc);

	i8251_set_icr(bas, 0);
	i8251_clrint(bas);

	return (0);
}

static int
i8251_bus_flush(struct uart_softc *sc, int what)
{
	struct i8251_softc *i8251;
	struct uart_bas *bas;
	int error;

	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;

	uart_lock(sc->sc_hwmtx);
	if (GET_SUBTYPE(bas) & COM_SUB_I8251F) {
		i8251_flush(bas, what);
		uart_setreg(bas, serf_fcr, I8251F_DEF_FIFO);
		error = 0;
	} else
		error = i8251_drain(bas, what);
	uart_unlock(sc->sc_hwmtx);

	return (error);
}

static int
i8251_bus_getsig(struct uart_softc *sc)
{
	struct uart_bas *bas;
	u_int32_t new, old, sig;
	u_int8_t msr;

	bas = &sc->sc_bas;

	do {
		old = sc->sc_hwsig;
		sig = old;
		uart_lock(sc->sc_hwmtx);
		msr = i8251_read_msr(bas);
		uart_unlock(sc->sc_hwmtx);
		SIGCHG(msr & MSR_DSR, sig, SER_DSR, SER_DDSR);
		SIGCHG(msr & MSR_CTS, sig, SER_CTS, SER_DCTS);
		SIGCHG(msr & MSR_DCD, sig, SER_DCD, SER_DDCD);
		SIGCHG(msr & MSR_RI,  sig, SER_RI,  SER_DRI);
		new = sig & ~SER_MASK_DELTA;
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));

	return (sig);
}

static int
i8251_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{
	struct i8251_softc *i8251;
	struct uart_bas *bas;
	int baudrate, error;
	u_int8_t cmd;

	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;

	error = 0;

	uart_lock(sc->sc_hwmtx);
	switch (request) {
	case UART_IOCTL_BREAK:
		cmd = i8251->cmd;
		if (data)
			cmd |= CMD8251_SBRK;
		else
			cmd &= ~CMD8251_SBRK;
		i8251_write_cmd(bas, cmd);
		break;
	case UART_IOCTL_IFLOW:
		cmd = i8251->cmd;
		if (data)
			cmd |= CMD8251_RxEN;
		else
			cmd &= ~CMD8251_RxEN;
		i8251_write_cmd(bas, cmd);
		break;
	case UART_IOCTL_OFLOW:
		cmd = i8251->cmd;
		if (data)
			cmd |= CMD8251_TxEN;
		else
			cmd &= ~CMD8251_TxEN;
		i8251_write_cmd(bas, cmd);
		break;
	case UART_IOCTL_BAUD:
		baudrate = i8251_get_baudrate(bas);
		if (baudrate > 0)
			*(int *)data = baudrate;
		else
			error = ENXIO;
		break;
	default:
		error = EINVAL;
		break;
	}
	uart_unlock(sc->sc_hwmtx);

	return (error);
}

static int
i8251_bus_ipend(struct uart_softc *sc)
{
	struct i8251_softc *i8251;
	struct uart_bas *bas;
	int ipend;
	u_int8_t iir, lsr;

	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;

	ipend = 0;

	uart_lock(sc->sc_hwmtx);

	if (GET_SUBTYPE(bas) & COM_SUB_I8251F) {
		iir = uart_getreg(bas, serf_iir);
		if (iir & IIR_NOPEND) {
			uart_unlock(sc->sc_hwmtx);
			return (0);
		}

		lsr = uart_getreg(bas, serf_lsr);
		if (lsr & FLSR_RCV_ERR) {
			(void)uart_getreg(bas, serf_data);
			i8251_write_cmd(bas, i8251->cmd | CMD8251_ER);
		}
		if (lsr & FLSR_OE)
			ipend |= SER_INT_OVERRUN;
		if (lsr & FLSR_BI)
			ipend |= SER_INT_BREAK;
		if (lsr & FLSR_RxRDY)
			ipend |= SER_INT_RXREADY;
		if ((lsr & FLSR_TxRDY) && sc->sc_txbusy)
			ipend |= SER_INT_TXIDLE;
	} else {
		lsr = uart_getreg(bas, seri_lsr);
		if (lsr & STS8251_RCV_ERR) {
			(void)uart_getreg(bas, seri_data);
			uart_setreg(bas, seri_cmd, i8251->cmd | CMD8251_ER);
		}
		if (lsr & STS8251_OE)
			ipend |= SER_INT_OVERRUN;
		if (lsr & STS8251_BI)
			ipend |= SER_INT_BREAK;
		if (lsr & STS8251_RxRDY)
			ipend |= SER_INT_RXREADY;
		if ((lsr & STS8251_TxRDY) && sc->sc_txbusy)
			ipend |= SER_INT_TXIDLE;
	}
	if (ipend == 0)
		i8251_clrint(bas);
	uart_unlock(sc->sc_hwmtx);

	return ((sc->sc_leaving) ? 0 : ipend);
}

static int
i8251_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{
	struct uart_bas *bas;
	int error;

	bas = &sc->sc_bas;

	uart_lock(sc->sc_hwmtx);
	error = i8251_param(bas, baudrate, databits, stopbits, parity);
	uart_unlock(sc->sc_hwmtx);

	return (error);
}

static int
i8251_bus_probe(struct uart_softc *sc)
{
	struct i8251_softc *i8251;
	struct uart_bas *bas;
	int type, error;
	char buf[80];

	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;
	type = GET_IFTYPE(bas);

	if (sc->sc_sysdev != NULL)
		goto describe;

	error = i8251_probe(bas);
	if (error)
		return (error);

	/* By using i8251_init() we also set DTR and RTS. */
	i8251_init(bas, 9600, 8, 1, UART_PARITY_NONE);

describe:
	snprintf(buf, sizeof(buf), "8251 (%s%s%s)", i8251_hw[type].desc,
	    GET_SUBTYPE(bas) & COM_SUB_I8251F ? " + FIFO" : "",
	    GET_SUBTYPE(bas) & COM_SUB_I8251VFAST ? " + V-FAST" : "");
	device_set_desc_copy(sc->sc_dev, buf);

	sc->sc_rxfifosz = 1;
	sc->sc_txfifosz = 1;

	switch (GET_IFTYPE(bas)) {
	case COM_IF_INTERNAL:
		if (GET_SUBTYPE(bas) & COM_SUB_I8251F) {
			sc->sc_rxfifosz = 16;
			sc->sc_txfifosz = 16;
		}
		break;
	case COM_IF_IND_SS_1:
	case COM_IF_IND_SS_2:
		sc->sc_rxfifosz = 400;
		break;
	}

	return (0);
}

static int
i8251_bus_receive(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int xc;
	u_int8_t lsr;

	bas = &sc->sc_bas;

	uart_lock(sc->sc_hwmtx);
	lsr = i8251_read_lsr(bas);
	while (lsr & STS8251_RxRDY) {
		if (uart_rx_full(sc)) {
			sc->sc_rxbuf[sc->sc_rxput] = UART_STAT_OVERRUN;
			break;
		}
		xc = i8251_data_getc(bas);
		if (lsr & STS8251_FE)
			xc |= UART_STAT_FRAMERR;
		if (lsr & STS8251_PE)
			xc |= UART_STAT_PARERR;
		uart_rx_put(sc, xc);
		lsr = i8251_read_lsr(bas);
	}
	/* Discard everything left in the Rx FIFO. */
	while (lsr & STS8251_RxRDY) {
		(void)i8251_data_getc(bas);
		lsr = i8251_read_lsr(bas);
	}
	uart_unlock(sc->sc_hwmtx);

	return (0);
}

static int
i8251_bus_setsig(struct uart_softc *sc, int sig)
{
	struct i8251_softc *i8251;
	struct uart_bas *bas;
	u_int32_t new, old;

	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;

	do {
		old = sc->sc_hwsig;
		new = old;
		if (sig & SER_DDTR) {
			SIGCHG(sig & SER_DTR, new, SER_DTR, SER_DDTR);
		}
		if (sig & SER_DRTS) {
			SIGCHG(sig & SER_RTS, new, SER_RTS, SER_DRTS);
		}
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));

	uart_lock(sc->sc_hwmtx);
	i8251->cmd &= ~(CMD8251_DTR | CMD8251_RTS);
	if (new & SER_DTR)
		i8251->cmd |= CMD8251_DTR;
	if (new & SER_RTS)
		i8251->cmd |= CMD8251_RTS;
	i8251_write_cmd(bas, i8251->cmd);
	uart_unlock(sc->sc_hwmtx);

	return (0);
}

static int
i8251_bus_transmit(struct uart_softc *sc)
{
	struct i8251_softc *i8251;
	struct uart_bas *bas;
	int i;
	int limit = 10;

	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;

	uart_lock(sc->sc_hwmtx);
	while (i8251_check_txready(bas) == 0 && --limit)
		;
	i8251->icr |= IEN_Tx;
	i8251_set_icr(bas, i8251->icr);
	for (i = 0; i < sc->sc_txdatasz; i++)
		i8251_data_putc(bas, sc->sc_txbuf[i]);
	sc->sc_txbusy = 1;
	uart_unlock(sc->sc_hwmtx);

	return (0);
}


static void
i8251_msrintr(void *arg)
{
	struct uart_softc *sc;
	struct i8251_softc *i8251;
	struct uart_bas *bas;
	u_int8_t msr;

	sc = (struct uart_softc *)arg;
	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;

	uart_lock(sc->sc_hwmtx);
	msr = i8251_read_msr(bas);
	uart_unlock(sc->sc_hwmtx);

	if (i8251->msr ^ msr) {
		i8251->msr = msr;
		uart_bus_ihand(sc->sc_dev, SER_INT_SIGCHG)(sc);
	}

	i8251->timeout_msr = timeout(i8251_msrintr, sc, hz / 10);
}

static void
i8251_enable_msrintr(struct uart_softc *sc)
{
	struct i8251_softc *i8251;
	struct uart_bas *bas;

	i8251 = (struct i8251_softc *)sc;
	bas = &sc->sc_bas;

	i8251->msr = i8251_read_msr(bas);
	i8251->timeout_msr = timeout(i8251_msrintr, sc, hz / 10);
}

static void
i8251_disable_msrintr(struct uart_softc *sc)
{
	struct i8251_softc *i8251;

	i8251 = (struct i8251_softc *)sc;

	untimeout(i8251_msrintr, sc, i8251->timeout_msr);
}
