/*
 * Copyright (c) 2002-2003 Luigi Rizzo
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * NEW command line interface for IP firewall facility
 *
 * $FreeBSD: head/sbin/ipfw/ipfw2.c 187716 2009-01-26 14:26:35Z luigi $
 */

/* main header for ipfw2 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <timeconv.h>	/* _long_to_time */
#include <unistd.h>
#include <fcntl.h>

#define IPFW_INTERNAL	/* Access to protected structures in ip_fw.h. */

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/pfvar.h>
#include <net/route.h> /* def. of struct route */
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <alias.h>

extern int
		do_value_as_ip,		/* show table value as IP */
		do_resolv,		/* Would try to resolve all */
		do_time,		/* Show time stamps */
		do_quiet,		/* Be quiet in add and flush */
		do_pipe,		/* this cmd refers to a pipe */
	        do_nat, 		/* Nat configuration. */
		do_sort,		/* field to sort results (0 = no) */
		do_dynamic,		/* display dynamic rules */
		do_expired,		/* display expired dynamic rules */
		do_compact,		/* show rules in compact mode */
		do_force,		/* do not ask for confirmation */
		use_set,		/* work with specified set number */
		show_sets,		/* display rule sets */
		test_only,		/* only check syntax */
		comment_only,		/* only print action and comment */
		verbose;

#define	IP_MASK_ALL	0xffffffff
/*
 * the following macro returns an error message if we run out of
 * arguments.
 */
#define NEED1(msg)      {if (!ac) errx(EX_USAGE, msg);}

#define GET_UINT_ARG(arg, min, max, tok, s_x) do {			\
	if (!ac)							\
		errx(EX_USAGE, "%s: missing argument", match_value(s_x, tok)); \
	if (_substrcmp(*av, "tablearg") == 0) {				\
		arg = IP_FW_TABLEARG;					\
		break;							\
	}								\
									\
	{								\
	long val;							\
	char *end;							\
									\
	val = strtol(*av, &end, 10);					\
									\
	if (!isdigit(**av) || *end != '\0' || (val == 0 && errno == EINVAL)) \
		errx(EX_DATAERR, "%s: invalid argument: %s",		\
		    match_value(s_x, tok), *av);			\
									\
	if (errno == ERANGE || val < min || val > max)			\
		errx(EX_DATAERR, "%s: argument is out of range (%u..%u): %s", \
		    match_value(s_x, tok), min, max, *av);		\
									\
	if (val == IP_FW_TABLEARG)					\
		errx(EX_DATAERR, "%s: illegal argument value: %s",	\
		    match_value(s_x, tok), *av);			\
	arg = val;							\
	}								\
} while (0)

#define PRINT_UINT_ARG(str, arg) do {					\
	if (str != NULL)						\
		printf("%s",str);					\
	if (arg == IP_FW_TABLEARG)					\
		printf("tablearg");					\
	else								\
		printf("%u", (uint32_t)arg);				\
} while (0)

/*
 * _s_x is a structure that stores a string <-> token pairs, used in
 * various places in the parser. Entries are stored in arrays,
 * with an entry with s=NULL as terminator.
 * The search routines are match_token() and match_value().
 * Often, an element with x=0 contains an error string.
 *
 */
struct _s_x {
	char const *s;
	int x;
};

extern struct _s_x f_tcpflags[];
extern struct _s_x f_tcpopts[];
extern struct _s_x f_ipopts[];
extern struct _s_x f_iptos[];
extern struct _s_x limit_masks[];
//extern struct _s_x icmp6codes[];

/*
 * we use IPPROTO_ETHERTYPE as a fake protocol id to call the print routines
 * This is only used in this code.
 */
#define IPPROTO_ETHERTYPE	0x1000
extern struct _s_x ether_types[];

enum tokens {
	TOK_NULL=0,

	TOK_OR,
	TOK_NOT,
	TOK_STARTBRACE,
	TOK_ENDBRACE,

