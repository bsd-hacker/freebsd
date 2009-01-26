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
 */

#include "ipfw2.h"

int
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

struct _s_x f_tcpflags[] = {
	{ "syn", TH_SYN },
	{ "fin", TH_FIN },
	{ "ack", TH_ACK },
	{ "psh", TH_PUSH },
	{ "rst", TH_RST },
	{ "urg", TH_URG },
	{ "tcp flag", 0 },
	{ NULL,	0 }
};

struct _s_x f_tcpopts[] = {
	{ "mss",	IP_FW_TCPOPT_MSS },
	{ "maxseg",	IP_FW_TCPOPT_MSS },
	{ "window",	IP_FW_TCPOPT_WINDOW },
	{ "sack",	IP_FW_TCPOPT_SACK },
	{ "ts",		IP_FW_TCPOPT_TS },
	{ "timestamp",	IP_FW_TCPOPT_TS },
	{ "cc",		IP_FW_TCPOPT_CC },
	{ "tcp option",	0 },
	{ NULL,	0 }
};

/*
 * IP options span the range 0 to 255 so we need to remap them
 * (though in fact only the low 5 bits are significant).
 */
struct _s_x f_ipopts[] = {
	{ "ssrr",	IP_FW_IPOPT_SSRR},
	{ "lsrr",	IP_FW_IPOPT_LSRR},
	{ "rr",		IP_FW_IPOPT_RR},
	{ "ts",		IP_FW_IPOPT_TS},
	{ "ip option",	0 },
	{ NULL,	0 }
};

struct _s_x f_iptos[] = {
	{ "lowdelay",	IPTOS_LOWDELAY},
	{ "throughput",	IPTOS_THROUGHPUT},
	{ "reliability", IPTOS_RELIABILITY},
	{ "mincost",	IPTOS_MINCOST},
	{ "congestion",	IPTOS_ECN_CE},
	{ "ecntransport", IPTOS_ECN_ECT0},
	{ "ip tos option", 0},
	{ NULL,	0 }
};

struct _s_x limit_masks[] = {
	{"all",		DYN_SRC_ADDR|DYN_SRC_PORT|DYN_DST_ADDR|DYN_DST_PORT},
	{"src-addr",	DYN_SRC_ADDR},
	{"src-port",	DYN_SRC_PORT},
	{"dst-addr",	DYN_DST_ADDR},
	{"dst-port",	DYN_DST_PORT},
	{NULL,		0}
};

/*
 * we use IPPROTO_ETHERTYPE as a fake protocol id to call the print routines
 * This is only used in this code.
 */
struct _s_x ether_types[] = {
    /*
     * Note, we cannot use "-:&/" in the names because they are field
     * separators in the type specifications. Also, we use s = NULL as
     * end-delimiter, because a type of 0 can be legal.
     */
	{ "ip",		0x0800 },
	{ "ipv4",	0x0800 },
	{ "ipv6",	0x86dd },
	{ "arp",	0x0806 },
	{ "rarp",	0x8035 },
	{ "vlan",	0x8100 },
	{ "loop",	0x9000 },
	{ "trail",	0x1000 },
	{ "at",		0x809b },
	{ "atalk",	0x809b },
	{ "aarp",	0x80f3 },
	{ "pppoe_disc",	0x8863 },
	{ "pppoe_sess",	0x8864 },
	{ "ipx_8022",	0x00E0 },
	{ "ipx_8023",	0x0000 },
	{ "ipx_ii",	0x8137 },
	{ "ipx_snap",	0x8137 },
	{ "ipx",	0x8137 },
	{ "ns",		0x0600 },
	{ NULL,		0 }
};

static void show_usage(void);

struct _s_x rule_actions[] = {
	{ "accept",		TOK_ACCEPT },
	{ "pass",		TOK_ACCEPT },
	{ "allow",		TOK_ACCEPT },
	{ "permit",		TOK_ACCEPT },
	{ "count",		TOK_COUNT },
	{ "pipe",		TOK_PIPE },
	{ "queue",		TOK_QUEUE },
	{ "divert",		TOK_DIVERT },
	{ "tee",		TOK_TEE },
	{ "netgraph",		TOK_NETGRAPH },
	{ "ngtee",		TOK_NGTEE },
	{ "fwd",		TOK_FORWARD },
	{ "forward",		TOK_FORWARD },
	{ "skipto",		TOK_SKIPTO },
	{ "deny",		TOK_DENY },
	{ "drop",		TOK_DENY },
	{ "reject",		TOK_REJECT },
	{ "reset6",		TOK_RESET6 },
	{ "reset",		TOK_RESET },
	{ "unreach6",		TOK_UNREACH6 },
	{ "unreach",		TOK_UNREACH },
	{ "check-state",	TOK_CHECKSTATE },
	{ "//",			TOK_COMMENT },
	{ "nat",                TOK_NAT },
	{ "setfib",		TOK_SETFIB },
	{ NULL, 0 }	/* terminator */
};

struct _s_x rule_action_params[] = {
	{ "altq",		TOK_ALTQ },
	{ "log",		TOK_LOG },
	{ "tag",		TOK_TAG },
	{ "untag",		TOK_UNTAG },
	{ NULL, 0 }	/* terminator */
};

