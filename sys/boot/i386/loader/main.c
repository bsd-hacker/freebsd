/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * MD bootstrap main() and assorted miscellaneous
 * commands.
 */

#include <stand.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <sys/reboot.h>

#include "bootstrap.h"
#include "libi386/libi386.h"
#include "btxv86.h"

#ifdef PXE_MORE
#include "pxe_arp.h"
#include "pxe_connection.h"
#include "pxe_dns.h"
#include "pxe_filter.h"
#include "pxe_http.h"
#include "pxe_icmp.h"
#include "pxe_ip.h"
#include "pxe_sock.h"
#include "pxe_tcp.h"
#include "pxe_udp.h"
#endif

#define	KARGS_FLAGS_CD		0x1
#define	KARGS_FLAGS_PXE		0x2

/* Arguments passed in from the boot1/boot2 loader */
static struct 
{
    u_int32_t	howto;
    u_int32_t	bootdev;
    u_int32_t	bootflags;
    u_int32_t	pxeinfo;
    u_int32_t	res2;
    u_int32_t	bootinfo;
} *kargs;

static u_int32_t	initial_howto;
static u_int32_t	initial_bootdev;
static struct bootinfo	*initial_bootinfo;

struct arch_switch	archsw;		/* MI/MD interface boundary */

static void		extract_currdev(void);
static int		isa_inb(int port);
static void		isa_outb(int port, int value);
void			exit(int code);

/* from vers.c */
extern	char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

/* XXX debugging */
extern char end[];

int
main(void)
{
    int			i;

    /* Pick up arguments */
    kargs = (void *)__args;
    initial_howto = kargs->howto;
    initial_bootdev = kargs->bootdev;
    initial_bootinfo = kargs->bootinfo ? (struct bootinfo *)PTOV(kargs->bootinfo) : NULL;

    /* 
     * Initialise the heap as early as possible.  Once this is done, malloc() is usable.
     */
    bios_getmem();

#if defined(LOADER_HTTP_SUPPORT) || defined (LOADER_TFTP_SUPPORT) || \
    defined(LOADER_NFS_SUPPORT)
    /* 0x8d000-0x9f800 memory area is used by UNDI/BC data/code segments
     * and PXE stack
     */
    setheap((void *)end, 0x8d000);    
#else
    setheap((void *)end, (void *)bios_basemem);
#endif
    
    /* 
     * XXX Chicken-and-egg problem; we want to have console output early, but some
     * console attributes may depend on reading from eg. the boot device, which we
     * can't do yet.
     *
     * We can use printf() etc. once this is done.
     * If the previous boot stage has requested a serial console, prefer that.
     */
    if (initial_howto & RB_MULTIPLE) {
	setenv("boot_multicons", "YES", 1);
	if (initial_howto & RB_SERIAL)
	    setenv("console", "comconsole vidconsole", 1);
	else
	    setenv("console", "vidconsole comconsole", 1);
    } else if (initial_howto & RB_SERIAL)
	setenv("console", "comconsole", 1);
    else if (initial_howto & RB_MUTE)
	setenv("console", "nullconsole", 1);
    cons_probe();

    /*
     * Initialise the block cache
     */
    bcache_init(32, 512);	/* 16k cache XXX tune this */

    /*
     * Special handling for PXE and CD booting.
     */
    if (kargs->bootinfo == 0) {
	/*
	 * We only want the PXE disk to try to init itself in the below
	 * walk through devsw if we actually booted off of PXE.
	 */
	if (kargs->bootflags & KARGS_FLAGS_PXE)
	    pxe_enable(kargs->pxeinfo ? PTOV(kargs->pxeinfo) : NULL);
	else if (kargs->bootflags & KARGS_FLAGS_CD)
	    bc_add(initial_bootdev);
    }

    /*
     * March through the device switch probing for things.
     */
    for (i = 0; devsw[i] != NULL; i++)
	if (devsw[i]->dv_init != NULL)
	    (devsw[i]->dv_init)();
    printf("BIOS %dkB/%dkB available memory\n", bios_basemem / 1024, bios_extmem / 1024);
    if (initial_bootinfo != NULL) {
	initial_bootinfo->bi_basemem = bios_basemem / 1024;
	initial_bootinfo->bi_extmem = bios_extmem / 1024;
    }

    /* detect ACPI for future reference */
    biosacpi_detect();

    printf("\n");
    printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
    printf("(%s, %s)\n", bootprog_maker, bootprog_date);

    extract_currdev();				/* set $currdev and $loaddev */
    setenv("LINES", "24", 1);			/* optional */
    
    bios_getsmap();

    archsw.arch_autoload = i386_autoload;
    archsw.arch_getdev = i386_getdev;
    archsw.arch_copyin = i386_copyin;
    archsw.arch_copyout = i386_copyout;
    archsw.arch_readin = i386_readin;
    archsw.arch_isainb = isa_inb;
    archsw.arch_isaoutb = isa_outb;

    interact();			/* doesn't return */

    /* if we ever get here, it is an error */
    return (1);
}

