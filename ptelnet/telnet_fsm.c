#include "pxe_telnet.h"
#include "telnet_fsm.h"
#include "telnet_opts.h"

uint8_t option_cmd = 0;

int     substate = 0;
uint8_t subfsm[NSSTATES][NCHRS];

int	ttstate = 0;
uint8_t	ttfsm[NTSTATES][NCHRS];

int     sostate = 0;
uint8_t sofsm[NKSTATES][NCHRS];

#define TINVALID        0xff    /* an invalid transition index          */

struct fsm_trans ttstab[] = {
	/* State	Input		Next State	Action	*/
	/* ------	------		-----------	-------	*/
	{ TSDATA,	TCIAC,		TSIAC,		no_op		},
	{ TSDATA,	TCANY,		TSDATA,		ttputc 		},
	{ TSIAC,	TCIAC,		TSDATA,		ttputc		},
	{ TSIAC,	TCSB,		TSSUBNEG,	no_op		},
/* Telnet Commands */
	{ TSIAC,	TCNOP,		TSDATA,		no_op		},
//	{ TSIAC,	TCDM,		TSDATA,		tcdm		},
/* Option Negotiation */
	{ TSIAC,	TCWILL,		TSWOPT,		recopt		},
	{ TSIAC,	TCWONT,		TSWOPT,		recopt		},
	{ TSIAC,	TCDO,		TSDOPT,		recopt		},
	{ TSIAC,	TCDONT,		TSDOPT,		recopt		},
	{ TSIAC,	TCANY,		TSDATA,		no_op		},
/* Option Subnegotion */
	{ TSSUBNEG,	TCIAC,		TSSUBIAC,	no_op		},
	{ TSSUBNEG,	TCANY,		TSSUBNEG,	subopt		},
	{ TSSUBIAC,	TCSE,		TSDATA,		subend		},
	{ TSSUBIAC,	TCANY,		TSSUBNEG,	subopt		},

	{ TSWOPT,	TOECHO,		TSDATA,		do_echo		},
	{ TSWOPT,	TONOGA,		TSDATA,		do_noga		},
	{ TSWOPT,	TOTXBINARY,	TSDATA,		do_txbinary	},
	{ TSWOPT,	TCANY,		TSDATA,		do_notsup	},

	{ TSDOPT,	TOTXBINARY,	TSDATA,		will_txbinary	},
	{ TSDOPT,	TCANY,		TSDATA,		will_notsup	},

	{ FSINVALID,	TCANY,		FSINVALID,	abort		},
};

struct fsm_trans sostab[] = {
	/* State        Input           Next State      Action  */
	/* ------       ------          -----------     ------- */
	/* Data Input */
	{ KSREMOTE,     KCESCAPE,       KSLOCAL,        no_op           },
	{ KSREMOTE,     KCANY,          KSREMOTE,       soputc          },

	/* Local Escape Commands */
	{ KSLOCAL,      KCESCAPE,       KSREMOTE,       soputc          },
	{ KSLOCAL,      KCDCON,         KSREMOTE,       disconnect      },
	{ KSLOCAL,      KCANY,          KSREMOTE,       sonotsup        },

	{ FSINVALID,    KCANY,          FSINVALID,      abort           },
};

struct fsm_trans substab[] = {
        /* State        Input           Next State      Action  */
	/* ------       ------          -----------     ------- */
	{ SS_START,     TOTERMTYPE,     SS_TERMTYPE,    no_op           },
	{ SS_START,     TCANY,          SS_END,         no_op           },
/*	{ SS_TERMTYPE,  TT_SEND,        SS_END,         subtermtype     },
	{ SS_TERMTYPE,  TCANY,          SS_END,         no_op           }, */
	{ SS_END,       TCANY,          SS_END,         no_op           },

	{ FSINVALID,    TCANY,          FSINVALID,      abort           },
};
								

/* fsmbuild()  build the Finite State Machine data structures
 * in/out:
 *	none
 * NOTE: may be better to use prebuilt structure, or not use matrix to
 *	reduce memory usage (~1kb).
 */
