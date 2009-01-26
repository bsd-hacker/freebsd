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
 * $FreeBSD: user/luigi/ipfw/sbin/ipfw/ipfw2.c 187733 2009-01-26 17:56:27Z luigi $
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

/*
 * prints one port, symbolic or numeric
 */
static void
print_port(int proto, uint16_t port)
{

	if (proto == IPPROTO_ETHERTYPE) {
		char const *s;

		if (do_resolv && (s = match_value(ether_types, port)) )
			printf("%s", s);
		else
			printf("0x%04x", port);
	} else {
		struct servent *se = NULL;
		if (do_resolv) {
			struct protoent *pe = getprotobynumber(proto);

			se = getservbyport(htons(port), pe ? pe->p_name : NULL);
		}
		if (se)
			printf("%s", se->s_name);
		else
			printf("%d", port);
	}
}

/*
 * Print the values in a list 16-bit items of the types above.
 * XXX todo: add support for mask.
 */
static void
print_newports(ipfw_insn_u16 *cmd, int proto, int opcode)
{
	uint16_t *p = cmd->ports;
	int i;
	char const *sep;

	if (opcode != 0) {
		sep = match_value(_port_name, opcode);
		if (sep == NULL)
			sep = "???";
		printf (" %s", sep);
	}
	sep = " ";
	for (i = F_LEN((ipfw_insn *)cmd) - 1; i > 0; i--, p += 2) {
		printf(sep);
		print_port(proto, p[0]);
		if (p[0] != p[1]) {
			printf("-");
			print_port(proto, p[1]);
		}
		sep = ",";
	}
}

#if 0
/*
 * Like strtol, but also translates service names into port numbers
 * for some protocols.
 * In particular:
 *	proto == -1 disables the protocol check;
 *	proto == IPPROTO_ETHERTYPE looks up an internal table
 *	proto == <some value in /etc/protocols> matches the values there.
 * Returns *end == s in case the parameter is not found.
 */
static int
strtoport(char *s, char **end, int base, int proto)
{
	char *p, *buf;
	char *s1;
	int i;

	*end = s;		/* default - not found */
	if (*s == '\0')
		return 0;	/* not found */

	if (isdigit(*s))
		return strtol(s, end, base);

	/*
	 * find separator. '\\' escapes the next char.
	 */
	for (s1 = s; *s1 && (isalnum(*s1) || *s1 == '\\') ; s1++)
		if (*s1 == '\\' && s1[1] != '\0')
			s1++;

	buf = safe_calloc(s1 - s + 1, 1);

	/*
	 * copy into a buffer skipping backslashes
	 */
	for (p = s, i = 0; p != s1 ; p++)
		if (*p != '\\')
			buf[i++] = *p;
	buf[i++] = '\0';

	if (proto == IPPROTO_ETHERTYPE) {
		i = match_token(ether_types, buf);
		free(buf);
		if (i != -1) {	/* found */
			*end = s1;
			return i;
		}
	} else {
		struct protoent *pe = NULL;
		struct servent *se;

		if (proto != 0)
			pe = getprotobynumber(proto);
		setservent(1);
		se = getservbyname(buf, pe ? pe->p_name : NULL);
		free(buf);
		if (se != NULL) {
			*end = s1;
			return ntohs(se->s_port);
		}
	}
	return 0;	/* not found */
}
#endif

/*
 * Map between current altq queue id numbers and names.
 */
static int altq_fetched = 0;
static TAILQ_HEAD(, pf_altq) altq_entries = 
	TAILQ_HEAD_INITIALIZER(altq_entries);

static void
altq_set_enabled(int enabled)
{
	int pffd;

	pffd = open("/dev/pf", O_RDWR);
	if (pffd == -1)
		err(EX_UNAVAILABLE,
		    "altq support opening pf(4) control device");
	if (enabled) {
		if (ioctl(pffd, DIOCSTARTALTQ) != 0 && errno != EEXIST)
			err(EX_UNAVAILABLE, "enabling altq");
	} else {
		if (ioctl(pffd, DIOCSTOPALTQ) != 0 && errno != ENOENT)
			err(EX_UNAVAILABLE, "disabling altq");
	}
	close(pffd);
}

static void
altq_fetch(void)
{
	struct pfioc_altq pfioc;
	struct pf_altq *altq;
	int pffd;
	unsigned int mnr;

	if (altq_fetched)
		return;
	altq_fetched = 1;
	pffd = open("/dev/pf", O_RDONLY);
	if (pffd == -1) {
		warn("altq support opening pf(4) control device");
		return;
	}
	bzero(&pfioc, sizeof(pfioc));
	if (ioctl(pffd, DIOCGETALTQS, &pfioc) != 0) {
		warn("altq support getting queue list");
		close(pffd);
		return;
	}
	mnr = pfioc.nr;
	for (pfioc.nr = 0; pfioc.nr < mnr; pfioc.nr++) {
		if (ioctl(pffd, DIOCGETALTQ, &pfioc) != 0) {
			if (errno == EBUSY)
				break;
			warn("altq support getting queue list");
			close(pffd);
			return;
		}
		if (pfioc.altq.qid == 0)
			continue;
		altq = safe_calloc(1, sizeof(*altq));
		*altq = pfioc.altq;
		TAILQ_INSERT_TAIL(&altq_entries, altq, entries);
	}
	close(pffd);
}

#if 0
static u_int32_t
altq_name_to_qid(const char *name)
{
	struct pf_altq *altq;

	altq_fetch();
	TAILQ_FOREACH(altq, &altq_entries, entries)
		if (strcmp(name, altq->qname) == 0)
			break;
	if (altq == NULL)
		errx(EX_DATAERR, "altq has no queue named `%s'", name);
	return altq->qid;
}
#endif

static const char *
altq_qid_to_name(u_int32_t qid)
{
	struct pf_altq *altq;

	altq_fetch();
	TAILQ_FOREACH(altq, &altq_entries, entries)
		if (qid == altq->qid)
			break;
	if (altq == NULL)
		return NULL;
	return altq->qname;
}

#if 0
static void
fill_altq_qid(u_int32_t *qid, const char *av)
{
	*qid = altq_name_to_qid(av);
}

/*
 * Fill the body of the command with the list of port ranges.
 */
static int
fill_newports(ipfw_insn_u16 *cmd, char *av, int proto)
{
	uint16_t a, b, *p = cmd->ports;
	int i = 0;
	char *s = av;

	while (*s) {
		a = strtoport(av, &s, 0, proto);
		if (s == av) 			/* empty or invalid argument */
			return (0);

		switch (*s) {
		case '-':			/* a range */
			av = s + 1;
			b = strtoport(av, &s, 0, proto);
			/* Reject expressions like '1-abc' or '1-2-3'. */
			if (s == av || (*s != ',' && *s != '\0'))
				return (0);
			p[0] = a;
			p[1] = b;
			break;
		case ',':			/* comma separated list */
		case '\0':
			p[0] = p[1] = a;
			break;
		default:
			warnx("port list: invalid separator <%c> in <%s>",
				*s, av);
			return (0);
		}

		i++;
		p += 2;
		av = s + 1;
	}
	if (i > 0) {
		if (i + 1 > F_LEN_MASK)
			errx(EX_DATAERR, "too many ports/ranges\n");
		cmd->o.len |= i + 1;	/* leave F_NOT and F_OR untouched */
	}
	return (i);
}
#endif

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