	TOK_ACCEPT,
	TOK_COUNT,
	TOK_PIPE,
	TOK_QUEUE,
	TOK_DIVERT,
	TOK_TEE,
	TOK_NETGRAPH,
	TOK_NGTEE,
	TOK_FORWARD,
	TOK_SKIPTO,
	TOK_DENY,
	TOK_REJECT,
	TOK_RESET,
	TOK_UNREACH,
	TOK_CHECKSTATE,
	TOK_NAT,

	TOK_ALTQ,
	TOK_LOG,
	TOK_TAG,
	TOK_UNTAG,

	TOK_TAGGED,
	TOK_UID,
	TOK_GID,
	TOK_JAIL,
	TOK_IN,
	TOK_LIMIT,
	TOK_KEEPSTATE,
	TOK_LAYER2,
	TOK_OUT,
	TOK_DIVERTED,
	TOK_DIVERTEDLOOPBACK,
	TOK_DIVERTEDOUTPUT,
	TOK_XMIT,
	TOK_RECV,
	TOK_VIA,
	TOK_FRAG,
	TOK_IPOPTS,
	TOK_IPLEN,
	TOK_IPID,
	TOK_IPPRECEDENCE,
	TOK_IPTOS,
	TOK_IPTTL,
	TOK_IPVER,
	TOK_ESTAB,
	TOK_SETUP,
	TOK_TCPDATALEN,
	TOK_TCPFLAGS,
	TOK_TCPOPTS,
	TOK_TCPSEQ,
	TOK_TCPACK,
	TOK_TCPWIN,
	TOK_ICMPTYPES,
	TOK_MAC,
	TOK_MACTYPE,
	TOK_VERREVPATH,
	TOK_VERSRCREACH,
	TOK_ANTISPOOF,
	TOK_IPSEC,
	TOK_COMMENT,

	TOK_PLR,
	TOK_NOERROR,
	TOK_BUCKETS,
	TOK_DSTIP,
	TOK_SRCIP,
	TOK_DSTPORT,
	TOK_SRCPORT,
	TOK_ALL,
	TOK_MASK,
	TOK_BW,
	TOK_DELAY,
	TOK_RED,
	TOK_GRED,
	TOK_DROPTAIL,
	TOK_PROTO,
	TOK_WEIGHT,
	TOK_IP,
	TOK_IF,
 	TOK_ALOG,
 	TOK_DENY_INC,
 	TOK_SAME_PORTS,
 	TOK_UNREG_ONLY,
 	TOK_RESET_ADDR,
 	TOK_ALIAS_REV,
 	TOK_PROXY_ONLY,
	TOK_REDIR_ADDR,
	TOK_REDIR_PORT,
	TOK_REDIR_PROTO,	

	TOK_IPV6,
	TOK_FLOWID,
	TOK_ICMP6TYPES,
	TOK_EXT6HDR,
	TOK_DSTIP6,
	TOK_SRCIP6,

	TOK_IPV4,
	TOK_UNREACH6,
	TOK_RESET6,

	TOK_FIB,
	TOK_SETFIB,
};

extern struct _s_x dummynet_params[];
extern struct _s_x nat_params[];
//extern struct _s_x rule_actions[];
extern struct _s_x rule_action_params[];
extern struct _s_x rule_options[];
extern struct _s_x icmpcodes[];

#define	TABLEARG	"tablearg"

static __inline uint64_t
align_uint64(uint64_t *pll) {
	uint64_t ret;

	bcopy (pll, &ret, sizeof(ret));
	return ret;
}

void *safe_calloc(size_t number, size_t size);
void *safe_realloc(void *ptr, size_t size);

int match_token(struct _s_x *table, char *string);
char const * match_value(struct _s_x *p, int value);

int do_cmd(int optname, void *optval, uintptr_t optlen);

int _substrcmp(const char *str1, const char* str2);
int lookup_host (char *host, struct in_addr *ipaddr);

void show_ipfw(struct ip_fw *rule, int pcwidth, int bcwidth);

/* table.c */
//void table_handler(int ac, char *av[]);

/* compile.c */
void add(int ac, char *av[]);
int contigmask(uint8_t *p, int len);
void n2mask(struct in6_addr *mask, int n);


/* ipv6.v */
void fill_unreach6_code(u_short *codep, char *str);
void print_unreach6_code(uint16_t code);