int
fsmbuild()
{

	fsminit(ttfsm, ttstab, NTSTATES);
	ttstate = TSDATA;
		   
	fsminit(sofsm, sostab, NKSTATES);
	sostate = KSREMOTE;
				   
	fsminit(subfsm, substab, NSSTATES);
	substate = SS_START;
}



/* fsminit() - inits Finite State Machine, actually performs build of matrix
 *	structure.
 * in:
 *	fsm	- storage for neded matrix
 *	ttab	- FSM description structures
 *	nsatates - number of states
 * out:
 *	0	- all is ok
 *	-1 	- failed
 */
int
fsminit(uint8_t fsm[][NCHRS], struct fsm_trans  ttab[], int nstates)
{
	struct fsm_trans        *pt;
	int                     sn, ti, cn;
		   
	for (cn = 0; cn < NCHRS; ++cn)
	for (ti = 0; ti < nstates; ++ti)
		fsm[ti][cn] = TINVALID;
								   
	for (ti = 0; ttab[ti].ft_state != FSINVALID; ++ti) {
		pt = &ttab[ti];
		sn = pt->ft_state;
		
		if (pt->ft_char == TCANY) {
			for (cn = 0; cn < NCHRS; ++cn)
		    		if (fsm[sn][cn] == TINVALID)
					fsm[sn][cn] = ti;
		} else
        		fsm[sn][pt->ft_char] = ti;
	}
	
	/* set all uninitialized indices to an invalid transition */
	for (cn = 0; cn < NCHRS; ++cn)
	for (ti = 0; ti < nstates; ++ti)
		if (fsm[ti][cn] == TINVALID)
			fsm[ti][cn] = ti;
			
	return (0);
}

/* fsm_setstate() - sets state of FSM, choosen by index
 * in:
 *	table_index	- index of FSM (one of FSM_... constants)
 *	state		- state to set current state to
 * out:
 *	0	- all is ok
 *	-1 	- failed
 */
int
fsm_setstate(int table_index, int state)
{
	int	*state = NULL;

	switch(table_index) {
	case FSM_TERMINAL:
		state = &ttstate;
		break;
		
	case FSM_SOCKET:
		state = &sostate;
		break;
		
	case FSM_SUBS:
		state = &substate;
		break;
		
	default:
		return (-1);
	}

	*state = state;	
	
	return (0);
}

/* fsm_process() - process FSM state change if any
 * in:
 *	table_index	- index of FSM (one of FSM_... constants)
 *	c		- incoming char to process
 *	socket		- socket to write to
 * out:
 *	0	- all is ok
 *	-1 	- failed
 */
int
fsm_process(int table_index, int socket, int c)
{
	uint8_t 		*fsm = NULL;
	int			*state = NULL;
	struct fsm_trans	*table = NULL;
	int			ti = 0;
	struct fsm_trans	*pt = NULL;
	int			result = -1;
	
	switch(table_index) {
	case FSM_TERMINAL:
		fsm = ttfsm;
		state = &ttstate;
		table = ttstab;
		break;
		
	case FSM_SOCKET:
		fsm = sofsm;
		state = &sostate;
		table = sostab;
		break;
		
	case FSM_SUBS:
		fsm = subfsm;
		state = &substate;
		table = substab;
		break;
		
	default:
		return (-1);
	}
	
	ti = fsm[ (*state) * NCHRS + c];
	*pt = &table[ti];
	
	result =(pt->ft_action)(socket, c);
	
	if (result >= 0)
		*state = pt->ft_next;
						
	return (result);
}

/* fsm_get_option_cmd() - return option_cmd
 * in:
 *	none
 * out:
 *	option_cmd (WILL/WONT & etc)
 */
uint8_t
fsm_get_option_cmd()
{

	return (option_cmd);
}

/* fsm_set_option_cmd() - set option_cmd to provided value
 * in:
 *	opt		- value to set option_cmd to
 * out:
 *	none
 */

void
fsm_set_option_cmd(uint8_t opt)
{

	option_cmd = opt;
}