struct _s_x rule_options[] = {
	{ "tagged",		TOK_TAGGED },
	{ "uid",		TOK_UID },
	{ "gid",		TOK_GID },
	{ "jail",		TOK_JAIL },
	{ "in",			TOK_IN },
	{ "limit",		TOK_LIMIT },
	{ "keep-state",		TOK_KEEPSTATE },
	{ "bridged",		TOK_LAYER2 },
	{ "layer2",		TOK_LAYER2 },
	{ "out",		TOK_OUT },
	{ "diverted",		TOK_DIVERTED },
	{ "diverted-loopback",	TOK_DIVERTEDLOOPBACK },
	{ "diverted-output",	TOK_DIVERTEDOUTPUT },
	{ "xmit",		TOK_XMIT },
	{ "recv",		TOK_RECV },
	{ "via",		TOK_VIA },
	{ "fragment",		TOK_FRAG },
	{ "frag",		TOK_FRAG },
	{ "fib",		TOK_FIB },
	{ "ipoptions",		TOK_IPOPTS },
	{ "ipopts",		TOK_IPOPTS },
	{ "iplen",		TOK_IPLEN },
	{ "ipid",		TOK_IPID },
	{ "ipprecedence",	TOK_IPPRECEDENCE },
	{ "iptos",		TOK_IPTOS },
	{ "ipttl",		TOK_IPTTL },
	{ "ipversion",		TOK_IPVER },
	{ "ipver",		TOK_IPVER },
	{ "estab",		TOK_ESTAB },
	{ "established",	TOK_ESTAB },
	{ "setup",		TOK_SETUP },
	{ "tcpdatalen",		TOK_TCPDATALEN },
	{ "tcpflags",		TOK_TCPFLAGS },
	{ "tcpflgs",		TOK_TCPFLAGS },
	{ "tcpoptions",		TOK_TCPOPTS },
	{ "tcpopts",		TOK_TCPOPTS },
	{ "tcpseq",		TOK_TCPSEQ },
	{ "tcpack",		TOK_TCPACK },
	{ "tcpwin",		TOK_TCPWIN },
	{ "icmptype",		TOK_ICMPTYPES },
	{ "icmptypes",		TOK_ICMPTYPES },
	{ "dst-ip",		TOK_DSTIP },
	{ "src-ip",		TOK_SRCIP },
	{ "dst-port",		TOK_DSTPORT },
	{ "src-port",		TOK_SRCPORT },
	{ "proto",		TOK_PROTO },
	{ "MAC",		TOK_MAC },
	{ "mac",		TOK_MAC },
	{ "mac-type",		TOK_MACTYPE },
	{ "verrevpath",		TOK_VERREVPATH },
	{ "versrcreach",	TOK_VERSRCREACH },
	{ "antispoof",		TOK_ANTISPOOF },
	{ "ipsec",		TOK_IPSEC },
	{ "icmp6type",		TOK_ICMP6TYPES },
	{ "icmp6types",		TOK_ICMP6TYPES },
	{ "ext6hdr",		TOK_EXT6HDR},
	{ "flow-id",		TOK_FLOWID},
	{ "ipv6",		TOK_IPV6},
	{ "ip6",		TOK_IPV6},
	{ "ipv4",		TOK_IPV4},
	{ "ip4",		TOK_IPV4},
	{ "dst-ipv6",		TOK_DSTIP6},
	{ "dst-ip6",		TOK_DSTIP6},
	{ "src-ipv6",		TOK_SRCIP6},
	{ "src-ip6",		TOK_SRCIP6},
	{ "//",			TOK_COMMENT },

	{ "not",		TOK_NOT },		/* pseudo option */
	{ "!", /* escape ? */	TOK_NOT },		/* pseudo option */
	{ "or",			TOK_OR },		/* pseudo option */
	{ "|", /* escape */	TOK_OR },		/* pseudo option */
	{ "{",			TOK_STARTBRACE },	/* pseudo option */
	{ "(",			TOK_STARTBRACE },	/* pseudo option */
	{ "}",			TOK_ENDBRACE },		/* pseudo option */
	{ ")",			TOK_ENDBRACE },		/* pseudo option */
	{ NULL, 0 }	/* terminator */
};

void *
safe_calloc(size_t number, size_t size)
{
	void *ret = calloc(number, size);

	if (ret == NULL)
		err(EX_OSERR, "calloc");
	return ret;
}

void *
safe_realloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);

	if (ret == NULL)
		err(EX_OSERR, "realloc");
	return ret;
}

/*
 * conditionally runs the command.
 */
int
do_cmd(int optname, void *optval, uintptr_t optlen)
{
	static int s = -1;	/* the socket */
	int i;

	if (test_only)
		return 0;

	if (s == -1)
		s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (s < 0)
		err(EX_UNAVAILABLE, "socket");

	if (optname == IP_FW_GET || optname == IP_DUMMYNET_GET ||
	    optname == IP_FW_ADD || optname == IP_FW_TABLE_LIST ||
	    optname == IP_FW_TABLE_GETSIZE || 
	    optname == IP_FW_NAT_GET_CONFIG || 
	    optname == IP_FW_NAT_GET_LOG)
		i = getsockopt(s, IPPROTO_IP, optname, optval,
			(socklen_t *)optlen);
	else
		i = setsockopt(s, IPPROTO_IP, optname, optval, optlen);
	return i;
}

/**
 * match_token takes a table and a string, returns the value associated
 * with the string (-1 in case of failure).
 */
int
match_token(struct _s_x *table, char *string)
{
	struct _s_x *pt;
	uint i = strlen(string);

	for (pt = table ; i && pt->s != NULL ; pt++)
		if (strlen(pt->s) == i && !bcmp(string, pt->s, i))
			return pt->x;
	return -1;
}

/**
 * match_value takes a table and a value, returns the string associated
 * with the value (NULL in case of failure).
 */
char const *
match_value(struct _s_x *p, int value)
{
	for (; p->s != NULL; p++)
		if (p->x == value)
			return p->s;
	return NULL;
}

/*
 * _substrcmp takes two strings and returns 1 if they do not match,
 * and 0 if they match exactly or the first string is a sub-string
 * of the second.  A warning is printed to stderr in the case that the
 * first string is a sub-string of the second.
 *
 * This function will be removed in the future through the usual
 * deprecation process.
 */
int
_substrcmp(const char *str1, const char* str2)
{
	
	if (strncmp(str1, str2, strlen(str1)) != 0)
		return 1;

	if (strlen(str1) != strlen(str2))
		warnx("DEPRECATED: '%s' matched '%s' as a sub-string",
		    str1, str2);
	return 0;
}

/*
 * _substrcmp2 takes three strings and returns 1 if the first two do not match,
 * and 0 if they match exactly or the second string is a sub-string
 * of the first.  A warning is printed to stderr in the case that the
 * first string does not match the third.
 *
 * This function exists to warn about the bizzare construction
 * strncmp(str, "by", 2) which is used to allow people to use a shotcut
 * for "bytes".  The problem is that in addition to accepting "by",
 * "byt", "byte", and "bytes", it also excepts "by_rabid_dogs" and any
 * other string beginning with "by".
 *
 * This function will be removed in the future through the usual
 * deprecation process.
 */
static int
_substrcmp2(const char *str1, const char* str2, const char* str3)
{
	
	if (strncmp(str1, str2, strlen(str2)) != 0)
		return 1;

	if (strcmp(str1, str3) != 0)
		warnx("DEPRECATED: '%s' matched '%s'",
		    str1, str3);
	return 0;
}

