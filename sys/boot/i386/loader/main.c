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
#include <stddef.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <sys/reboot.h>

#include "bootstrap.h"
#include "common/bootargs.h"
#include "libi386/libi386.h"
#include "libi386/smbios.h"
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

#ifdef LOADER_ZFS_SUPPORT
#include "../zfs/libzfs.h"
#endif

CTASSERT(sizeof(struct bootargs) == BOOTARGS_SIZE);
CTASSERT(offsetof(struct bootargs, bootinfo) == BA_BOOTINFO);
CTASSERT(offsetof(struct bootargs, bootflags) == BA_BOOTFLAGS);
CTASSERT(offsetof(struct bootinfo, bi_size) == BI_SIZE);

/* Arguments passed in from the boot1/boot2 loader */
static struct bootargs *kargs;

static u_int32_t	initial_howto;
static u_int32_t	initial_bootdev;
static struct bootinfo	*initial_bootinfo;

struct arch_switch	archsw;		/* MI/MD interface boundary */

static void		extract_currdev(void);
static int		isa_inb(int port);
static void		isa_outb(int port, int value);
void			exit(int code);
#ifdef LOADER_ZFS_SUPPORT
static void		i386_zfs_probe(void);
#endif

/* from vers.c */
extern	char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

/* XXX debugging */
extern char end[];

static void *heap_top;
static void *heap_bottom;

