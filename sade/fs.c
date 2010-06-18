/*-
 * Copyright (c) 2010 Andrey V. Elsukov <bu7cher@yandex.ru>
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

#include <sys/types.h>
#include <sys/ucred.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <paths.h>
#include <fstab.h>
#include <libsade.h>

const char *
de_fstypestr(enum de_fstype type)
{
	switch (type) {
	case EMPTY:	return ("empty");
	case SWAP:	return ("swap");
	case UFS:	return ("ufs");
	case ZFS:	return ("zfs");
	default:	return ("unknown");
	}
	/* NOTREACHED */
	return (NULL);
}

static int
de_fslist_add(struct de_fslist *fslist, enum de_fstype type,
    const char *parttype, const char *partname, const char *devname,
    const char *scheme, const char *mntfrom, const char *mntto,
    const char *mounted, const char *mntops, off_t size,
    int freq, int pass, void *priv)
{
	struct de_fs *fs;

	fs = malloc(sizeof(*fs));
	if (fs == NULL)
		return (ENOMEM);
	bzero(fs, sizeof(*fs));
	if (parttype)
		fs->de_parttype = strdup(parttype);
	if (partname)
		fs->de_partname = strdup(partname);
	if (devname)
		fs->de_devname = strdup(devname);
	if (scheme)
		fs->de_scheme = strdup(scheme);
	if (mntfrom)
		fs->de_mntfrom = strdup(mntfrom);
	if (mntto)
		fs->de_mntto = strdup(mntto);
	if (mounted)
		fs->de_mounted = strdup(mounted);
	if (mntops)
		fs->de_mntops = strdup(mntops);
	fs->de_size = size;
	fs->de_type = type;
	fs->de_freq = freq;
	fs->de_pass = pass;
	fs->de_private = priv;
	TAILQ_INSERT_TAIL(fslist, fs, de_fs);
	return (0);
}

void
de_fslist_free(struct de_fslist *fslist)
{
	struct de_fs *fs;

	while (!TAILQ_EMPTY(fslist)) {
		fs = TAILQ_FIRST(fslist);
		free(fs->de_devname);
		free(fs->de_scheme);
		free(fs->de_parttype);
		free(fs->de_mntfrom);
		free(fs->de_mntto);
		free(fs->de_mounted);
		free(fs->de_mntops);
		free(fs->de_private);
		TAILQ_REMOVE(fslist, fs, de_fs);
		free(fs);
	}
}

char **
de_dev_aliases_get(const char *devname)
{
	struct gmesh mesh;
	struct gclass *cp;
	struct ggeom *gp;
	struct gprovider *pp;
	char **labels = NULL, **ptr;
	int error, cnt = 1, max = 5;

	assert(devname != NULL);
	error = geom_gettree(&mesh);
	if (error)
		return (NULL);
	cp = find_class(&mesh, "LABEL");
	if (cp == NULL) {
		geom_deletetree(&mesh);
		return (NULL);
	}
	ptr = (char **)malloc(sizeof(char *) * max);
	if (ptr == NULL)
		goto done;
	ptr[0] = strdup(devname);
	if (ptr[0] == NULL) {
		free(ptr);
		goto done;
	}
	labels = ptr;
	LIST_FOREACH(gp, &cp->lg_geom, lg_geom) {
		if (strcmp(gp->lg_name, devname) != 0)
			continue;
		pp = LIST_FIRST(&gp->lg_provider);
		if (pp == NULL)
			continue;
		if (cnt + 1 == max) {
			max += 5;
			ptr = (char **)realloc(labels, sizeof(char *) * max);
			if (ptr == NULL)
				break;
			labels = ptr;
		}
		labels[cnt] = strdup(pp->lg_name);
		if (labels[cnt] == NULL)
			break;
		cnt++;
	}
	labels[cnt] = NULL;
done:
	geom_deletetree(&mesh);
	return (labels);
}

void
de_dev_aliases_free(char **labels)
{
	int i;
	if (labels == NULL)
		return;
	for (i = 0; labels[i]; i++)
		free(labels[i]);
	free(labels);
}

static struct fstab *
de_fstab_get(char **labels)
{
	struct fstab *pfstab = NULL;
	int error, i;

	assert(labels != NULL);
	error = setfsent();
	if (error == 0)
		return (NULL);
	while((pfstab = getfsent()) != NULL) {
		if (strlen(pfstab->fs_spec) < sizeof(_PATH_DEV))
			continue;
		for (i = 0; labels[i] != NULL; i++)
			if (strcmp(pfstab->fs_spec + sizeof(_PATH_DEV) - 1,
			    labels[i]) == 0)
				return (pfstab);

	}
	return (NULL);
}