struct _s_x _port_name[] = {
	{"dst-port",	O_IP_DSTPORT},
	{"src-port",	O_IP_SRCPORT},
	{"ipid",	O_IPID},
	{"iplen",	O_IPLEN},
	{"ipttl",	O_IPTTL},
	{"mac-type",	O_MAC_TYPE},
	{"tcpdatalen",	O_TCPDATALEN},
	{"tagged",	O_TAGGED},
	{NULL,		0}
};

struct _s_x icmpcodes[] = {
      { "net",			ICMP_UNREACH_NET },
      { "host",			ICMP_UNREACH_HOST },
      { "protocol",		ICMP_UNREACH_PROTOCOL },
      { "port",			ICMP_UNREACH_PORT },
      { "needfrag",		ICMP_UNREACH_NEEDFRAG },
      { "srcfail",		ICMP_UNREACH_SRCFAIL },
      { "net-unknown",		ICMP_UNREACH_NET_UNKNOWN },
      { "host-unknown",		ICMP_UNREACH_HOST_UNKNOWN },
      { "isolated",		ICMP_UNREACH_ISOLATED },
      { "net-prohib",		ICMP_UNREACH_NET_PROHIB },
      { "host-prohib",		ICMP_UNREACH_HOST_PROHIB },
      { "tosnet",		ICMP_UNREACH_TOSNET },
      { "toshost",		ICMP_UNREACH_TOSHOST },
      { "filter-prohib",	ICMP_UNREACH_FILTER_PROHIB },
      { "host-precedence",	ICMP_UNREACH_HOST_PRECEDENCE },
      { "precedence-cutoff",	ICMP_UNREACH_PRECEDENCE_CUTOFF },
      { NULL, 0 }
};

struct _s_x icmp6codes[] = {
      { "no-route",		ICMP6_DST_UNREACH_NOROUTE },
      { "admin-prohib",		ICMP6_DST_UNREACH_ADMIN },
      { "address",		ICMP6_DST_UNREACH_ADDR },
      { "port",			ICMP6_DST_UNREACH_NOPORT },
      { NULL, 0 }
};

/*
 * This one handles all set-related commands
 * 	ipfw set { show | enable | disable }
 * 	ipfw set swap X Y
 * 	ipfw set move X to Y
 * 	ipfw set move rule X to Y
 */
static void
sets_handler(int ac, char *av[])
{
	uint32_t set_disable, masks[2];
	int i, nbytes;
	uint16_t rulenum;
	uint8_t cmd, new_set;

	ac--;
	av++;

	if (!ac)
		errx(EX_USAGE, "set needs command");
	if (_substrcmp(*av, "show") == 0) {
		void *data;
		char const *msg;

		nbytes = sizeof(struct ip_fw);
		data = safe_calloc(1, nbytes);
		if (do_cmd(IP_FW_GET, data, (uintptr_t)&nbytes) < 0)
			err(EX_OSERR, "getsockopt(IP_FW_GET)");
		bcopy(&((struct ip_fw *)data)->next_rule,
			&set_disable, sizeof(set_disable));

		for (i = 0, msg = "disable" ; i < RESVD_SET; i++)
			if ((set_disable & (1<<i))) {
				printf("%s %d", msg, i);
				msg = "";
			}
		msg = (set_disable) ? " enable" : "enable";
		for (i = 0; i < RESVD_SET; i++)
			if (!(set_disable & (1<<i))) {
				printf("%s %d", msg, i);
				msg = "";
			}
		printf("\n");
	} else if (_substrcmp(*av, "swap") == 0) {
		ac--; av++;
		if (ac != 2)
			errx(EX_USAGE, "set swap needs 2 set numbers\n");
		rulenum = atoi(av[0]);
		new_set = atoi(av[1]);
		if (!isdigit(*(av[0])) || rulenum > RESVD_SET)
			errx(EX_DATAERR, "invalid set number %s\n", av[0]);
		if (!isdigit(*(av[1])) || new_set > RESVD_SET)
			errx(EX_DATAERR, "invalid set number %s\n", av[1]);
		masks[0] = (4 << 24) | (new_set << 16) | (rulenum);
		i = do_cmd(IP_FW_DEL, masks, sizeof(uint32_t));
	} else if (_substrcmp(*av, "move") == 0) {
		ac--; av++;
		if (ac && _substrcmp(*av, "rule") == 0) {
			cmd = 2;
			ac--; av++;
		} else
			cmd = 3;
		if (ac != 3 || _substrcmp(av[1], "to") != 0)
			errx(EX_USAGE, "syntax: set move [rule] X to Y\n");
		rulenum = atoi(av[0]);
		new_set = atoi(av[2]);
		if (!isdigit(*(av[0])) || (cmd == 3 && rulenum > RESVD_SET) ||
			(cmd == 2 && rulenum == IPFW_DEFAULT_RULE) )
			errx(EX_DATAERR, "invalid source number %s\n", av[0]);
		if (!isdigit(*(av[2])) || new_set > RESVD_SET)
			errx(EX_DATAERR, "invalid dest. set %s\n", av[1]);
		masks[0] = (cmd << 24) | (new_set << 16) | (rulenum);
		i = do_cmd(IP_FW_DEL, masks, sizeof(uint32_t));
	} else if (_substrcmp(*av, "disable") == 0 ||
		   _substrcmp(*av, "enable") == 0 ) {
		int which = _substrcmp(*av, "enable") == 0 ? 1 : 0;

		ac--; av++;
		masks[0] = masks[1] = 0;

		while (ac) {
			if (isdigit(**av)) {
				i = atoi(*av);
				if (i < 0 || i > RESVD_SET)
					errx(EX_DATAERR,
					    "invalid set number %d\n", i);
				masks[which] |= (1<<i);
			} else if (_substrcmp(*av, "disable") == 0)
				which = 0;
			else if (_substrcmp(*av, "enable") == 0)
				which = 1;
			else
				errx(EX_DATAERR,
					"invalid set command %s\n", *av);
			av++; ac--;
		}
		if ( (masks[0] & masks[1]) != 0 )
			errx(EX_DATAERR,
			    "cannot enable and disable the same set\n");

		i = do_cmd(IP_FW_DEL, masks, sizeof(masks));
		if (i)
			warn("set enable/disable: setsockopt(IP_FW_DEL)");
	} else
		errx(EX_USAGE, "invalid set command %s\n", *av);
}

