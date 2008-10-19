/*-
 * Copyright (c) 2008 TAKAHASHI Yoshihiro
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

#ifndef	_DEV_UART_UART_DEV_I8251_H_
#define	_DEV_UART_UART_DEV_I8251_H_

/*
 * Interface type
 */
#define	COM_IF_INTERNAL		0x00
#define	COM_IF_PC9861K_1	0x01
#define	COM_IF_PC9861K_2	0x02
#define	COM_IF_IND_SS_1		0x03
#define	COM_IF_IND_SS_2		0x04
#define	COM_IF_PIO9032B_1	0x05
#define	COM_IF_PIO9032B_2	0x06
#define	COM_IF_B98_01_1		0x07
#define	COM_IF_B98_01_2		0x08

#define	COM_SUB_I8251F		0x10
#define	COM_SUB_I8251VFAST	0x20

#define	GET_IFTYPE(bas)		((bas)->type & 0x0f)
#define	GET_SUBTYPE(bas)	((bas)->type & 0xf0)
#define	SET_TYPE(bas, t)	((bas)->type |= (t))


/*
 * Hardware specific table
 */
struct i8251_speedtab {
	int sp_speed;
	int sp_code;
};
struct i8251_hw_table {
	char *desc;
	bus_space_iat_t iat;
	bus_size_t iatsz;
	struct i8251_speedtab *sp_tab;
};


/*
 * Port index
 */
#define	seri_data	0
#define	seri_mod	1
#define	seri_cmd	2
#define	seri_lsr	3
#define	seri_msr	4
#define	seri_icr	5
#define	seri_div	6
#define	seri_func	7

#define	serf_data	8
#define	serf_lsr	9
#define	serf_msr	10
#define	serf_iir	11
#define	serf_fcr	12
#define	serf_div	13

#define	I8251_DEF_MODE	(MOD8251_8BITS | MOD8251_STOP2 | MOD8251_CLKx16)
#define	I8251_DEF_CMD	\
	(CMD8251_TxEN | CMD8251_RxEN | CMD8251_DTR | CMD8251_RTS)
#define	I8251F_DEF_FIFO	(FIFO_ENABLE | FIFO_TRIGGER_8)


/*
 * Debug
 */
#define	I8251_DISABLE_FIFO

#endif	/* _DEV_UART_UART_DEV_I8251_H_ */
