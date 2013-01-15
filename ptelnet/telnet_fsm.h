#ifndef __TELNET__FSM__
#define __TELNET__FSM__

#include <stdint.h>

/* Telnet Socket-Input FSM States: */
#define	TSDATA		 0	/* normal data processing		*/
#define	TSIAC		 1	/* have seen IAC			*/
#define	TSWOPT		 2	/* have seen IAC-{WILL/WONT}		*/
#define	TSDOPT		 3	/* have seen IAC-{DO/DONT}		*/
#define	TSSUBNEG	 4	/* have seen IAC-SB			*/
#define	TSSUBIAC	 5	/* have seen IAC-SB-...-IAC		*/

#define	NTSTATES	 6	/* # of TS* states			*/

/* Telnet Keyboard-Input FSM States: */
#define	KSREMOTE	 0	/* input goes to the socket		*/
#define	KSLOCAL		 1	/* input goes to a local func.		*/
#define	KSCOLLECT	 2	/* input is scripting-file name		*/

#define	NKSTATES	 2	/* # of KS* states			*/

/* Telnet Option Subnegotiation FSM States: */
#define SS_START	0	/* initial state			*/
#define	SS_TERMTYPE	1	/* TERMINAL_TYPE option subnegotiation	*/
#define	SS_END		2	/* state after all legal input		*/

#define	NSSTATES	3	/* # of SS_* states			*/

#define	FSINVALID	0xff	/* an invalid state number		*/

#define	NCHRS		256		/* number of valid characters	*/
#define	TCANY		(NCHRS+1)	/* match any character		*/

#define FSM_TERMINAL	0
#define FSM_SOCKET	1
#define FSM_SUBS	2

struct fsm_trans {
	uint8_t ft_state;		/* current state		*/
	int16_t	ft_char;		/* input character		*/
	uint8_t	ft_next;		/* next state			*/
	int	(*ft_action)();		/* action to take		*/
};

#endif