static void
sysctl_handler(int ac, char *av[], int which)
{
	ac--;
	av++;

	if (ac == 0) {
		warnx("missing keyword to enable/disable\n");
	} else if (_substrcmp(*av, "firewall") == 0) {
		sysctlbyname("net.inet.ip.fw.enable", NULL, 0,
		    &which, sizeof(which));
	} else if (_substrcmp(*av, "one_pass") == 0) {
		sysctlbyname("net.inet.ip.fw.one_pass", NULL, 0,
		    &which, sizeof(which));
	} else if (_substrcmp(*av, "debug") == 0) {
		sysctlbyname("net.inet.ip.fw.debug", NULL, 0,
		    &which, sizeof(which));
	} else if (_substrcmp(*av, "verbose") == 0) {
		sysctlbyname("net.inet.ip.fw.verbose", NULL, 0,
		    &which, sizeof(which));
	} else if (_substrcmp(*av, "dyn_keepalive") == 0) {
		sysctlbyname("net.inet.ip.fw.dyn_keepalive", NULL, 0,
		    &which, sizeof(which));
	} else if (_substrcmp(*av, "altq") == 0) {
		altq_set_enabled(which);
	} else {
		warnx("unrecognize enable/disable keyword: %s\n", *av);
	}
}

static void
show_usage(void)
{
	fprintf(stderr, "usage: ipfw [options]\n"
"do \"ipfw -h\" or see ipfw manpage for details\n"
);
	exit(EX_USAGE);
}

static void
help(void)
{
	fprintf(stderr,
"ipfw syntax summary (but please do read the ipfw(8) manpage):\n"
"ipfw [-abcdefhnNqStTv] <command> where <command> is one of:\n"
"add [num] [set N] [prob x] RULE-BODY\n"
"{pipe|queue} N config PIPE-BODY\n"
"[pipe|queue] {zero|delete|show} [N{,N}]\n"
"nat N config {ip IPADDR|if IFNAME|log|deny_in|same_ports|unreg_only|reset|\n"
"		reverse|proxy_only|redirect_addr linkspec|\n"
"		redirect_port linkspec|redirect_proto linkspec}\n"
"set [disable N... enable N...] | move [rule] X to Y | swap X Y | show\n"
"set N {show|list|zero|resetlog|delete} [N{,N}] | flush\n"
"table N {add ip[/bits] [value] | delete ip[/bits] | flush | list}\n"
"table all {flush | list}\n"
"\n"
"RULE-BODY:	check-state [PARAMS] | ACTION [PARAMS] ADDR [OPTION_LIST]\n"
"ACTION:	check-state | allow | count | deny | unreach{,6} CODE |\n"
"               skipto N | {divert|tee} PORT | forward ADDR |\n"
"               pipe N | queue N | nat N | setfib FIB\n"
"PARAMS: 	[log [logamount LOGLIMIT]] [altq QUEUE_NAME]\n"
"ADDR:		[ MAC dst src ether_type ] \n"
"		[ ip from IPADDR [ PORT ] to IPADDR [ PORTLIST ] ]\n"
"		[ ipv6|ip6 from IP6ADDR [ PORT ] to IP6ADDR [ PORTLIST ] ]\n"
"IPADDR:	[not] { any | me | ip/bits{x,y,z} | table(t[,v]) | IPLIST }\n"
"IP6ADDR:	[not] { any | me | me6 | ip6/bits | IP6LIST }\n"
"IP6LIST:	{ ip6 | ip6/bits }[,IP6LIST]\n"
"IPLIST:	{ ip | ip/bits | ip:mask }[,IPLIST]\n"
"OPTION_LIST:	OPTION [OPTION_LIST]\n"
"OPTION:	bridged | diverted | diverted-loopback | diverted-output |\n"
"	{dst-ip|src-ip} IPADDR | {dst-ip6|src-ip6|dst-ipv6|src-ipv6} IP6ADDR |\n"
"	{dst-port|src-port} LIST |\n"
"	estab | frag | {gid|uid} N | icmptypes LIST | in | out | ipid LIST |\n"
"	iplen LIST | ipoptions SPEC | ipprecedence | ipsec | iptos SPEC |\n"
"	ipttl LIST | ipversion VER | keep-state | layer2 | limit ... |\n"
"	icmp6types LIST | ext6hdr LIST | flow-id N[,N] | fib FIB |\n"
"	mac ... | mac-type LIST | proto LIST | {recv|xmit|via} {IF|IPADDR} |\n"
"	setup | {tcpack|tcpseq|tcpwin} NN | tcpflags SPEC | tcpoptions SPEC |\n"
"	tcpdatalen LIST | verrevpath | versrcreach | antispoof\n"
);
exit(0);
}


int
lookup_host (char *host, struct in_addr *ipaddr)
{
	struct hostent *he;

	if (!inet_aton(host, ipaddr)) {
		if ((he = gethostbyname(host)) == NULL)
			return(-1);
		*ipaddr = *(struct in_addr *)he->h_addr_list[0];
	}
	return(0);
}

