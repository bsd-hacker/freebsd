/*-
 * Copyright (c) 2017 Rick Macklem
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

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/extattr.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <nfs/nfssvc.h>

#include <fs/nfs/nfsproto.h>
#include <fs/nfs/nfskpiport.h>
#include <fs/nfs/nfs.h>
#include <fs/nfs/nfsrvstate.h>

static void usage(void);

/*
 * This program creates a copy of the file's (first argument) data on the
 * new/recovering DS mirror.  If the file is already on the new/recovering
 * DS, it will simply exit(0).
 */
int
main(int argc, char *argv[])
{
	struct nfsd_pnfsd_args pnfsdarg;
	struct pnfsdsfile dsfile[NFSDEV_MAXMIRRORS];
	struct stat sb;
	struct statfs sf;
	struct addrinfo hints, *res, *nres;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	ssize_t xattrsize;
	int fnd, i, mirrorcnt, ret;
	char host[MNAMELEN + NI_MAXHOST + 2], *cp;

	if (argc != 4)
		usage();
	if (geteuid() != 0)
		errx(1, "Must be run as root/su");

	/*
	 * The host address and directory where the data storage file is
	 * located is in the extended attribute "pnfsd.dsfile".
	 */
	xattrsize = extattr_get_file(argv[1], EXTATTR_NAMESPACE_SYSTEM,
	    "pnfsd.dsfile", dsfile, sizeof(dsfile));
	mirrorcnt = xattrsize / sizeof(struct pnfsdsfile);
	if (mirrorcnt < 1 || xattrsize != mirrorcnt * sizeof(struct pnfsdsfile))
		errx(1, "Can't get extattr pnfsd.dsfile for %s", argv[1]);

	/* Check the second argument to see that it is an NFS mount point. */
	if (stat(argv[2], &sb) < 0)
		errx(1, "Can't stat %s", argv[2]);
	if (!S_ISDIR(sb.st_mode))
		errx(1, "%s is not a directory", argv[2]);
	if (statfs(argv[2], &sf) < 0)
		errx(1, "Can't fsstat %s", argv[2]);
	if (strcmp(sf.f_fstypename, "nfs") != 0)
		errx(1, "%s is not an NFS mount", argv[2]);
	if (strcmp(sf.f_mntonname, argv[2]) != 0)
		errx(1, "%s is not the mounted-on dir for the new DS", argv[2]);

	/*
	 * Check the IP address of the NFS server against the entrie(s) in
	 * the extended attribute.
	 */
	strlcpy(host, sf.f_mntfromname, sizeof(host));
	cp = strchr(host, ':');
	if (cp == NULL)
		errx(1, "No <host>: in mount %s", host);
	*cp = '\0';
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, NULL, &hints, &res) != 0)
		errx(1, "Can't get address for %s", host);
	for (i = 0; i < mirrorcnt; i++) {
		nres = res;
		while (nres != NULL) {
			if (dsfile[i].dsf_sin.sin_family == nres->ai_family) {
				/*
				 * If there is already an entry for this
				 * DS, just exit(0), since copying isn't
				 * required.
				 */
				if (nres->ai_family == AF_INET) {
					sin = (struct sockaddr_in *)
					    nres->ai_addr;
					if (sin->sin_addr.s_addr ==
					    dsfile[i].dsf_sin.sin_addr.s_addr)
						exit(0);
				} else if (nres->ai_family == AF_INET6) {
					sin6 = (struct sockaddr_in6 *)
					    nres->ai_addr;
					if (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &dsfile[i].dsf_sin6.sin6_addr))
						exit(0);
				}
			}
			nres = nres->ai_next;
		}
	}
	freeaddrinfo(res);

	/* Check the third argument to see that it is an NFS mount point. */
	if (stat(argv[3], (struct stat *)&sb) < 0)
		errx(1, "Can't stat %s", argv[3]);
	if (!S_ISDIR(sb.st_mode))
		errx(1, "%s is not a directory", argv[3]);
	if (statfs(argv[3], (struct statfs *)&sf) < 0)
		errx(1, "Can't fsstat %s", argv[3]);
	if (strcmp(sf.f_fstypename, "nfs") != 0)
		errx(1, "%s is not an NFS mount", argv[3]);
	if (strcmp(sf.f_mntonname, argv[3]) != 0)
		errx(1, "%s is not the mounted-on dir of the cur DS", argv[3]);

	/*
	 * Check the IP address of the NFS server against the entrie(s) in
	 * the extended attribute.
	 */
	strlcpy(host, sf.f_mntfromname, sizeof(host));
	cp = strchr(host, ':');
	if (cp == NULL)
		errx(1, "No <host>: in mount %s", host);
	*cp = '\0';
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, NULL, &hints, &res) != 0)
		errx(1, "Can't get address for %s", host);
	fnd = 0;
	for (i = 0; i < mirrorcnt && fnd == 0; i++) {
		nres = res;
		while (nres != NULL) {
			if (dsfile[i].dsf_sin.sin_family == nres->ai_family) {
				/*
				 * If there is already an entry for this
				 * DS, just exit(0), since copying isn't
				 * required.
				 */
				if (nres->ai_family == AF_INET) {
					sin = (struct sockaddr_in *)
					    nres->ai_addr;
					if (sin->sin_addr.s_addr ==
					    dsfile[i].dsf_sin.sin_addr.s_addr) {
						fnd = 1;
						break;
					}
				} else if (nres->ai_family == AF_INET6) {
					sin6 = (struct sockaddr_in6 *)
					    nres->ai_addr;
					if (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &dsfile[i].dsf_sin6.sin6_addr)) {
						fnd = 1;
						break;
					}
				}
			}
			nres = nres->ai_next;
		}
	}
	freeaddrinfo(res);
	/*
	 * If not found, just exit(0) since this file isn't stored on the
	 * current mirror and, therefore, isn't stored on this mirror set.
	 */
	if (fnd == 0)
		exit(0);

	/* Do the copy via the nfssvc() syscall. */
	pnfsdarg.op = PNFSDOP_COPYMR;
	pnfsdarg.mdspath = argv[1];
	pnfsdarg.dspath = argv[2];
	pnfsdarg.curdspath = argv[3];
	ret = nfssvc(NFSSVC_PNFSDS, &pnfsdarg);
	if (ret < 0 && errno != EEXIST)
		err(1, "Copymr failed args %s, %s, %s", argv[1], argv[2],
		    argv[3]);
	exit(0);
}

static void
usage(void)
{

	fprintf(stderr, "pnfsdscopymr <mds-filename> "
	    "<recovered-DS-mounted-on-path> <current-DS-mounted-on-path>\n");
	exit(1);
}