#if 0
static void
fill_reject_code(u_short *codep, char *str)
{
	int val;
	char *s;

	val = strtoul(str, &s, 0);
	if (s == str || *s != '\0' || val >= 0x100)
		val = match_token(icmpcodes, str);
	if (val < 0)
		errx(EX_DATAERR, "unknown ICMP unreachable code ``%s''", str);
	*codep = val;
	return;
}
#endif

static void
print_reject_code(uint16_t code)
{
	char const *s = match_value(icmpcodes, code);

	if (s != NULL)
		printf("unreach %s", s);
	else
		printf("unreach %u", code);
}

struct _s_x icmp6codes[] = {
      { "no-route",		ICMP6_DST_UNREACH_NOROUTE },
      { "admin-prohib",		ICMP6_DST_UNREACH_ADMIN },
      { "address",		ICMP6_DST_UNREACH_ADDR },
      { "port",			ICMP6_DST_UNREACH_NOPORT },
      { NULL, 0 }
};

#if 0
static void
fill_unreach6_code(u_short *codep, char *str)
{
	int val;
	char *s;

	val = strtoul(str, &s, 0);
	if (s == str || *s != '\0' || val >= 0x100)
		val = match_token(icmp6codes, str);
	if (val < 0)
		errx(EX_DATAERR, "unknown ICMPv6 unreachable code ``%s''", str);
	*codep = val;
	return;
}

static void
print_unreach6_code(uint16_t code)
{
	char const *s = match_value(icmp6codes, code);

	if (s != NULL)
		printf("unreach6 %s", s);
	else
		printf("unreach6 %u", code);
}

/*
 * Returns the number of bits set (from left) in a contiguous bitmask,
 * or -1 if the mask is not contiguous.
 * XXX this needs a proper fix.
 * This effectively works on masks in big-endian (network) format.
 * when compiled on little endian architectures.
 *
 * First bit is bit 7 of the first byte -- note, for MAC addresses,
 * the first bit on the wire is bit 0 of the first byte.
 * len is the max length in bits.
 */
static int
contigmask(uint8_t *p, int len)
{
	int i, n;

	for (i=0; i<len ; i++)
		if ( (p[i/8] & (1 << (7 - (i%8)))) == 0) /* first bit unset */
			break;
	for (n=i+1; n < len; n++)
		if ( (p[n/8] & (1 << (7 - (n%8)))) != 0)
			return -1; /* mask not contiguous */
	return i;
}
#endif

/*
 * print flags set/clear in the two bitmasks passed as parameters.
 * There is a specialized check for f_tcpflags.
 */
static void
print_flags(char const *name, ipfw_insn *cmd, struct _s_x *list)
{
	char const *comma = "";
	int i;
	uint8_t set = cmd->arg1 & 0xff;
	uint8_t clear = (cmd->arg1 >> 8) & 0xff;

	if (list == f_tcpflags && set == TH_SYN && clear == TH_ACK) {
		printf(" setup");
		return;
	}

	printf(" %s ", name);
	for (i=0; list[i].x != 0; i++) {
		if (set & list[i].x) {
			set &= ~list[i].x;
			printf("%s%s", comma, list[i].s);
			comma = ",";
		}
		if (clear & list[i].x) {
			clear &= ~list[i].x;
			printf("%s!%s", comma, list[i].s);
			comma = ",";
		}
	}
}

/*
 * Print the ip address contained in a command.
 */
static void
print_ip(ipfw_insn_ip *cmd, char const *s)
{
	struct hostent *he = NULL;
	int len = F_LEN((ipfw_insn *)cmd);
	uint32_t *a = ((ipfw_insn_u32 *)cmd)->d;

	printf("%s%s ", cmd->o.len & F_NOT ? " not": "", s);

	if (cmd->o.opcode == O_IP_SRC_ME || cmd->o.opcode == O_IP_DST_ME) {
		printf("me");
		return;
	}
	if (cmd->o.opcode == O_IP_SRC_LOOKUP ||
	    cmd->o.opcode == O_IP_DST_LOOKUP) {
		printf("table(%u", ((ipfw_insn *)cmd)->arg1);
		if (len == F_INSN_SIZE(ipfw_insn_u32))
			printf(",%u", *a);
		printf(")");
		return;
	}
	if (cmd->o.opcode == O_IP_SRC_SET || cmd->o.opcode == O_IP_DST_SET) {
		uint32_t x, *map = (uint32_t *)&(cmd->mask);
		int i, j;
		char comma = '{';

		x = cmd->o.arg1 - 1;
		x = htonl( ~x );
		cmd->addr.s_addr = htonl(cmd->addr.s_addr);
		printf("%s/%d", inet_ntoa(cmd->addr),
			contigmask((uint8_t *)&x, 32));
		x = cmd->addr.s_addr = htonl(cmd->addr.s_addr);
		x &= 0xff; /* base */
		/*
		 * Print bits and ranges.
		 * Locate first bit set (i), then locate first bit unset (j).
		 * If we have 3+ consecutive bits set, then print them as a
		 * range, otherwise only print the initial bit and rescan.
		 */
		for (i=0; i < cmd->o.arg1; i++)
			if (map[i/32] & (1<<(i & 31))) {
				for (j=i+1; j < cmd->o.arg1; j++)
					if (!(map[ j/32] & (1<<(j & 31))))
						break;
				printf("%c%d", comma, i+x);
				if (j>i+2) { /* range has at least 3 elements */
					printf("-%d", j-1+x);
					i = j-1;
				}
				comma = ',';
			}
		printf("}");
		return;
	}
	/*
	 * len == 2 indicates a single IP, whereas lists of 1 or more
	 * addr/mask pairs have len = (2n+1). We convert len to n so we
	 * use that to count the number of entries.
	 */
    for (len = len / 2; len > 0; len--, a += 2) {
	int mb =	/* mask length */
	    (cmd->o.opcode == O_IP_SRC || cmd->o.opcode == O_IP_DST) ?
		32 : contigmask((uint8_t *)&(a[1]), 32);
	if (mb == 32 && do_resolv)
		he = gethostbyaddr((char *)&(a[0]), sizeof(u_long), AF_INET);
	if (he != NULL)		/* resolved to name */
		printf("%s", he->h_name);
	else if (mb == 0)	/* any */
		printf("any");
	else {		/* numeric IP followed by some kind of mask */
		printf("%s", inet_ntoa( *((struct in_addr *)&a[0]) ) );
		if (mb < 0)
			printf(":%s", inet_ntoa( *((struct in_addr *)&a[1]) ) );
		else if (mb < 32)
			printf("/%d", mb);
	}
	if (len > 1)
		printf(",");
    }
}

/*
 * prints a MAC address/mask pair
 */