/*
 * Set the 'current device' by (if possible) recovering the boot device as 
 * supplied by the initial bootstrap.
 *
 * XXX should be extended for netbooting.
 */
static void
extract_currdev(void)
{
    struct i386_devdesc	new_currdev;
    int			major, biosdev = -1;

    /* Assume we are booting from a BIOS disk by default */
    new_currdev.d_dev = &biosdisk;

    /* new-style boot loaders such as pxeldr and cdldr */
    if (kargs->bootinfo == 0) {
        if ((kargs->bootflags & KARGS_FLAGS_CD) != 0) {
	    /* we are booting from a CD with cdboot */
	    new_currdev.d_dev = &bioscd;
	    new_currdev.d_kind.bioscd.unit = bc_bios2unit(initial_bootdev);
	} else if ((kargs->bootflags & KARGS_FLAGS_PXE) != 0) {
	    /* we are booting from pxeldr */
	    new_currdev.d_dev = &pxedisk;
	    new_currdev.d_kind.netif.unit = 0;
	} else {
	    /* we don't know what our boot device is */
	    new_currdev.d_kind.biosdisk.slice = -1;
	    new_currdev.d_kind.biosdisk.partition = 0;
	    biosdev = -1;
	}
    } else if ((initial_bootdev & B_MAGICMASK) != B_DEVMAGIC) {
	/* The passed-in boot device is bad */
	new_currdev.d_kind.biosdisk.slice = -1;
	new_currdev.d_kind.biosdisk.partition = 0;
	biosdev = -1;
    } else {
	new_currdev.d_kind.biosdisk.slice = (B_ADAPTOR(initial_bootdev) << 4) +
					     B_CONTROLLER(initial_bootdev) - 1;
	new_currdev.d_kind.biosdisk.partition = B_PARTITION(initial_bootdev);
	biosdev = initial_bootinfo->bi_bios_dev;
	major = B_TYPE(initial_bootdev);

	/*
	 * If we are booted by an old bootstrap, we have to guess at the BIOS
	 * unit number.  We will loose if there is more than one disk type
	 * and we are not booting from the lowest-numbered disk type 
	 * (ie. SCSI when IDE also exists).
	 */
	if ((biosdev == 0) && (B_TYPE(initial_bootdev) != 2))	/* biosdev doesn't match major */
	    biosdev = 0x80 + B_UNIT(initial_bootdev);		/* assume harddisk */
    }
    new_currdev.d_type = new_currdev.d_dev->dv_type;
    
    /*
     * If we are booting off of a BIOS disk and we didn't succeed in determining
     * which one we booted off of, just use disk0: as a reasonable default.
     */
    if ((new_currdev.d_type == biosdisk.dv_type) &&
	((new_currdev.d_kind.biosdisk.unit = bd_bios2unit(biosdev)) == -1)) {
	printf("Can't work out which disk we are booting from.\n"
	       "Guessed BIOS device 0x%x not found by probes, defaulting to disk0:\n", biosdev);
	new_currdev.d_kind.biosdisk.unit = 0;
    }
    env_setenv("currdev", EV_VOLATILE, i386_fmtdev(&new_currdev),
	       i386_setcurrdev, env_nounset);
    env_setenv("loaddev", EV_VOLATILE, i386_fmtdev(&new_currdev), env_noset,
	       env_nounset);
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{
    int i;

    for (i = 0; devsw[i] != NULL; ++i)
	if (devsw[i]->dv_cleanup != NULL)
	    (devsw[i]->dv_cleanup)();

    printf("Rebooting...\n");
    delay(1000000);
    __exit(0);
}

/* provide this for panic, as it's not in the startup code */
void
exit(int code)
{
    __exit(code);
}

COMMAND_SET(heap, "heap", "show heap usage", command_heap);

static int
command_heap(int argc, char *argv[])
{
    mallocstats();
    printf("heap base at %p, top at %p\n", end, sbrk(0));
    return(CMD_OK);
}

/* added for pxe_http */
#ifdef PXE_MORE
static int
command_route(int argc, char *argv[])
{
    PXE_IPADDR net;
    PXE_IPADDR gw;

    if (argc < 2) {
	printf("use: route add|del|print [default|net_addr gw_addr] \n");
	return (CMD_OK);
    } 

    if (!strcmp(argv[1], "print")) {
    	pxe_ip_route_stat();
	return (CMD_OK);
    }
    
    if (argc < 4) {
	printf("use: route add|del default|net_addr gw_addr\n");
	return (CMD_OK);
    }
	
    if ( (strcmp(argv[1], "add") != 0) && (strcmp(argv[1], "del") != 0))
    	return (CMD_OK);
    
    if (!strcmp(argv[2], "default")) {
	
	if (!strcmp(argv[1], "del")) {
	    printf("Cannot delete default gateway.\n");
	    return (CMD_OK);
	}
		
	gw.ip = pxe_convert_ipstr(argv[3]);
		
	pxe_ip_route_default(&gw);
	    
	return (CMD_OK);
    }
	
    gw.ip = pxe_convert_ipstr(argv[3]);
    net.ip = pxe_convert_ipstr(argv[2]);
	
    if (!strcmp(argv[1], "add")) {
	pxe_ip_route_add(&net, pxe_ip_get_netmask(&net), &gw);
	return (CMD_OK);
    }

    pxe_ip_route_del(&net, pxe_ip_get_netmask(&net), &gw);
    
    return (CMD_OK);
}

static int
command_arp(int argc, char *argv[])
{
    PXE_IPADDR *ip;

    if (argc > 1) {
	
	if (strcmp(argv[1], "stats") != 0)
	    ip = pxe_gethostbyname(argv[1]);
	else {
	    pxe_arp_stats();
	    return (CMD_OK);
	}

    } else {
	printf("use: arp ip4_address|stats\n");
	return (CMD_OK);
    }
    
    printf("searching ip: %s\n", (ip != NULL) ? inet_ntoa(ip->ip) : "?");
    
    const uint8_t* mac = (const uint8_t *)pxe_arp_ip4mac(ip);

    if (mac != NULL)
           printf("MAC: %6D\n", mac, ":");
    else
	   printf("MAC search failed.\n");
								   
    return (CMD_OK);
}

static int
command_ping(int argc, char *argv[])
{
    PXE_IPADDR* ip = NULL;

    pxe_icmp_init();

    if (argc > 1)
	ip = pxe_gethostbyname(argv[1]);
    else {
	printf("use: ping ip4_address\n");
	return (CMD_OK);
    }

    pxe_ping(ip, 5, 1);
								   
    return (CMD_OK);
}

static int
command_await()
{

    while (1) {
        if (!pxe_core_recv_packets()) {
	    twiddle();
	    delay(10000);
	}
    }
    
    return (0);
}

static int
command_sock(int argc, char *argv[])
{
    if (argc < 2) {
    	printf("use: socket stats|tcptest\n");
	return (CMD_OK);
    }
    
    if (!strcmp(argv[1], "stats")) {
	pxe_sock_stats();
	return (CMD_OK);
    }
    
    if (argc < 3) {
    	printf("use: socket tcptest ip4.addr\n");
	return (CMD_OK);
    }
    
    if (!strcmp(argv[1], "tcptest")) {
	int socket = pxe_socket();
	PXE_IPADDR	ip;
	uint32_t	bps = 0;
	uint32_t	start_time = 0;
	int		index2 = 0;	
	    
	ip.ip = pxe_convert_ipstr(argv[2]);
	    
	int res = pxe_connect(socket, &ip, 26, PXE_TCP_PROTOCOL);

	if (res == -1)
	    printf("tcptest: failed to connect socket.\n");
	else {
	    int index = 0;
	    int recvc = 0;
	    uint8_t  data;
	    start_time = pxe_get_secs();

	    index = 0;
	    recvc = 0;
		    
	    data = 1;

	    while ( data != 0) {
	        recvc = pxe_recv(socket, &data, 1);
			    
	        if (recvc == -1) {
		    printf("tcptest: %d bytes recv, but next failed.\n", index);
		    pxe_close(socket);
		    return (CMD_OK);
		}
			    
		if (recvc == 0) {
		    printf("!");
		    continue;
		}
			    
		recvc = (index % 149) + 1; 

		if (data == 0) {
		    printf("tcptest: end of test.\n");
		    break;
		}
			    
		if (data != recvc)
		    printf("tcptest: error: step %d, waited %d, got %d.\n",
		        index, recvc, data);

		index += 1;
		index2 += 1;
			    
		if ( index2 > 100000) {
		    uint32_t delta = pxe_get_secs() - start_time;
		    bps = ((double)index) / ((delta != 0) ? delta : 1);

		    printf("tcptest: %d bytes received, %d bytes/sec.\n",
		        index, bps);
			    
		    index2 = 0;
		}
	    }
		    
	    uint32_t delta = pxe_get_secs() - start_time;
	    bps = ((double)index) / ((delta != 0) ? delta : 1);
		    
	    printf("tcptest: ok (recv: %d), avg %d bytes/sec.\n", index, bps);
	}
	pxe_close(socket);
    }
    
    return (CMD_OK);
}

static int
command_resolve(int argc, char *argv[])
{
    if (argc < 2) {
    	printf("use: resolve dns_name\n");
	return (CMD_OK);
    }
    
    PXE_IPADDR	*ip;
    
    char*	name = argv[1];

    ip = pxe_gethostbyname(name);
    
    if ( (ip == NULL) || (ip->ip == 0))
        printf("failed to resolve domain %s\n", name);
    else
	printf("%s resolved as %s\n", name, inet_ntoa(ip->ip));
    
    return (CMD_OK);
}

static int
command_ns(int argc, char *argv[])
{
    PXE_IPADDR	*ip;
    
    if (argc == 1) {
	ip = pxe_get_ip(PXE_IP_NAMESERVER);
	
    	printf("primary nameserver: %s\n", inet_ntoa(ip->ip));

	return (CMD_OK);
    }
    
    PXE_IPADDR	addr;
    
    addr.ip = pxe_convert_ipstr(argv[1]);
    
    if (addr.ip != 0)
	pxe_set_ip(PXE_IP_NAMESERVER, &addr);
    else
	printf("Syntax error in ip address.\n");
    
    return (CMD_OK);
}

static int
command_fetch(int argc, char *argv[])
{
    if (argc == 1) {
	printf("usage: fetch server/path/to/file.ext\n");
	return (CMD_OK);
    }

    char *server_name = argv[1];
    char *filename = server_name;

    while (*filename) {
        if (*filename == '/') {
	    *filename = '\0';
	    ++filename;
	    break;
        }
        ++filename;
    }
    
    /* retrieve all file */
    pxe_fetch(server_name, filename, 0LL, 0L);
    
    return (CMD_OK);
}

COMMAND_SET(pxe, "pxe", "pxe test module", command_pxe);

static int
command_pxe(int argc, char *argv[])
{
    if (argc<2) {
        printf("PXE test module (built at %s %s)\n", __DATE__, __TIME__);
        printf("  use: pxe arp|await|connections|fetch|filters|\n"
	       "\tping|resolve|route|socket\n");
	return (CMD_OK);
    } 

    if (!strcmp(argv[1], "arp"))
	return command_arp(argc - 1, &argv[1]);
	
    if (!strcmp(argv[1], "ping"))
	return command_ping(argc - 1, &argv[1]);
	
    if (!strcmp(argv[1], "route"))
	return command_route(argc - 1, &argv[1]);

    if (!strcmp(argv[1], "filters")) {
	pxe_filter_stats();
	return (CMD_OK);
    }
	
    if (!strcmp(argv[1], "socket"))
	return command_sock(argc - 1, &argv[1]);
	
    if (!strcmp(argv[1], "resolve"))
	return command_resolve(argc - 1, &argv[1]);
	
    if (!strcmp(argv[1], "ns"))
	return command_ns(argc - 1, &argv[1]);
	
	
    if (!strcmp(argv[1], "await"))
	return command_await();
	
    if (!strcmp(argv[1], "connections")) {
	pxe_connection_stats();
	return (CMD_OK);
    }
    
    if (!strcmp(argv[1], "fetch")) {
	return command_fetch(argc - 1, &argv[1]);
    }

    printf("unknown pxe command '%s'\n", argv[1]);
    
    return (CMD_OK);
}
/* pxe_http add end */
#endif

/* ISA bus access functions for PnP, derived from <machine/cpufunc.h> */
static int		
isa_inb(int port)
{
    u_char	data;
    
    if (__builtin_constant_p(port) && 
	(((port) & 0xffff) < 0x100) && 
	((port) < 0x10000)) {
	__asm __volatile("inb %1,%0" : "=a" (data) : "id" ((u_short)(port)));
    } else {
	__asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
    }
    return(data);
}

static void
isa_outb(int port, int value)
{
    u_char	al = value;
    
    if (__builtin_constant_p(port) && 
	(((port) & 0xffff) < 0x100) && 
	((port) < 0x10000)) {
	__asm __volatile("outb %0,%1" : : "a" (al), "id" ((u_short)(port)));
    } else {
        __asm __volatile("outb %0,%%dx" : : "a" (al), "d" (port));
    }
}

