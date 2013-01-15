#include <sys/types.h>
#include <stdint.h>
#include <stand.h>

#include "pxe_telnet.h"
#include "telnet_fsm.h"

extern uint8_t	doecho;		/* nonzero, if remote ECHO	*/
extern uint8_t	rcvbinary;	/* non-zero if remote TRANSMIT-BINARY	*/
extern int	substate;
extern uint8_t	sndbinary;	/* non-zero if TRANSMIT-BINARY		*/
extern uint8_t	termtype;	/* non-zero if received "DO TERMTYPE"	*/
extern uint8_t	noga;

int
is_already(uint8_t val, uint8_t opt_cmd) 
{

	if (val) {
		if (opt_cmd == TCWILL)
			return (1);	/* already doing needed option */
	} else if (opt_cmd == TCWONT)
		return (1);		/* already NOT doing needed option */
		
	return (0);
}

void
do_answer(uint8_t val, uint8_t c, int socket)
{

        putc(TCIAC, socket);
	putc(val ? TCDO : TCDONT, socket);
	putc(c, socket);
}

/* do_echo() - processes ECHO option
 * in:
 *	socket	- socket to place data to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
do_echo(int socket, int c)
{

	if (is_already(doecho, fsm_get_option_cmd()))
		return (0);

	doecho = !doecho;
        do_answer(doecho, (uint8_t)c, socket);

	return (0);
}

/* do_noga() - process "no go-ahead" option
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
do_noga(int socket, int c)
{

	if (is_already(noga, fsm_get_option_cmd()))
		return (0);

	noga = !noga;
	do_answer(noga, (uint8_t)c, socket);
	
	return (0);
}

/* do_notsup() - process not supported will/won't option
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
do_notsup(int socket, int c)
{

	do_answer(0, (uint8_t)c, socket);
	return (0);
}


/* do_txbinary() - process transmit binary option
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
do_txbinary(int socket, int c)
{

	if (is_already(rcvbinary, fsm_get_option_cmd()))
		return (0);
		
	rcvbinary = !rcvbinary;
	do_answer(rcvbinary, (uint8_t)c, socket);	
	
	return (0);
}

/* recopt() - process option type
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
recopt(int socket, int c)
{

	fsm_set_option_cmd((uint8_t)c);
	
	return (0);
}

/* no_op() - don't do anything
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
no_op(int socket, int c)
{

	return (0);
}


/* subend() - end of option subnegotiation
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
subend(int socket, int c)
{

	return fsm_setstate(FSM_SUBS, SS_START);
}

/* subopt() - do option subnegotiation FSM state change
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
subopt(int socket, int c)
{

	return fsm_process(FSM_SUBS, socket, c);
}


/* subtermtype() - terminal type subnegotiation
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
subtermtype(int socket, int c)
{

	putc(TCIAC, socket);
	putc(TCSB, socket);
	putc(TOTERMTYPE, socket);
	putc(TT_IS, socket);
	fputs("pxe_term", socket);
	putc(TCIAC, socket);
	putc(TCSE, socket);
	
	return (0);
}

/* will_notsup() - process not supported do/don't option
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
will_notsup(int socket, int c)
{

	putc(TCIAC, socket);
	putc(TCWONT, socket);
	putc((uint8_t)c, socket);
	
	return (0);
}

/* will_txbinary() - process transmit binary option
 * in:
 *	socket	- socket to place response to
 *	c	- provided command
 * out:
 *	0	- ok
 *	-1	- problems
 */
int
will_txbinary(int socket, int c)
{

	if (is_already(sndbinary, fsm_get_option_cmd()))
		return (0);
		
	sndbinary = !sndbinary;

	putc(TCIAC, socket);
	putc(sndbinary ? TCWILL : TCWONT, socket);
	putc((char)c, socket);
	
	return (0);
}