/* n2mask sets n bits of the mask */
void
n2mask(struct in6_addr *mask, int n)
{
	static int	minimask[9] =
	    { 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
	u_char		*p;

	memset(mask, 0, sizeof(struct in6_addr));
	p = (u_char *) mask;
	for (; n > 0; p++, n -= 8) {
		if (n >= 8)
			*p = 0xff;
		else
			*p = minimask[n];
	}
	return;
}
 

static void
delete(int ac, char *av[])
{
	uint32_t rulenum;
	struct dn_pipe p;
	int i;
	int exitval = EX_OK;
	int do_set = 0;

	memset(&p, 0, sizeof p);

	av++; ac--;
	NEED1("missing rule specification");
	if (ac > 0 && _substrcmp(*av, "set") == 0) {
		/* Do not allow using the following syntax:
		 *	ipfw set N delete set M
		 */
		if (use_set)
			errx(EX_DATAERR, "invalid syntax");
		do_set = 1;	/* delete set */
		ac--; av++;
	}

	/* Rule number */
	while (ac && isdigit(**av)) {
		i = atoi(*av); av++; ac--;
		if (do_nat) {
			exitval = do_cmd(IP_FW_NAT_DEL, &i, sizeof i);
			if (exitval) {
				exitval = EX_UNAVAILABLE;
				warn("rule %u not available", i);
			}
 		} else if (do_pipe) {
			if (do_pipe == 1)
				p.pipe_nr = i;
			else
				p.fs.fs_nr = i;
			i = do_cmd(IP_DUMMYNET_DEL, &p, sizeof p);
			if (i) {
				exitval = 1;
				warn("rule %u: setsockopt(IP_DUMMYNET_DEL)",
				    do_pipe == 1 ? p.pipe_nr : p.fs.fs_nr);
			}
		} else {
			if (use_set)
				rulenum = (i & 0xffff) | (5 << 24) |
				    ((use_set - 1) << 16);
			else
			rulenum =  (i & 0xffff) | (do_set << 24);
			i = do_cmd(IP_FW_DEL, &rulenum, sizeof rulenum);
			if (i) {
				exitval = EX_UNAVAILABLE;
				warn("rule %u: setsockopt(IP_FW_DEL)",
				    rulenum);
			}
		}
	}
	if (exitval != EX_OK)
		exit(exitval);
}


/* 
 * Search for interface with name "ifn", and fill n accordingly:
 *
 * n->ip        ip address of interface "ifn"
 * n->if_name   copy of interface name "ifn"
 */
static void
set_addr_dynamic(const char *ifn, struct cfg_nat *n)
{
	size_t needed;
	int mib[6];
	char *buf, *lim, *next;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sin;
	int ifIndex, ifMTU;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;	
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;		
/*
 * Get interface data.
 */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) == -1)
		err(1, "iflist-sysctl-estimate");
	buf = safe_calloc(1, needed);
	if (sysctl(mib, 6, buf, &needed, NULL, 0) == -1)
		err(1, "iflist-sysctl-get");
	lim = buf + needed;
/*
 * Loop through interfaces until one with
 * given name is found. This is done to
 * find correct interface index for routing
 * message processing.
 */
	ifIndex	= 0;
	next = buf;
	while (next < lim) {
		ifm = (struct if_msghdr *)next;
		next += ifm->ifm_msglen;
		if (ifm->ifm_version != RTM_VERSION) {
			if (verbose)
				warnx("routing message version %d "
				    "not understood", ifm->ifm_version);
			continue;
		}
		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			if (strlen(ifn) == sdl->sdl_nlen &&
			    strncmp(ifn, sdl->sdl_data, sdl->sdl_nlen) == 0) {
				ifIndex = ifm->ifm_index;
				ifMTU = ifm->ifm_data.ifi_mtu;
				break;
			}
		}
	}
	if (!ifIndex)
		errx(1, "unknown interface name %s", ifn);
/*
 * Get interface address.
 */
	sin = NULL;
	while (next < lim) {
		ifam = (struct ifa_msghdr *)next;
		next += ifam->ifam_msglen;
		if (ifam->ifam_version != RTM_VERSION) {
			if (verbose)
				warnx("routing message version %d "
				    "not understood", ifam->ifam_version);
			continue;
		}
		if (ifam->ifam_type != RTM_NEWADDR)
			break;
		if (ifam->ifam_addrs & RTA_IFA) {
			int i;
			char *cp = (char *)(ifam + 1);

			for (i = 1; i < RTA_IFA; i <<= 1) {
				if (ifam->ifam_addrs & i)
					cp += SA_SIZE((struct sockaddr *)cp);
			}
			if (((struct sockaddr *)cp)->sa_family == AF_INET) {
				sin = (struct sockaddr_in *)cp;
				break;
			}
		}
	}
	if (sin == NULL)
		errx(1, "%s: cannot get interface address", ifn);

	n->ip = sin->sin_addr;
	strncpy(n->if_name, ifn, IF_NAMESIZE);

	free(buf);
}

static void
zero(int ac, char *av[], int optname /* IP_FW_ZERO or IP_FW_RESETLOG */)
{
	uint32_t arg, saved_arg;
	int failed = EX_OK;
	char const *name = optname == IP_FW_ZERO ?  "ZERO" : "RESETLOG";
	char const *errstr;

	av++; ac--;

	if (!ac) {
		/* clear all entries */
		if (do_cmd(optname, NULL, 0) < 0)
			err(EX_UNAVAILABLE, "setsockopt(IP_FW_%s)", name);
		if (!do_quiet)
			printf("%s.\n", optname == IP_FW_ZERO ?
			    "Accounting cleared":"Logging counts reset");

		return;
	}

	while (ac) {
		/* Rule number */
		if (isdigit(**av)) {
			arg = strtonum(*av, 0, 0xffff, &errstr);
			if (errstr)
				errx(EX_DATAERR,
				    "invalid rule number %s\n", *av);
			saved_arg = arg;
			if (use_set)
				arg |= (1 << 24) | ((use_set - 1) << 16);
			av++;
			ac--;
			if (do_cmd(optname, &arg, sizeof(arg))) {
				warn("rule %u: setsockopt(IP_FW_%s)",
				    saved_arg, name);
				failed = EX_UNAVAILABLE;
			} else if (!do_quiet)
				printf("Entry %d %s.\n", saved_arg,
				    optname == IP_FW_ZERO ?
					"cleared" : "logging count reset");
		} else {
			errx(EX_USAGE, "invalid rule number ``%s''", *av);
		}
	}
	if (failed != EX_OK)
		exit(failed);
}

static void
flush(int force)
{
	int cmd = do_pipe ? IP_DUMMYNET_FLUSH : IP_FW_FLUSH;

	if (!force && !do_quiet) { /* need to ask user */
		int c;

		printf("Are you sure? [yn] ");
		fflush(stdout);
		do {
			c = toupper(getc(stdin));
			while (c != '\n' && getc(stdin) != '\n')
				if (feof(stdin))
					return; /* and do not flush */
		} while (c != 'Y' && c != 'N');
		printf("\n");
		if (c == 'N')	/* user said no */
			return;
	}
	/* `ipfw set N flush` - is the same that `ipfw delete set N` */
	if (use_set) {
		uint32_t arg = ((use_set - 1) & 0xffff) | (1 << 24);
		if (do_cmd(IP_FW_DEL, &arg, sizeof(arg)) < 0)
			err(EX_UNAVAILABLE, "setsockopt(IP_FW_DEL)");
	} else if (do_cmd(cmd, NULL, 0) < 0)
		err(EX_UNAVAILABLE, "setsockopt(IP_%s_FLUSH)",
		    do_pipe ? "DUMMYNET" : "FW");
	if (!do_quiet)
		printf("Flushed all %s.\n", do_pipe ? "pipes" : "rules");
}