static struct statfs *
de_mntinfo_get(struct statfs *psfs, int cnt, char **labels)
{
	int i, j;

	assert(labels != NULL);
	if (cnt == 0 || psfs == NULL)
		return (NULL);
	for (j = 0; j < cnt; j++) {
		if (psfs[j].f_fstypename &&
		    strcmp(psfs[j].f_fstypename, "ufs") != 0)
			continue;
		if (strlen(psfs[j].f_mntfromname) < sizeof(_PATH_DEV))
			continue;
		for (i = 0; labels[i] != NULL; i++)
			if (strcmp(psfs[j].f_mntfromname +
			    sizeof(_PATH_DEV) - 1, labels[i]) == 0)
				return (psfs + j);

	}
	return (NULL);
}

static enum de_fstype
de_ufs_check(const char *pname, void **priv)
{
	struct de_ufs_priv *pu;
	struct fs *pfs;
	struct uufsd disk;
	enum de_fstype type;
	int error;

	assert(pname != NULL);

	bzero(&disk, sizeof(disk));
	error = ufs_disk_fillout(&disk, pname);
	if (error == -1) {
		if (errno == ENOENT)
			type = EMPTY;
		else
			type = UNKNOWN;
	} else {
		if (disk.d_ufs == 1 || disk.d_ufs == 2) {
			type = UFS;
			pu = malloc(sizeof(*pu));
			if (pu != NULL) {
				bzero(pu, sizeof(*pu));
				pfs = &disk.d_fs;
				pu->de_id[0] = pfs->fs_id[0];
				pu->de_id[1] = pfs->fs_id[1];
				pu->de_ufs1 = (disk.d_ufs == 1);
				pu->de_su = ((pfs->fs_flags & FS_DOSOFTDEP) != 0);
				pu->de_suj = ((pfs->fs_flags & FS_SUJ) != 0);
				pu->de_gj = ((pfs->fs_flags & FS_GJOURNAL) != 0);
				pu->de_acl = ((pfs->fs_flags & FS_ACLS) != 0);
				pu->de_nfs4acl = ((pfs->fs_flags & FS_NFS4ACLS) != 0);
				pu->de_mac = ((pfs->fs_flags & FS_MULTILABEL) != 0);
				strncpy(pu->de_volname, pfs->fs_volname, MAXVOLLEN);
				*priv = (void *)pu;
			}
		} else
			type = UNKNOWN;
		ufs_disk_close(&disk);
	}
	return (type);
}


int
de_fslist_get(struct de_fslist *fslist)
{
	struct de_devlist devices;
	struct de_device *pdev;
	struct de_part *ppart;
	struct fstab *pfstab;
	struct statfs *psfs, *pmsfs;
	enum de_fstype type;
	char *mounted, *mntto, *mntops, *mntfrom, **labels;
	int error, mntcnt, pass, freq;
	void *priv;

	assert(fslist != NULL);

	error = de_devlist_partitioned_get(&devices);
	if (error)
		return (error);

	TAILQ_INIT(fslist);
	mntcnt = getmntinfo(&psfs, MNT_NOWAIT);
	TAILQ_FOREACH(pdev, &devices, de_device) {
		error = de_partlist_get(pdev);
		if (error)
			break;
		TAILQ_FOREACH(ppart, &pdev->de_part, de_part) {
			/* skip empty chunks */
			if (ppart->de_type == NULL)
				continue;
			pass = freq = 0;
			priv = mounted = mntops = mntto = mntfrom = NULL;
			if (strcmp(ppart->de_type, "freebsd-swap") == 0) {
				type = SWAP;
			} else if (strcmp(ppart->de_type, "freebsd-ufs") == 0) {
				type = de_ufs_check(ppart->de_name, &priv);
#ifdef WITH_ZFS
			} else if (strcmp(ppart->de_type, "freebsd-zfs") == 0) {
#endif
			} else
				continue;
			if (type == SWAP || type == UFS) {
				labels = de_dev_aliases_get(ppart->de_name);
				pfstab = de_fstab_get(labels);
				if (pfstab) {
					mntto = pfstab->fs_file;
					mntops = pfstab->fs_mntops;
					mntfrom = pfstab->fs_spec;
					freq = pfstab->fs_freq;
					pass = pfstab->fs_passno;
				}
				if (type == UFS) {
					pmsfs = de_mntinfo_get(psfs, mntcnt, labels);
					if (pmsfs)
						mounted = pmsfs->f_mntonname;
				}
				de_dev_aliases_free(labels);
			}
			error = de_fslist_add(fslist, type, ppart->de_type,
			    ppart->de_name, pdev->de_name, pdev->de_scheme,
			    mntfrom, mntto, mounted, mntops, (1 + ppart->de_end -
				ppart->de_start) * pdev->de_sectorsize,
			    freq, pass, priv);
			if (error)
				break;
		}
		de_dev_partlist_free(pdev);
		if (error)
			break;
	}
	de_devlist_free(&devices);
	return (error);
}

int
de_fslist_count(struct de_fslist *fslist)
{
	int count = 0;
	struct de_fs *pfs;

	TAILQ_FOREACH(pfs, fslist, de_fs){
		count++;
	}
	return (count);
}

int
de_fs_newfs(struct de_fs *pfs)
{
	return (0);
}