static void
print_mac(uint8_t *addr, uint8_t *mask)
{
	int l = contigmask(mask, 48);

	if (l == 0)
		printf(" any");
	else {
		printf(" %02x:%02x:%02x:%02x:%02x:%02x",
		    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		if (l == -1)
			printf("&%02x:%02x:%02x:%02x:%02x:%02x",
			    mask[0], mask[1], mask[2],
			    mask[3], mask[4], mask[5]);
		else if (l < 48)
			printf("/%d", l);
	}
}

#if 0
static void
fill_icmptypes(ipfw_insn_u32 *cmd, char *av)
{
	uint8_t type;

	cmd->d[0] = 0;
	while (*av) {
		if (*av == ',')
			av++;

		type = strtoul(av, &av, 0);

		if (*av != ',' && *av != '\0')
			errx(EX_DATAERR, "invalid ICMP type");

		if (type > 31)
			errx(EX_DATAERR, "ICMP type out of range");

		cmd->d[0] |= 1 << type;
	}
	cmd->o.opcode = O_ICMPTYPE;
	cmd->o.len |= F_INSN_SIZE(ipfw_insn_u32);
}
#endif

static void
print_icmptypes(ipfw_insn_u32 *cmd)
{
	int i;
	char sep= ' ';

	printf(" icmptypes");
	for (i = 0; i < 32; i++) {
		if ( (cmd->d[0] & (1 << (i))) == 0)
			continue;
		printf("%c%d", sep, i);
		sep = ',';
	}
}

/* 
 * Print the ip address contained in a command.
 */
static void
print_ip6(ipfw_insn_ip6 *cmd, char const *s)
{
       struct hostent *he = NULL;
       int len = F_LEN((ipfw_insn *) cmd) - 1;
       struct in6_addr *a = &(cmd->addr6);
       char trad[255];

       printf("%s%s ", cmd->o.len & F_NOT ? " not": "", s);

       if (cmd->o.opcode == O_IP6_SRC_ME || cmd->o.opcode == O_IP6_DST_ME) {
               printf("me6");
               return;
       }
       if (cmd->o.opcode == O_IP6) {
               printf(" ip6");
               return;
       }

       /*
        * len == 4 indicates a single IP, whereas lists of 1 or more
        * addr/mask pairs have len = (2n+1). We convert len to n so we
        * use that to count the number of entries.
        */

       for (len = len / 4; len > 0; len -= 2, a += 2) {
           int mb =        /* mask length */
               (cmd->o.opcode == O_IP6_SRC || cmd->o.opcode == O_IP6_DST) ?
               128 : contigmask((uint8_t *)&(a[1]), 128);

           if (mb == 128 && do_resolv)
               he = gethostbyaddr((char *)a, sizeof(*a), AF_INET6);
           if (he != NULL)             /* resolved to name */
               printf("%s", he->h_name);
           else if (mb == 0)           /* any */
               printf("any");
           else {          /* numeric IP followed by some kind of mask */
               if (inet_ntop(AF_INET6,  a, trad, sizeof( trad ) ) == NULL)
                   printf("Error ntop in print_ip6\n");
               printf("%s",  trad );
               if (mb < 0)     /* XXX not really legal... */
                   printf(":%s",
                       inet_ntop(AF_INET6, &a[1], trad, sizeof(trad)));
               else if (mb < 128)
                   printf("/%d", mb);
           }
           if (len > 2)
               printf(",");
       }
}

#if 0
static void
fill_icmp6types(ipfw_insn_icmp6 *cmd, char *av)
{
       uint8_t type;

       bzero(cmd, sizeof(*cmd));
       while (*av) {
           if (*av == ',')
               av++;
           type = strtoul(av, &av, 0);
           if (*av != ',' && *av != '\0')
               errx(EX_DATAERR, "invalid ICMP6 type");
	   /*
	    * XXX: shouldn't this be 0xFF?  I can't see any reason why
	    * we shouldn't be able to filter all possiable values
	    * regardless of the ability of the rest of the kernel to do
	    * anything useful with them.
	    */
           if (type > ICMP6_MAXTYPE)
               errx(EX_DATAERR, "ICMP6 type out of range");
           cmd->d[type / 32] |= ( 1 << (type % 32));
       }
       cmd->o.opcode = O_ICMP6TYPE;
       cmd->o.len |= F_INSN_SIZE(ipfw_insn_icmp6);
}
#endif


static void
print_icmp6types(ipfw_insn_u32 *cmd)
{
       int i, j;
       char sep= ' ';

       printf(" ip6 icmp6types");
       for (i = 0; i < 7; i++)
               for (j=0; j < 32; ++j) {
                       if ( (cmd->d[i] & (1 << (j))) == 0)
                               continue;
                       printf("%c%d", sep, (i*32 + j));
                       sep = ',';
               }
}

static void
print_flow6id( ipfw_insn_u32 *cmd)
{
       uint16_t i, limit = cmd->o.arg1;
       char sep = ',';

       printf(" flow-id ");
       for( i=0; i < limit; ++i) {
               if (i == limit - 1)
                       sep = ' ';
               printf("%d%c", cmd->d[i], sep);
       }
}

#if 0
/* structure and define for the extension header in ipv6 */
static struct _s_x ext6hdrcodes[] = {
       { "frag",       EXT_FRAGMENT },
       { "hopopt",     EXT_HOPOPTS },
       { "route",      EXT_ROUTING },
       { "dstopt",     EXT_DSTOPTS },
       { "ah",         EXT_AH },
       { "esp",        EXT_ESP },
       { "rthdr0",     EXT_RTHDR0 },
       { "rthdr2",     EXT_RTHDR2 },
       { NULL,         0 }
};

/* fills command for the extension header filtering */
static int
fill_ext6hdr( ipfw_insn *cmd, char *av)
{
       int tok;
       char *s = av;

       cmd->arg1 = 0;

       while(s) {
           av = strsep( &s, ",") ;
           tok = match_token(ext6hdrcodes, av);
           switch (tok) {
           case EXT_FRAGMENT:
               cmd->arg1 |= EXT_FRAGMENT;
               break;

           case EXT_HOPOPTS:
               cmd->arg1 |= EXT_HOPOPTS;
               break;

           case EXT_ROUTING:
               cmd->arg1 |= EXT_ROUTING;
               break;

           case EXT_DSTOPTS:
               cmd->arg1 |= EXT_DSTOPTS;
               break;

           case EXT_AH:
               cmd->arg1 |= EXT_AH;
               break;

           case EXT_ESP:
               cmd->arg1 |= EXT_ESP;
               break;

           case EXT_RTHDR0:
               cmd->arg1 |= EXT_RTHDR0;
               break;

           case EXT_RTHDR2:
               cmd->arg1 |= EXT_RTHDR2;
               break;

           default:
               errx( EX_DATAERR, "invalid option for ipv6 exten header" );
               break;
           }
       }
       if (cmd->arg1 == 0 )
           return 0;
       cmd->opcode = O_EXT_HDR;
       cmd->len |= F_INSN_SIZE( ipfw_insn );
       return 1;
}
#endif

static void
print_ext6hdr( ipfw_insn *cmd )
{
       char sep = ' ';

       printf(" extension header:");
       if (cmd->arg1 & EXT_FRAGMENT ) {
           printf("%cfragmentation", sep);
           sep = ',';
       }
       if (cmd->arg1 & EXT_HOPOPTS ) {
           printf("%chop options", sep);
           sep = ',';
       }
       if (cmd->arg1 & EXT_ROUTING ) {
           printf("%crouting options", sep);
           sep = ',';
       }
       if (cmd->arg1 & EXT_RTHDR0 ) {
           printf("%crthdr0", sep);
           sep = ',';
       }
       if (cmd->arg1 & EXT_RTHDR2 ) {
           printf("%crthdr2", sep);
           sep = ',';
       }
       if (cmd->arg1 & EXT_DSTOPTS ) {
           printf("%cdestination options", sep);
           sep = ',';
       }
       if (cmd->arg1 & EXT_AH ) {
           printf("%cauthentication header", sep);
           sep = ',';
       }
       if (cmd->arg1 & EXT_ESP ) {
           printf("%cencapsulated security payload", sep);
       }
}

/*
 * show_ipfw() prints the body of an ipfw rule.
 * Because the standard rule has at least proto src_ip dst_ip, we use
 * a helper function to produce these entries if not provided explicitly.
 * The first argument is the list of fields we have, the second is
 * the list of fields we want to be printed.
 *
 * Special cases if we have provided a MAC header:
 *   + if the rule does not contain IP addresses/ports, do not print them;
 *   + if the rule does not contain an IP proto, print "all" instead of "ip";
 *
 * Once we have 'have_options', IP header fields are printed as options.
 */
#define	HAVE_PROTO	0x0001
#define	HAVE_SRCIP	0x0002
#define	HAVE_DSTIP	0x0004
#define	HAVE_PROTO4	0x0008
#define	HAVE_PROTO6	0x0010
#define	HAVE_OPTIONS	0x8000

#define	HAVE_IP		(HAVE_PROTO | HAVE_SRCIP | HAVE_DSTIP)
static void
show_prerequisites(int *flags, int want, int cmd __unused)
{
	if (comment_only)
		return;
	if ( (*flags & HAVE_IP) == HAVE_IP)
		*flags |= HAVE_OPTIONS;

	if ( !(*flags & HAVE_OPTIONS)) {
		if ( !(*flags & HAVE_PROTO) && (want & HAVE_PROTO)) {
			if ( (*flags & HAVE_PROTO4))
				printf(" ip4");
			else if ( (*flags & HAVE_PROTO6))
				printf(" ip6");
			else
				printf(" ip");
		}
		if ( !(*flags & HAVE_SRCIP) && (want & HAVE_SRCIP))
			printf(" from any");
		if ( !(*flags & HAVE_DSTIP) && (want & HAVE_DSTIP))
			printf(" to any");
	}
	*flags |= want;
}

void
show_ipfw(struct ip_fw *rule, int pcwidth, int bcwidth)
{
	static int twidth = 0;
	int l;
	ipfw_insn *cmd, *tagptr = NULL;
	const char *comment = NULL;	/* ptr to comment if we have one */
	int proto = 0;		/* default */
	int flags = 0;	/* prerequisites */
	ipfw_insn_log *logptr = NULL; /* set if we find an O_LOG */
	ipfw_insn_altq *altqptr = NULL; /* set if we find an O_ALTQ */
	int or_block = 0;	/* we are in an or block */
	uint32_t set_disable;

	bcopy(&rule->next_rule, &set_disable, sizeof(set_disable));

	if (set_disable & (1 << rule->set)) { /* disabled */
		if (!show_sets)
			return;
		else
			printf("# DISABLED ");
	}
	printf("%05u ", rule->rulenum);

	if (pcwidth>0 || bcwidth>0)
		printf("%*llu %*llu ", pcwidth, align_uint64(&rule->pcnt),
		    bcwidth, align_uint64(&rule->bcnt));

	if (do_time == 2)
		printf("%10u ", rule->timestamp);
	else if (do_time == 1) {
		char timestr[30];
		time_t t = (time_t)0;

		if (twidth == 0) {
			strcpy(timestr, ctime(&t));
			*strchr(timestr, '\n') = '\0';
			twidth = strlen(timestr);
		}
		if (rule->timestamp) {
			t = _long_to_time(rule->timestamp);

			strcpy(timestr, ctime(&t));
			*strchr(timestr, '\n') = '\0';
			printf("%s ", timestr);
		} else {
			printf("%*s", twidth, " ");
		}
	}

	if (show_sets)
		printf("set %d ", rule->set);

	/*
	 * print the optional "match probability"
	 */
	if (rule->cmd_len > 0) {
		cmd = rule->cmd ;
		if (cmd->opcode == O_PROB) {
			ipfw_insn_u32 *p = (ipfw_insn_u32 *)cmd;
			double d = 1.0 * p->d[0];

			d = (d / 0x7fffffff);
			printf("prob %f ", d);
		}
	}

	/*
	 * first print actions
	 */
        for (l = rule->cmd_len - rule->act_ofs, cmd = ACTION_PTR(rule);
			l > 0 ; l -= F_LEN(cmd), cmd += F_LEN(cmd)) {
		switch(cmd->opcode) {
		case O_CHECK_STATE:
			printf("check-state");
			flags = HAVE_IP; /* avoid printing anything else */
			break;

		case O_ACCEPT:
			printf("allow");
			break;

		case O_COUNT:
			printf("count");
			break;

		case O_DENY:
			printf("deny");
			break;

		case O_REJECT:
			if (cmd->arg1 == ICMP_REJECT_RST)
				printf("reset");
			else if (cmd->arg1 == ICMP_UNREACH_HOST)
				printf("reject");
			else
				print_reject_code(cmd->arg1);
			break;

		case O_UNREACH6:
			if (cmd->arg1 == ICMP6_UNREACH_RST)
				printf("reset6");
			else
				print_unreach6_code(cmd->arg1);
			break;

		case O_SKIPTO:
			PRINT_UINT_ARG("skipto ", cmd->arg1);
			break;

		case O_PIPE:
			PRINT_UINT_ARG("pipe ", cmd->arg1);
			break;

		case O_QUEUE:
			PRINT_UINT_ARG("queue ", cmd->arg1);
			break;

		case O_DIVERT:
			PRINT_UINT_ARG("divert ", cmd->arg1);
			break;

		case O_TEE:
			PRINT_UINT_ARG("tee ", cmd->arg1);
			break;

		case O_NETGRAPH:
			PRINT_UINT_ARG("netgraph ", cmd->arg1);
			break;

		case O_NGTEE:
			PRINT_UINT_ARG("ngtee ", cmd->arg1);
			break;

		case O_FORWARD_IP:
		    {
			ipfw_insn_sa *s = (ipfw_insn_sa *)cmd;

			if (s->sa.sin_addr.s_addr == INADDR_ANY) {
				printf("fwd tablearg");
			} else {
				printf("fwd %s", inet_ntoa(s->sa.sin_addr));
			}
			if (s->sa.sin_port)
				printf(",%d", s->sa.sin_port);
		    }
			break;

		case O_LOG: /* O_LOG is printed last */
			logptr = (ipfw_insn_log *)cmd;
			break;

		case O_ALTQ: /* O_ALTQ is printed after O_LOG */
			altqptr = (ipfw_insn_altq *)cmd;
			break;

		case O_TAG:
			tagptr = cmd;
			break;

		case O_NAT:
			PRINT_UINT_ARG("nat ", cmd->arg1);
 			break;
			
		case O_SETFIB:
			PRINT_UINT_ARG("setfib ", cmd->arg1);
 			break;
			
		default:
			printf("** unrecognized action %d len %d ",
				cmd->opcode, cmd->len);
		}
	}
	if (logptr) {
		if (logptr->max_log > 0)
			printf(" log logamount %d", logptr->max_log);
		else
			printf(" log");
	}
	if (altqptr) {
		const char *qname;

		qname = altq_qid_to_name(altqptr->qid);
		if (qname == NULL)
			printf(" altq ?<%u>", altqptr->qid);
		else
			printf(" altq %s", qname);
	}
	if (tagptr) {
		if (tagptr->len & F_NOT)
			PRINT_UINT_ARG(" untag ", tagptr->arg1);
		else
			PRINT_UINT_ARG(" tag ", tagptr->arg1);
	}

	/*
	 * then print the body.
	 */
        for (l = rule->act_ofs, cmd = rule->cmd ;
			l > 0 ; l -= F_LEN(cmd) , cmd += F_LEN(cmd)) {
		if ((cmd->len & F_OR) || (cmd->len & F_NOT))
			continue;
		if (cmd->opcode == O_IP4) {
			flags |= HAVE_PROTO4;
			break;
		} else if (cmd->opcode == O_IP6) {
			flags |= HAVE_PROTO6;
			break;
		}			
	}
	if (rule->_pad & 1) {	/* empty rules before options */
		if (!do_compact) {
			show_prerequisites(&flags, HAVE_PROTO, 0);
			printf(" from any to any");
		}
		flags |= HAVE_IP | HAVE_OPTIONS;
	}

	if (comment_only)
		comment = "...";

        for (l = rule->act_ofs, cmd = rule->cmd ;
			l > 0 ; l -= F_LEN(cmd) , cmd += F_LEN(cmd)) {
		/* useful alias */
		ipfw_insn_u32 *cmd32 = (ipfw_insn_u32 *)cmd;

		if (comment_only) {
			if (cmd->opcode != O_NOP)
				continue;
			printf(" // %s\n", (char *)(cmd + 1));
			return;
		}

		show_prerequisites(&flags, 0, cmd->opcode);

		switch(cmd->opcode) {
		case O_PROB:
			break;	/* done already */

		case O_PROBE_STATE:
			break; /* no need to print anything here */

		case O_IP_SRC:
		case O_IP_SRC_LOOKUP:
		case O_IP_SRC_MASK:
		case O_IP_SRC_ME:
		case O_IP_SRC_SET:
			show_prerequisites(&flags, HAVE_PROTO, 0);
			if (!(flags & HAVE_SRCIP))
				printf(" from");
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_ip((ipfw_insn_ip *)cmd,
				(flags & HAVE_OPTIONS) ? " src-ip" : "");
			flags |= HAVE_SRCIP;
			break;

		case O_IP_DST:
		case O_IP_DST_LOOKUP:
		case O_IP_DST_MASK:
		case O_IP_DST_ME:
		case O_IP_DST_SET:
			show_prerequisites(&flags, HAVE_PROTO|HAVE_SRCIP, 0);
			if (!(flags & HAVE_DSTIP))
				printf(" to");
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_ip((ipfw_insn_ip *)cmd,
				(flags & HAVE_OPTIONS) ? " dst-ip" : "");
			flags |= HAVE_DSTIP;
			break;

		case O_IP6_SRC:
		case O_IP6_SRC_MASK:
		case O_IP6_SRC_ME:
			show_prerequisites(&flags, HAVE_PROTO, 0);
			if (!(flags & HAVE_SRCIP))
				printf(" from");
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_ip6((ipfw_insn_ip6 *)cmd,
			    (flags & HAVE_OPTIONS) ? " src-ip6" : "");
			flags |= HAVE_SRCIP | HAVE_PROTO;
			break;

		case O_IP6_DST:
		case O_IP6_DST_MASK:
		case O_IP6_DST_ME:
			show_prerequisites(&flags, HAVE_PROTO|HAVE_SRCIP, 0);
			if (!(flags & HAVE_DSTIP))
				printf(" to");
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_ip6((ipfw_insn_ip6 *)cmd,
			    (flags & HAVE_OPTIONS) ? " dst-ip6" : "");
			flags |= HAVE_DSTIP;
			break;

		case O_FLOW6ID:
		print_flow6id( (ipfw_insn_u32 *) cmd );
		flags |= HAVE_OPTIONS;
		break;

		case O_IP_DSTPORT:
			show_prerequisites(&flags, HAVE_IP, 0);
		case O_IP_SRCPORT:
			show_prerequisites(&flags, HAVE_PROTO|HAVE_SRCIP, 0);
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			if (cmd->len & F_NOT)
				printf(" not");
			print_newports((ipfw_insn_u16 *)cmd, proto,
				(flags & HAVE_OPTIONS) ? cmd->opcode : 0);
			break;

		case O_PROTO: {
			struct protoent *pe = NULL;

			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			if (cmd->len & F_NOT)
				printf(" not");
			proto = cmd->arg1;
			pe = getprotobynumber(cmd->arg1);
			if ((flags & (HAVE_PROTO4 | HAVE_PROTO6)) &&
			    !(flags & HAVE_PROTO))
				show_prerequisites(&flags,
				    HAVE_IP | HAVE_OPTIONS, 0);
			if (flags & HAVE_OPTIONS)
				printf(" proto");
			if (pe)
				printf(" %s", pe->p_name);
			else
				printf(" %u", cmd->arg1);
			}
			flags |= HAVE_PROTO;
			break;

		default: /*options ... */
			if (!(cmd->len & (F_OR|F_NOT)))
				if (((cmd->opcode == O_IP6) &&
				    (flags & HAVE_PROTO6)) ||
				    ((cmd->opcode == O_IP4) &&
				    (flags & HAVE_PROTO4)))
					break;
			show_prerequisites(&flags, HAVE_IP | HAVE_OPTIONS, 0);
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			if (cmd->len & F_NOT && cmd->opcode != O_IN)
				printf(" not");
			switch(cmd->opcode) {
			case O_MACADDR2: {
				ipfw_insn_mac *m = (ipfw_insn_mac *)cmd;

				printf(" MAC");
				print_mac(m->addr, m->mask);
				print_mac(m->addr + 6, m->mask + 6);
				}
				break;

			case O_MAC_TYPE:
				print_newports((ipfw_insn_u16 *)cmd,
						IPPROTO_ETHERTYPE, cmd->opcode);
				break;


			case O_FRAG:
				printf(" frag");
				break;

			case O_FIB:
				printf(" fib %u", cmd->arg1 );
				break;

			case O_IN:
				printf(cmd->len & F_NOT ? " out" : " in");
				break;

			case O_DIVERTED:
				switch (cmd->arg1) {
				case 3:
					printf(" diverted");
					break;
				case 1:
					printf(" diverted-loopback");
					break;
				case 2:
					printf(" diverted-output");
					break;
				default:
					printf(" diverted-?<%u>", cmd->arg1);
					break;
				}
				break;

			case O_LAYER2:
				printf(" layer2");
				break;
			case O_XMIT:
			case O_RECV:
			case O_VIA:
			    {
				char const *s;
				ipfw_insn_if *cmdif = (ipfw_insn_if *)cmd;

				if (cmd->opcode == O_XMIT)
					s = "xmit";
				else if (cmd->opcode == O_RECV)
					s = "recv";
				else /* if (cmd->opcode == O_VIA) */
					s = "via";
				if (cmdif->name[0] == '\0')
					printf(" %s %s", s,
					    inet_ntoa(cmdif->p.ip));
				else
					printf(" %s %s", s, cmdif->name);

				break;
			    }
			case O_IPID:
				if (F_LEN(cmd) == 1)
				    printf(" ipid %u", cmd->arg1 );
				else
				    print_newports((ipfw_insn_u16 *)cmd, 0,
					O_IPID);
				break;

			case O_IPTTL:
				if (F_LEN(cmd) == 1)
				    printf(" ipttl %u", cmd->arg1 );
				else
				    print_newports((ipfw_insn_u16 *)cmd, 0,
					O_IPTTL);
				break;

			case O_IPVER:
				printf(" ipver %u", cmd->arg1 );
				break;

			case O_IPPRECEDENCE:
				printf(" ipprecedence %u", (cmd->arg1) >> 5 );
				break;

			case O_IPLEN:
				if (F_LEN(cmd) == 1)
				    printf(" iplen %u", cmd->arg1 );
				else
				    print_newports((ipfw_insn_u16 *)cmd, 0,
					O_IPLEN);
				break;

			case O_IPOPT:
				print_flags("ipoptions", cmd, f_ipopts);
				break;

			case O_IPTOS:
				print_flags("iptos", cmd, f_iptos);
				break;

			case O_ICMPTYPE:
				print_icmptypes((ipfw_insn_u32 *)cmd);
				break;

			case O_ESTAB:
				printf(" established");
				break;

			case O_TCPDATALEN:
				if (F_LEN(cmd) == 1)
				    printf(" tcpdatalen %u", cmd->arg1 );
				else
				    print_newports((ipfw_insn_u16 *)cmd, 0,
					O_TCPDATALEN);
				break;

			case O_TCPFLAGS:
				print_flags("tcpflags", cmd, f_tcpflags);
				break;

			case O_TCPOPTS:
				print_flags("tcpoptions", cmd, f_tcpopts);
				break;

			case O_TCPWIN:
				printf(" tcpwin %d", ntohs(cmd->arg1));
				break;

			case O_TCPACK:
				printf(" tcpack %d", ntohl(cmd32->d[0]));
				break;

			case O_TCPSEQ:
				printf(" tcpseq %d", ntohl(cmd32->d[0]));
				break;

			case O_UID:
			    {
				struct passwd *pwd = getpwuid(cmd32->d[0]);

				if (pwd)
					printf(" uid %s", pwd->pw_name);
				else
					printf(" uid %u", cmd32->d[0]);
			    }
				break;

			case O_GID:
			    {
				struct group *grp = getgrgid(cmd32->d[0]);

				if (grp)
					printf(" gid %s", grp->gr_name);
				else
					printf(" gid %u", cmd32->d[0]);
			    }
				break;

			case O_JAIL:
				printf(" jail %d", cmd32->d[0]);
				break;

			case O_VERREVPATH:
				printf(" verrevpath");
				break;

			case O_VERSRCREACH:
				printf(" versrcreach");
				break;

			case O_ANTISPOOF:
				printf(" antispoof");
				break;

			case O_IPSEC:
				printf(" ipsec");
				break;

			case O_NOP:
				comment = (char *)(cmd + 1);
				break;

			case O_KEEP_STATE:
				printf(" keep-state");
				break;

			case O_LIMIT: {
				struct _s_x *p = limit_masks;
				ipfw_insn_limit *c = (ipfw_insn_limit *)cmd;
				uint8_t x = c->limit_mask;
				char const *comma = " ";

				printf(" limit");
				for (; p->x != 0 ; p++)
					if ((x & p->x) == p->x) {
						x &= ~p->x;
						printf("%s%s", comma, p->s);
						comma = ",";
					}
				PRINT_UINT_ARG(" ", c->conn_limit);
				break;
			}

			case O_IP6:
				printf(" ip6");
				break;

			case O_IP4:
				printf(" ip4");
				break;

			case O_ICMP6TYPE:
				print_icmp6types((ipfw_insn_u32 *)cmd);
				break;

			case O_EXT_HDR:
				print_ext6hdr( (ipfw_insn *) cmd );
				break;

			case O_TAGGED:
				if (F_LEN(cmd) == 1)
					PRINT_UINT_ARG(" tagged ", cmd->arg1);
				else
					print_newports((ipfw_insn_u16 *)cmd, 0,
					    O_TAGGED);
				break;

			default:
				printf(" [opcode %d len %d]",
				    cmd->opcode, cmd->len);
			}
		}
		if (cmd->len & F_OR) {
			printf(" or");
			or_block = 1;
		} else if (or_block) {
			printf(" }");
			or_block = 0;
		}
	}
	show_prerequisites(&flags, HAVE_IP, 0);
	if (comment)
		printf(" // %s", comment);
	printf("\n");
}

static void
show_dyn_ipfw(ipfw_dyn_rule *d, int pcwidth, int bcwidth)
{
	struct protoent *pe;
	struct in_addr a;
	uint16_t rulenum;
	char buf[INET6_ADDRSTRLEN];

	if (!do_expired) {
		if (!d->expire && !(d->dyn_type == O_LIMIT_PARENT))
			return;
	}
	bcopy(&d->rule, &rulenum, sizeof(rulenum));
	printf("%05d", rulenum);
	if (pcwidth>0 || bcwidth>0)
	    printf(" %*llu %*llu (%ds)", pcwidth,
		align_uint64(&d->pcnt), bcwidth,
		align_uint64(&d->bcnt), d->expire);
	switch (d->dyn_type) {
	case O_LIMIT_PARENT:
		printf(" PARENT %d", d->count);
		break;
	case O_LIMIT:
		printf(" LIMIT");
		break;
	case O_KEEP_STATE: /* bidir, no mask */
		printf(" STATE");
		break;
	}

	if ((pe = getprotobynumber(d->id.proto)) != NULL)
		printf(" %s", pe->p_name);
	else
		printf(" proto %u", d->id.proto);

	if (d->id.addr_type == 4) {
		a.s_addr = htonl(d->id.src_ip);
		printf(" %s %d", inet_ntoa(a), d->id.src_port);

		a.s_addr = htonl(d->id.dst_ip);
		printf(" <-> %s %d", inet_ntoa(a), d->id.dst_port);
	} else if (d->id.addr_type == 6) {
		printf(" %s %d", inet_ntop(AF_INET6, &d->id.src_ip6, buf,
		    sizeof(buf)), d->id.src_port);
		printf(" <-> %s %d", inet_ntop(AF_INET6, &d->id.dst_ip6, buf,
		    sizeof(buf)), d->id.dst_port);
	} else
		printf(" UNKNOWN <-> UNKNOWN\n");
	
	printf("\n");
}

static int
sort_q(const void *pa, const void *pb)
{
	int rev = (do_sort < 0);
	int field = rev ? -do_sort : do_sort;
	long long res = 0;
	const struct dn_flow_queue *a = pa;
	const struct dn_flow_queue *b = pb;

	switch (field) {
	case 1: /* pkts */
		res = a->len - b->len;
		break;
	case 2: /* bytes */
		res = a->len_bytes - b->len_bytes;
		break;

	case 3: /* tot pkts */
		res = a->tot_pkts - b->tot_pkts;
		break;

	case 4: /* tot bytes */
		res = a->tot_bytes - b->tot_bytes;
		break;
	}
	if (res < 0)
		res = -1;
	if (res > 0)
		res = 1;
	return (int)(rev ? res : -res);
}

static void
list_queues(struct dn_flow_set *fs, struct dn_flow_queue *q)
{
	int l;
	int index_printed, indexes = 0;
	char buff[255];
	struct protoent *pe;

	if (fs->rq_elements == 0)
		return;

	if (do_sort != 0)
		heapsort(q, fs->rq_elements, sizeof *q, sort_q);

	/* Print IPv4 flows */
	index_printed = 0;
	for (l = 0; l < fs->rq_elements; l++) {
		struct in_addr ina;

		/* XXX: Should check for IPv4 flows */
		if (IS_IP6_FLOW_ID(&(q[l].id)))
			continue;

		if (!index_printed) {
			index_printed = 1;
			if (indexes > 0)	/* currently a no-op */
				printf("\n");
			indexes++;
			printf("    "
			    "mask: 0x%02x 0x%08x/0x%04x -> 0x%08x/0x%04x\n",
			    fs->flow_mask.proto,
			    fs->flow_mask.src_ip, fs->flow_mask.src_port,
			    fs->flow_mask.dst_ip, fs->flow_mask.dst_port);

			printf("BKT Prot ___Source IP/port____ "
			    "____Dest. IP/port____ "
			    "Tot_pkt/bytes Pkt/Byte Drp\n");
		}

		printf("%3d ", q[l].hash_slot);
		pe = getprotobynumber(q[l].id.proto);
		if (pe)
			printf("%-4s ", pe->p_name);
		else
			printf("%4u ", q[l].id.proto);
		ina.s_addr = htonl(q[l].id.src_ip);
		printf("%15s/%-5d ",
		    inet_ntoa(ina), q[l].id.src_port);
		ina.s_addr = htonl(q[l].id.dst_ip);
		printf("%15s/%-5d ",
		    inet_ntoa(ina), q[l].id.dst_port);
		printf("%4qu %8qu %2u %4u %3u\n",
		    q[l].tot_pkts, q[l].tot_bytes,
		    q[l].len, q[l].len_bytes, q[l].drops);
		if (verbose)
			printf("   S %20qd  F %20qd\n",
			    q[l].S, q[l].F);
	}

	/* Print IPv6 flows */
	index_printed = 0;
	for (l = 0; l < fs->rq_elements; l++) {
		if (!IS_IP6_FLOW_ID(&(q[l].id)))
			continue;

		if (!index_printed) {
			index_printed = 1;
			if (indexes > 0)
				printf("\n");
			indexes++;
			printf("\n        mask: proto: 0x%02x, flow_id: 0x%08x,  ",
			    fs->flow_mask.proto, fs->flow_mask.flow_id6);
			inet_ntop(AF_INET6, &(fs->flow_mask.src_ip6),
			    buff, sizeof(buff));
			printf("%s/0x%04x -> ", buff, fs->flow_mask.src_port);
			inet_ntop( AF_INET6, &(fs->flow_mask.dst_ip6),
			    buff, sizeof(buff) );
			printf("%s/0x%04x\n", buff, fs->flow_mask.dst_port);

			printf("BKT ___Prot___ _flow-id_ "
			    "______________Source IPv6/port_______________ "
			    "_______________Dest. IPv6/port_______________ "
			    "Tot_pkt/bytes Pkt/Byte Drp\n");
		}
		printf("%3d ", q[l].hash_slot);
		pe = getprotobynumber(q[l].id.proto);
		if (pe != NULL)
			printf("%9s ", pe->p_name);
		else
			printf("%9u ", q[l].id.proto);
		printf("%7d  %39s/%-5d ", q[l].id.flow_id6,
		    inet_ntop(AF_INET6, &(q[l].id.src_ip6), buff, sizeof(buff)),
		    q[l].id.src_port);
		printf(" %39s/%-5d ",
		    inet_ntop(AF_INET6, &(q[l].id.dst_ip6), buff, sizeof(buff)),
		    q[l].id.dst_port);
		printf(" %4qu %8qu %2u %4u %3u\n",
		    q[l].tot_pkts, q[l].tot_bytes,
		    q[l].len, q[l].len_bytes, q[l].drops);
		if (verbose)
			printf("   S %20qd  F %20qd\n", q[l].S, q[l].F);
	}
}

static void
print_flowset_parms(struct dn_flow_set *fs, char *prefix)
{
	int l;
	char qs[30];
	char plr[30];
	char red[90];	/* Display RED parameters */

	l = fs->qsize;
	if (fs->flags_fs & DN_QSIZE_IS_BYTES) {
		if (l >= 8192)
			sprintf(qs, "%d KB", l / 1024);
		else
			sprintf(qs, "%d B", l);
	} else
		sprintf(qs, "%3d sl.", l);
	if (fs->plr)
		sprintf(plr, "plr %f", 1.0 * fs->plr / (double)(0x7fffffff));
	else
		plr[0] = '\0';
	if (fs->flags_fs & DN_IS_RED)	/* RED parameters */
		sprintf(red,
		    "\n\t  %cRED w_q %f min_th %d max_th %d max_p %f",
		    (fs->flags_fs & DN_IS_GENTLE_RED) ? 'G' : ' ',
		    1.0 * fs->w_q / (double)(1 << SCALE_RED),
		    SCALE_VAL(fs->min_th),
		    SCALE_VAL(fs->max_th),
		    1.0 * fs->max_p / (double)(1 << SCALE_RED));
	else
		sprintf(red, "droptail");

	printf("%s %s%s %d queues (%d buckets) %s\n",
	    prefix, qs, plr, fs->rq_elements, fs->rq_size, red);
}

static void
list_pipes(void *data, uint nbytes, int ac, char *av[])
{
	int rulenum;
	void *next = data;
	struct dn_pipe *p = (struct dn_pipe *) data;
	struct dn_flow_set *fs;
	struct dn_flow_queue *q;
	int l;

	if (ac > 0)
		rulenum = strtoul(*av++, NULL, 10);
	else
		rulenum = 0;
	for (; nbytes >= sizeof *p; p = (struct dn_pipe *)next) {
		double b = p->bandwidth;
		char buf[30];
		char prefix[80];

		if (SLIST_NEXT(p, next) != (struct dn_pipe *)DN_IS_PIPE)
			break;	/* done with pipes, now queues */

		/*
		 * compute length, as pipe have variable size
		 */
		l = sizeof(*p) + p->fs.rq_elements * sizeof(*q);
		next = (char *)p + l;
		nbytes -= l;

		if ((rulenum != 0 && rulenum != p->pipe_nr) || do_pipe == 2)
			continue;

		/*
		 * Print rate (or clocking interface)
		 */
		if (p->if_name[0] != '\0')
			sprintf(buf, "%s", p->if_name);
		else if (b == 0)
			sprintf(buf, "unlimited");
		else if (b >= 1000000)
			sprintf(buf, "%7.3f Mbit/s", b/1000000);
		else if (b >= 1000)
			sprintf(buf, "%7.3f Kbit/s", b/1000);
		else
			sprintf(buf, "%7.3f bit/s ", b);

		sprintf(prefix, "%05d: %s %4d ms ",
		    p->pipe_nr, buf, p->delay);
		print_flowset_parms(&(p->fs), prefix);
		if (verbose)
			printf("   V %20qd\n", p->V >> MY_M);

		q = (struct dn_flow_queue *)(p+1);
		list_queues(&(p->fs), q);
	}
	for (fs = next; nbytes >= sizeof *fs; fs = next) {
		char prefix[80];

		if (SLIST_NEXT(fs, next) != (struct dn_flow_set *)DN_IS_QUEUE)
			break;
		l = sizeof(*fs) + fs->rq_elements * sizeof(*q);
		next = (char *)fs + l;
		nbytes -= l;

		if (rulenum != 0 && ((rulenum != fs->fs_nr && do_pipe == 2) ||
		    (rulenum != fs->parent_nr && do_pipe == 1))) {
			continue;
		}

		q = (struct dn_flow_queue *)(fs+1);
		sprintf(prefix, "q%05d: weight %d pipe %d ",
		    fs->fs_nr, fs->weight, fs->parent_nr);
		print_flowset_parms(fs, prefix);
		list_queues(fs, q);
	}
}

static void
list(int ac, char *av[], int show_counters)
{
	struct ip_fw *r;
	ipfw_dyn_rule *dynrules, *d;

#define NEXT(r)	((struct ip_fw *)((char *)r + RULESIZE(r)))
	char *lim;
	void *data = NULL;
	int bcwidth, n, nbytes, nstat, ndyn, pcwidth, width;
	int exitval = EX_OK;
	int lac;
	char **lav;
	u_long rnum, last;
	char *endptr;
	int seen = 0;
	uint8_t set;

	const int ocmd = do_pipe ? IP_DUMMYNET_GET : IP_FW_GET;
	int nalloc = 1024;	/* start somewhere... */

	last = 0;

	if (test_only) {
		fprintf(stderr, "Testing only, list disabled\n");
		return;
	}

	ac--;
	av++;

	/* get rules or pipes from kernel, resizing array as necessary */
	nbytes = nalloc;

	while (nbytes >= nalloc) {
		nalloc = nalloc * 2 + 200;
		nbytes = nalloc;
		data = safe_realloc(data, nbytes);
		if (do_cmd(ocmd, data, (uintptr_t)&nbytes) < 0)
			err(EX_OSERR, "getsockopt(IP_%s_GET)",
				do_pipe ? "DUMMYNET" : "FW");
	}

	if (do_pipe) {
		list_pipes(data, nbytes, ac, av);
		goto done;
	}

	/*
	 * Count static rules. They have variable size so we
	 * need to scan the list to count them.
	 */
	for (nstat = 1, r = data, lim = (char *)data + nbytes;
		    r->rulenum < IPFW_DEFAULT_RULE && (char *)r < lim;
		    ++nstat, r = NEXT(r) )
		; /* nothing */

	/*
	 * Count dynamic rules. This is easier as they have
	 * fixed size.
	 */
	r = NEXT(r);
	dynrules = (ipfw_dyn_rule *)r ;
	n = (char *)r - (char *)data;
	ndyn = (nbytes - n) / sizeof *dynrules;

	/* if showing stats, figure out column widths ahead of time */
	bcwidth = pcwidth = 0;
	if (show_counters) {
		for (n = 0, r = data; n < nstat; n++, r = NEXT(r)) {
			/* skip rules from another set */
			if (use_set && r->set != use_set - 1)
				continue;

			/* packet counter */
			width = snprintf(NULL, 0, "%llu",
			    align_uint64(&r->pcnt));
			if (width > pcwidth)
				pcwidth = width;

			/* byte counter */
			width = snprintf(NULL, 0, "%llu",
			    align_uint64(&r->bcnt));
			if (width > bcwidth)
				bcwidth = width;
		}
	}
	if (do_dynamic && ndyn) {
		for (n = 0, d = dynrules; n < ndyn; n++, d++) {
			if (use_set) {
				/* skip rules from another set */
				bcopy((char *)&d->rule + sizeof(uint16_t),
				      &set, sizeof(uint8_t));
				if (set != use_set - 1)
					continue;
			}
			width = snprintf(NULL, 0, "%llu",
			    align_uint64(&d->pcnt));
			if (width > pcwidth)
				pcwidth = width;

			width = snprintf(NULL, 0, "%llu",
			    align_uint64(&d->bcnt));
			if (width > bcwidth)
				bcwidth = width;
		}
	}
	/* if no rule numbers were specified, list all rules */
	if (ac == 0) {
		for (n = 0, r = data; n < nstat; n++, r = NEXT(r)) {
			if (use_set && r->set != use_set - 1)
				continue;
			show_ipfw(r, pcwidth, bcwidth);
		}

		if (do_dynamic && ndyn) {
			printf("## Dynamic rules (%d):\n", ndyn);
			for (n = 0, d = dynrules; n < ndyn; n++, d++) {
				if (use_set) {
					bcopy((char *)&d->rule + sizeof(uint16_t),
					      &set, sizeof(uint8_t));
					if (set != use_set - 1)
						continue;
				}
				show_dyn_ipfw(d, pcwidth, bcwidth);
		}
		}
		goto done;
	}

	/* display specific rules requested on command line */

	for (lac = ac, lav = av; lac != 0; lac--) {
		/* convert command line rule # */
		last = rnum = strtoul(*lav++, &endptr, 10);
		if (*endptr == '-')
			last = strtoul(endptr+1, &endptr, 10);
		if (*endptr) {
			exitval = EX_USAGE;
			warnx("invalid rule number: %s", *(lav - 1));
			continue;
		}
		for (n = seen = 0, r = data; n < nstat; n++, r = NEXT(r) ) {
			if (r->rulenum > last)
				break;
			if (use_set && r->set != use_set - 1)
				continue;
			if (r->rulenum >= rnum && r->rulenum <= last) {
				show_ipfw(r, pcwidth, bcwidth);
				seen = 1;
			}
		}
		if (!seen) {
			/* give precedence to other error(s) */
			if (exitval == EX_OK)
				exitval = EX_UNAVAILABLE;
			warnx("rule %lu does not exist", rnum);
		}
	}

	if (do_dynamic && ndyn) {
		printf("## Dynamic rules:\n");
		for (lac = ac, lav = av; lac != 0; lac--) {
			last = rnum = strtoul(*lav++, &endptr, 10);
			if (*endptr == '-')
				last = strtoul(endptr+1, &endptr, 10);
			if (*endptr)
				/* already warned */
				continue;
			for (n = 0, d = dynrules; n < ndyn; n++, d++) {
				uint16_t rulenum;

				bcopy(&d->rule, &rulenum, sizeof(rulenum));
				if (rulenum > rnum)
					break;
				if (use_set) {
					bcopy((char *)&d->rule + sizeof(uint16_t),
					      &set, sizeof(uint8_t));
					if (set != use_set - 1)
						continue;
				}
				if (r->rulenum >= rnum && r->rulenum <= last)
					show_dyn_ipfw(d, pcwidth, bcwidth);
			}
		}
	}

	ac = 0;

done:
	free(data);

	if (exitval != EX_OK)
		exit(exitval);
#undef NEXT
}