/*
 * Free a the (locally allocated) copy of command line arguments.
 */
static void
free_args(int ac, char **av)
{
	int i;

	for (i=0; i < ac; i++)
		free(av[i]);
	free(av);
}

static void table_list(ipfw_table_entry ent, int need_header);

/*
 * This one handles all table-related commands
 * 	ipfw table N add addr[/masklen] [value]
 * 	ipfw table N delete addr[/masklen]
 * 	ipfw table {N | all} flush
 * 	ipfw table {N | all} list
 */
static void
table_handler(int ac, char *av[])
{
	ipfw_table_entry ent;
	int do_add;
	int is_all;
	size_t len;
	char *p;
	uint32_t a;
	uint32_t tables_max;

	len = sizeof(tables_max);
	if (sysctlbyname("net.inet.ip.fw.tables_max", &tables_max, &len,
		NULL, 0) == -1) {
#ifdef IPFW_TABLES_MAX
		warn("Warn: Failed to get the max tables number via sysctl. "
		     "Using the compiled in defaults. \nThe reason was");
		tables_max = IPFW_TABLES_MAX;
#else
		errx(1, "Failed sysctlbyname(\"net.inet.ip.fw.tables_max\")");
#endif
	}

	ac--; av++;
	if (ac && isdigit(**av)) {
		ent.tbl = atoi(*av);
		is_all = 0;
		ac--; av++;
	} else if (ac && _substrcmp(*av, "all") == 0) {
		ent.tbl = 0;
		is_all = 1;
		ac--; av++;
	} else
		errx(EX_USAGE, "table number or 'all' keyword required");
	if (ent.tbl >= tables_max)
		errx(EX_USAGE, "The table number exceeds the maximum allowed "
			"value (%d)", tables_max - 1);
	NEED1("table needs command");
	if (is_all && _substrcmp(*av, "list") != 0
		   && _substrcmp(*av, "flush") != 0)
		errx(EX_USAGE, "table number required");

	if (_substrcmp(*av, "add") == 0 ||
	    _substrcmp(*av, "delete") == 0) {
		do_add = **av == 'a';
		ac--; av++;
		if (!ac)
			errx(EX_USAGE, "IP address required");
		p = strchr(*av, '/');
		if (p) {
			*p++ = '\0';
			ent.masklen = atoi(p);
			if (ent.masklen > 32)
				errx(EX_DATAERR, "bad width ``%s''", p);
		} else
			ent.masklen = 32;
		if (lookup_host(*av, (struct in_addr *)&ent.addr) != 0)
			errx(EX_NOHOST, "hostname ``%s'' unknown", *av);
		ac--; av++;
		if (do_add && ac) {
			unsigned int tval;
			/* isdigit is a bit of a hack here.. */
			if (strchr(*av, (int)'.') == NULL && isdigit(**av))  {
				ent.value = strtoul(*av, NULL, 0);
			} else {
		        	if (lookup_host(*av, (struct in_addr *)&tval) == 0) {
					/* The value must be stored in host order	 *
					 * so that the values < 65k can be distinguished */
		       			ent.value = ntohl(tval); 
				} else {
					errx(EX_NOHOST, "hostname ``%s'' unknown", *av);
				}
			}
		} else
			ent.value = 0;
		if (do_cmd(do_add ? IP_FW_TABLE_ADD : IP_FW_TABLE_DEL,
		    &ent, sizeof(ent)) < 0) {
			/* If running silent, don't bomb out on these errors. */
			if (!(do_quiet && (errno == (do_add ? EEXIST : ESRCH))))
				err(EX_OSERR, "setsockopt(IP_FW_TABLE_%s)",
				    do_add ? "ADD" : "DEL");
			/* In silent mode, react to a failed add by deleting */
			if (do_add) {
				do_cmd(IP_FW_TABLE_DEL, &ent, sizeof(ent));
				if (do_cmd(IP_FW_TABLE_ADD,
				    &ent, sizeof(ent)) < 0)
					err(EX_OSERR,
				            "setsockopt(IP_FW_TABLE_ADD)");
			}
		}
	} else if (_substrcmp(*av, "flush") == 0) {
		a = is_all ? tables_max : (ent.tbl + 1);
		do {
			if (do_cmd(IP_FW_TABLE_FLUSH, &ent.tbl,
			    sizeof(ent.tbl)) < 0)
				err(EX_OSERR, "setsockopt(IP_FW_TABLE_FLUSH)");
		} while (++ent.tbl < a);
	} else if (_substrcmp(*av, "list") == 0) {
		a = is_all ? tables_max : (ent.tbl + 1);
		do {
			table_list(ent, is_all);
		} while (++ent.tbl < a);
	} else
		errx(EX_USAGE, "invalid table command %s", *av);
}

static void
table_list(ipfw_table_entry ent, int need_header)
{
	ipfw_table *tbl;
	socklen_t l;
	uint32_t a;

	a = ent.tbl;
	l = sizeof(a);
	if (do_cmd(IP_FW_TABLE_GETSIZE, &a, (uintptr_t)&l) < 0)
		err(EX_OSERR, "getsockopt(IP_FW_TABLE_GETSIZE)");

	/* If a is zero we have nothing to do, the table is empty. */
	if (a == 0)
		return;

	l = sizeof(*tbl) + a * sizeof(ipfw_table_entry);
	tbl = safe_calloc(1, l);
	tbl->tbl = ent.tbl;
	if (do_cmd(IP_FW_TABLE_LIST, tbl, (uintptr_t)&l) < 0)
		err(EX_OSERR, "getsockopt(IP_FW_TABLE_LIST)");
	if (tbl->cnt && need_header)
		printf("---table(%d)---\n", tbl->tbl);
	for (a = 0; a < tbl->cnt; a++) {
		unsigned int tval;
		tval = tbl->ent[a].value;
		if (do_value_as_ip) {
			char tbuf[128];
			strncpy(tbuf, inet_ntoa(*(struct in_addr *)
				&tbl->ent[a].addr), 127);
			/* inet_ntoa expects network order */
			tval = htonl(tval);
			printf("%s/%u %s\n", tbuf, tbl->ent[a].masklen,
				inet_ntoa(*(struct in_addr *)&tval));
		} else {
			printf("%s/%u %u\n",
				inet_ntoa(*(struct in_addr *)&tbl->ent[a].addr),
				tbl->ent[a].masklen, tval);
		}
	}
	free(tbl);
}