int
main(void)
{
    int			i;

    /* Pick up arguments */
    kargs = (void *)__args;
    initial_howto = kargs->howto;
    initial_bootdev = kargs->bootdev;
    initial_bootinfo = kargs->bootinfo ? (struct bootinfo *)PTOV(kargs->bootinfo) : NULL;

    /* Initialize the v86 register set to a known-good state. */
    bzero(&v86, sizeof(v86));
    v86.efl = PSL_RESERVED_DEFAULT | PSL_I;

    /* 
     * Initialise the heap as early as possible.  Once this is done, malloc() is usable.
     */
    bios_getmem();

#if defined(LOADER_BZIP2_SUPPORT) || defined(LOADER_FIREWIRE_SUPPORT) || \
    defined(LOADER_GPT_SUPPORT) || defined(LOADER_ZFS_SUPPORT)
    if (high_heap_size > 0) {
	heap_top = PTOV(high_heap_base + high_heap_size);
	heap_bottom = PTOV(high_heap_base);
	if (high_heap_base < memtop_copyin)
	    memtop_copyin = high_heap_base;
    } else
#endif
    {
	heap_top = (void *)PTOV(bios_basemem);
	heap_bottom = (void *)end;
    }
    setheap(heap_bottom, heap_top);

    /*
     * XXX Chicken-and-egg problem; we want to have console output early, but some
     * console attributes may depend on reading from eg. the boot device, which we
     * can't do yet.
     *
     * We can use printf() etc. once this is done.
     * If the previous boot stage has requested a serial console, prefer that.
     */
    bi_setboothowto(initial_howto);
    if (initial_howto & RB_MULTIPLE) {
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

    archsw.arch_autoload = i386_autoload;
    archsw.arch_getdev = i386_getdev;
    archsw.arch_copyin = i386_copyin;
    archsw.arch_copyout = i386_copyout;
    archsw.arch_readin = i386_readin;
    archsw.arch_isainb = isa_inb;
    archsw.arch_isaoutb = isa_outb;
#ifdef LOADER_ZFS_SUPPORT
    archsw.arch_zfs_probe = i386_zfs_probe;
#endif

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

    /* detect SMBIOS for future reference */
    smbios_detect(NULL);

    /* detect PCI BIOS for future reference */
    biospci_detect();

    printf("\n");
    printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
    printf("(%s, %s)\n", bootprog_maker, bootprog_date);

    extract_currdev();				/* set $currdev and $loaddev */
    setenv("LINES", "24", 1);			/* optional */
    
    bios_getsmap();

#ifdef LOADER_TFTP_SUPPORT
    if (kargs->bootflags & KARGS_FLAGS_PXE)
	interact(pxe_default_rc());
    else
#endif
    interact(NULL);

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
    struct i386_devdesc		new_currdev;
#ifdef LOADER_ZFS_SUPPORT
    char			buf[20];
    struct zfs_boot_args	*zargs;
#endif
    int				biosdev = -1;

    /* Assume we are booting from a BIOS disk by default */
    new_currdev.d_dev = &biosdisk;

    /* new-style boot loaders such as pxeldr and cdldr */
    if (kargs->bootinfo == 0) {
        if ((kargs->bootflags & KARGS_FLAGS_CD) != 0) {
	    /* we are booting from a CD with cdboot */
	    new_currdev.d_dev = &bioscd;
	    new_currdev.d_unit = bc_bios2unit(initial_bootdev);
	} else if ((kargs->bootflags & KARGS_FLAGS_PXE) != 0) {
	    /* we are booting from pxeldr */
	    new_currdev.d_dev = &pxedisk;
	    new_currdev.d_unit = 0;
	} else {
	    /* we don't know what our boot device is */
	    new_currdev.d_kind.biosdisk.slice = -1;
	    new_currdev.d_kind.biosdisk.partition = 0;
	    biosdev = -1;
	}
#ifdef LOADER_ZFS_SUPPORT
    } else if ((kargs->bootflags & KARGS_FLAGS_ZFS) != 0) {
	zargs = NULL;
	/* check for new style extended argument */
	if ((kargs->bootflags & KARGS_FLAGS_EXTARG) != 0)
	    zargs = (struct zfs_boot_args *)(kargs + 1);

	if (zargs != NULL &&
	    zargs->size >= offsetof(struct zfs_boot_args, primary_pool)) {
	    /* sufficient data is provided */
	    new_currdev.d_kind.zfs.pool_guid = zargs->pool;
	    new_currdev.d_kind.zfs.root_guid = zargs->root;
	    if (zargs->size >= sizeof(*zargs) && zargs->primary_vdev != 0) {
		sprintf(buf, "%llu", zargs->primary_pool);
		setenv("vfs.zfs.boot.primary_pool", buf, 1);
		sprintf(buf, "%llu", zargs->primary_vdev);
		setenv("vfs.zfs.boot.primary_vdev", buf, 1);
	    }
	} else {
	    /* old style zfsboot block */
	    new_currdev.d_kind.zfs.pool_guid = kargs->zfspool;
	    new_currdev.d_kind.zfs.root_guid = 0;
	}
	new_currdev.d_dev = &zfs_dev;
#endif
    } else if ((initial_bootdev & B_MAGICMASK) != B_DEVMAGIC) {
	/* The passed-in boot device is bad */
	new_currdev.d_kind.biosdisk.slice = -1;
	new_currdev.d_kind.biosdisk.partition = 0;
	biosdev = -1;
    } else {
	new_currdev.d_kind.biosdisk.slice = B_SLICE(initial_bootdev) - 1;
	new_currdev.d_kind.biosdisk.partition = B_PARTITION(initial_bootdev);
	biosdev = initial_bootinfo->bi_bios_dev;

	/*
	 * If we are booted by an old bootstrap, we have to guess at the BIOS
	 * unit number.  We will lose if there is more than one disk type
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
	((new_currdev.d_unit = bd_bios2unit(biosdev)) == -1)) {
	printf("Can't work out which disk we are booting from.\n"
	       "Guessed BIOS device 0x%x not found by probes, defaulting to disk0:\n", biosdev);
	new_currdev.d_unit = 0;
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
    printf("heap base at %p, top at %p, upper limit at %p\n", heap_bottom,
      sbrk(0), heap_top);
    return(CMD_OK);
}

#ifdef LOADER_ZFS_SUPPORT
COMMAND_SET(lszfs, "lszfs", "list child datasets of a zfs dataset",
    command_lszfs);

static int
command_lszfs(int argc, char *argv[])
{
    int err;

    if (argc != 2) {
	command_errmsg = "wrong number of arguments";
	return (CMD_ERROR);
    }

    err = zfs_list(argv[1]);
    if (err != 0) {
	command_errmsg = strerror(err);
	return (CMD_ERROR);
    }
    return (CMD_OK);
}
#endif

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
	    twiddle(1);
	    delay(10000);
	}
    }
    
    return (0);
}

#if 0
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
#endif

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
	
#if 0
    if (!strcmp(argv[1], "socket"))
	return command_sock(argc - 1, &argv[1]);
#endif
	
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
/* PXE_MORE end */
#endif

/* ISA bus access functions for PnP. */
static int
isa_inb(int port)
{

    return (inb(port));
}

static void
isa_outb(int port, int value)
{

    outb(port, value);
}

#ifdef LOADER_ZFS_SUPPORT
static void
i386_zfs_probe(void)
{
    char devname[32];
    int unit;

    /*
     * Open all the disks we can find and see if we can reconstruct
     * ZFS pools from them.
     */
    for (unit = 0; unit < MAXBDDEV; unit++) {
	if (bd_unit2bios(unit) == -1)
	    break;
	sprintf(devname, "disk%d:", unit);
	zfs_probe_dev(devname, NULL);
    }
}
#endif