/*
 * Called with the arguments, including program name because getopt
 * wants it to be present.
 * Returns 0 if successful, 1 if empty command, errx() in case of errors.
 */
static int
ipfw_main(int oldac, char **oldav)
{
	int ch, ac, save_ac;
	const char *errstr;
	char **av, **save_av;
	int do_acct = 0;		/* Show packet/byte count */

#define WHITESP		" \t\f\v\n\r"
	if (oldac < 2)
		return 1;	/* need at least one argument */
	if (oldac == 2) {
		/*
		 * If we are called with a single string, try to split it into
		 * arguments for subsequent parsing.
		 * But first, remove spaces after a ',', by copying the string
		 * in-place.
		 */
		char *arg = oldav[1];	/* The string is the first arg. */
		int l = strlen(arg);
		int copy = 0;		/* 1 if we need to copy, 0 otherwise */
		int i, j;
		for (i = j = 0; i < l; i++) {
			if (arg[i] == '#')	/* comment marker */
				break;
			if (copy) {
				arg[j++] = arg[i];
				copy = !index("," WHITESP, arg[i]);
			} else {
				copy = !index(WHITESP, arg[i]);
				if (copy)
					arg[j++] = arg[i];
			}
		}
		if (!copy && j > 0)	/* last char was a 'blank', remove it */
			j--;
		l = j;			/* the new argument length */
		arg[j++] = '\0';
		if (l == 0)		/* empty string! */
			return 1;

		/*
		 * First, count number of arguments. Because of the previous
		 * processing, this is just the number of blanks plus 1.
		 */
		for (i = 0, ac = 1; i < l; i++)
			if (index(WHITESP, arg[i]) != NULL)
				ac++;

		/*
		 * Allocate the argument list, including one entry for
		 * the program name because getopt expects it.
		 */
		av = safe_calloc(ac + 1, sizeof(char *));

		/*
		 * Second, copy arguments from arg[] to av[]. For each one,
		 * j is the initial character, i is the one past the end.
		 */
		for (ac = 1, i = j = 0; i < l; i++)
			if (index(WHITESP, arg[i]) != NULL || i == l-1) {
				if (i == l-1)
					i++;
				av[ac] = safe_calloc(i-j+1, 1);
				bcopy(arg+j, av[ac], i-j);
				ac++;
				j = i + 1;
			}
	} else {
		/*
		 * If an argument ends with ',' join with the next one.
		 */
		int first, i, l;

		av = safe_calloc(oldac, sizeof(char *));
		for (first = i = ac = 1, l = 0; i < oldac; i++) {
			char *arg = oldav[i];
			int k = strlen(arg);

			l += k;
			if (arg[k-1] != ',' || i == oldac-1) {
				/* Time to copy. */
				av[ac] = safe_calloc(l+1, 1);
				for (l=0; first <= i; first++) {
					strcat(av[ac]+l, oldav[first]);
					l += strlen(oldav[first]);
				}
				ac++;
				l = 0;
				first = i+1;
			}
		}
	}

	av[0] = strdup(oldav[0]);	/* copy progname from the caller */
	/* Set the force flag for non-interactive processes */
	if (!do_force)
		do_force = !isatty(STDIN_FILENO);

	/* Save arguments for final freeing of memory. */
	save_ac = ac;
	save_av = av;

	optind = optreset = 1;	/* restart getopt() */
	while ((ch = getopt(ac, av, "abcdefhinNqs:STtv")) != -1)
		switch (ch) {
		case 'a':
			do_acct = 1;
			break;

		case 'b':
			comment_only = 1;
			do_compact = 1;
			break;

		case 'c':
			do_compact = 1;
			break;

		case 'd':
			do_dynamic = 1;
			break;

		case 'e':
			do_expired = 1;
			break;

		case 'f':
			do_force = 1;
			break;

		case 'h': /* help */
			free_args(save_ac, save_av);
			help();
			break;	/* NOTREACHED */

		case 'i':
			do_value_as_ip = 1;
			break;

		case 'n':
			test_only = 1;
			break;

		case 'N':
			do_resolv = 1;
			break;

		case 'q':
			do_quiet = 1;
			break;

		case 's': /* sort */
			do_sort = atoi(optarg);
			break;

		case 'S':
			show_sets = 1;
			break;

		case 't':
			do_time = 1;
			break;

		case 'T':
			do_time = 2;	/* numeric timestamp */
			break;

		case 'v': /* verbose */
			verbose = 1;
			break;

		default:
			free_args(save_ac, save_av);
			return 1;
		}

	ac -= optind;
	av += optind;
	NEED1("bad arguments, for usage summary ``ipfw''");

	/*
	 * An undocumented behaviour of ipfw1 was to allow rule numbers first,
	 * e.g. "100 add allow ..." instead of "add 100 allow ...".
	 * In case, swap first and second argument to get the normal form.
	 */
	if (ac > 1 && isdigit(*av[0])) {
		char *p = av[0];

		av[0] = av[1];
		av[1] = p;
	}

	/*
	 * Optional: pipe, queue or nat.
	 */
	do_nat = 0;
	do_pipe = 0;
	if (!strncmp(*av, "nat", strlen(*av)))
 	        do_nat = 1;
 	else if (!strncmp(*av, "pipe", strlen(*av)))
		do_pipe = 1;
	else if (_substrcmp(*av, "queue") == 0)
		do_pipe = 2;
	else if (!strncmp(*av, "set", strlen(*av))) {
		if (ac > 1 && isdigit(av[1][0])) {
			use_set = strtonum(av[1], 0, RESVD_SET, &errstr);
			if (errstr)
				errx(EX_DATAERR,
				    "invalid set number %s\n", av[1]);
			ac -= 2; av += 2; use_set++;
		}
	}

	if (do_pipe || do_nat) {
		ac--;
		av++;
	}
	NEED1("missing command");

	/*
	 * For pipes, queues and nats we normally say 'nat|pipe NN config'
	 * but the code is easier to parse as 'nat|pipe config NN'
	 * so we swap the two arguments.
	 */
	if ((do_pipe || do_nat) && ac > 1 && isdigit(*av[0])) {
		char *p = av[0];

		av[0] = av[1];
		av[1] = p;
	}

	int try_next = 0;
	if (use_set == 0) {
		if (_substrcmp(*av, "add") == 0)
			add(ac, av);
		else if (do_nat && _substrcmp(*av, "show") == 0)
 			show_nat(ac, av);
		else if (do_pipe && _substrcmp(*av, "config") == 0)
			config_pipe(ac, av);
		else if (do_nat && _substrcmp(*av, "config") == 0)
 			config_nat(ac, av);
		else if (_substrcmp(*av, "set") == 0)
			sets_handler(ac, av);
		else if (_substrcmp(*av, "table") == 0)
			table_handler(ac, av);
		else if (_substrcmp(*av, "enable") == 0)
			sysctl_handler(ac, av, 1);
		else if (_substrcmp(*av, "disable") == 0)
			sysctl_handler(ac, av, 0);
		else
			try_next = 1;
	}

	if (use_set || try_next) {
		if (_substrcmp(*av, "delete") == 0)
			delete(ac, av);
		else if (_substrcmp(*av, "flush") == 0)
			flush(do_force);
		else if (_substrcmp(*av, "zero") == 0)
			zero(ac, av, IP_FW_ZERO);
		else if (_substrcmp(*av, "resetlog") == 0)
			zero(ac, av, IP_FW_RESETLOG);
		else if (_substrcmp(*av, "print") == 0 ||
		         _substrcmp(*av, "list") == 0)
			list(ac, av, do_acct);
		else if (_substrcmp(*av, "show") == 0)
			list(ac, av, 1 /* show counters */);
		else
			errx(EX_USAGE, "bad command `%s'", *av);
	}

	/* Free memory allocated in the argument parsing. */
	free_args(save_ac, save_av);
	return 0;
}


static void
ipfw_readfile(int ac, char *av[])
{
#define MAX_ARGS	32
	char	buf[BUFSIZ];
	char *progname = av[0];		/* original program name */
	const char *cmd = NULL;		/* preprocessor name, if any */
	const char *filename = av[ac-1]; /* file to read */
	int	c, lineno=0;
	FILE	*f = NULL;
	pid_t	preproc = 0;

	while ((c = getopt(ac, av, "cfNnp:qS")) != -1) {
		switch(c) {
		case 'c':
			do_compact = 1;
			break;

		case 'f':
			do_force = 1;
			break;

		case 'N':
			do_resolv = 1;
			break;

		case 'n':
			test_only = 1;
			break;

		case 'p':
			/*
			 * ipfw -p cmd [args] filename
			 *
			 * We are done with getopt(). All arguments
			 * except the filename go to the preprocessor,
			 * so we need to do the following:
			 * - check that a filename is actually present;
			 * - advance av by optind-1 to skip arguments
			 *   already processed;
			 * - decrease ac by optind, to remove the args
			 *   already processed and the final filename;
			 * - set the last entry in av[] to NULL so
			 *   popen() can detect the end of the array;
			 * - set optind=ac to let getopt() terminate.
			 */
			if (optind == ac)
				errx(EX_USAGE, "no filename argument");
			cmd = optarg;
			av[ac-1] = NULL;
			av += optind - 1;
			ac -= optind;
			optind = ac;
			break;

		case 'q':
			do_quiet = 1;
			break;

		case 'S':
			show_sets = 1;
			break;

		default:
			errx(EX_USAGE, "bad arguments, for usage"
			     " summary ``ipfw''");
		}

	}

	if (cmd == NULL && ac != optind + 1) {
		fprintf(stderr, "ac %d, optind %d\n", ac, optind);
		errx(EX_USAGE, "extraneous filename arguments");
	}

	if ((f = fopen(filename, "r")) == NULL)
		err(EX_UNAVAILABLE, "fopen: %s", filename);

	if (cmd != NULL) {			/* pipe through preprocessor */
		int pipedes[2];

		if (pipe(pipedes) == -1)
			err(EX_OSERR, "cannot create pipe");

		preproc = fork();
		if (preproc == -1)
			err(EX_OSERR, "cannot fork");

		if (preproc == 0) {
			/*
			 * Child, will run the preprocessor with the
			 * file on stdin and the pipe on stdout.
			 */
			if (dup2(fileno(f), 0) == -1
			    || dup2(pipedes[1], 1) == -1)
				err(EX_OSERR, "dup2()");
			fclose(f);
			close(pipedes[1]);
			close(pipedes[0]);
			execvp(cmd, av);
			err(EX_OSERR, "execvp(%s) failed", cmd);
		} else { /* parent, will reopen f as the pipe */
			fclose(f);
			close(pipedes[1]);
			if ((f = fdopen(pipedes[0], "r")) == NULL) {
				int savederrno = errno;

				(void)kill(preproc, SIGTERM);
				errno = savederrno;
				err(EX_OSERR, "fdopen()");
			}
		}
	}

	while (fgets(buf, BUFSIZ, f)) {		/* read commands */
		char linename[10];
		char *args[2];

		lineno++;
		sprintf(linename, "Line %d", lineno);
		setprogname(linename); /* XXX */
		args[0] = progname;
		args[1] = buf;
		ipfw_main(2, args);
	}
	fclose(f);
	if (cmd != NULL) {
		int status;

		if (waitpid(preproc, &status, 0) == -1)
			errx(EX_OSERR, "waitpid()");
		if (WIFEXITED(status) && WEXITSTATUS(status) != EX_OK)
			errx(EX_UNAVAILABLE,
			    "preprocessor exited with status %d",
			    WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			errx(EX_UNAVAILABLE,
			    "preprocessor exited with signal %d",
			    WTERMSIG(status));
	}
}

int
main(int ac, char *av[])
{
	/*
	 * If the last argument is an absolute pathname, interpret it
	 * as a file to be preprocessed.
	 */

	if (ac > 1 && av[ac - 1][0] == '/' && access(av[ac - 1], R_OK) == 0)
		ipfw_readfile(ac, av);
	else {
		if (ipfw_main(ac, av))
			show_usage();
	}
	return EX_OK;
}
