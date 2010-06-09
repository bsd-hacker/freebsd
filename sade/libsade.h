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
 *
 */

#ifndef _LIBSADE_H_
#define _LIBSADE_H_

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/mount.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <libufs.h>
#include <libgeom.h>

TAILQ_HEAD(de_devlist, de_device);
TAILQ_HEAD(de_partlist, de_part);
TAILQ_HEAD(de_fslist, de_fs);

struct de_device {
	char			*de_name;	/* device name */
	char			*de_desc;	/* device description */
	char			*de_class;	/* GEOM class name */
	char			*de_geom;	/* GEOM name */
	char			*de_scheme;	/* partition scheme name */
	TAILQ_ENTRY(de_device)	de_device;	/* devices list entry */
	struct de_partlist	de_part;	/* partitions list */
	off_t			de_mediasize;	/* provider mediasize */
	u_int			de_sectorsize;	/* provider sectorsize */
	u_int			de_state;	/* device state */

	u_int			de_flags;	/* device flags */
	void			*de_private;	/* device private data */
};

struct de_part {
	char			*de_name;	/* partition provider name */
	struct de_device	*de_device;	/* parent device */
	char			*de_type;	/* partition type */
	char			*de_label;	/* partition label */
	TAILQ_ENTRY(de_part)	de_part;	/* partitions list entry */
	off_t			de_start;	/* partition start LBA */
	off_t			de_end;		/* partition end LBA */
	u_int			de_index;	/* partition index */
	u_int			de_state;	/* partition state */

	u_int			de_flags;	/* partition flags */
	void			*de_private;	/* partition private data */
};

enum de_fstype {
	UNKNOWN, EMPTY, SWAP, UFS, ZFS
};

struct de_fs {
	TAILQ_ENTRY(de_fs)	de_fs;		/* file system list entry */
	enum de_fstype		de_type;	/* file system type */
	char			*de_parttype;	/* partition type */
	char			*de_partname;	/* partition name */
	char			*de_mntfrom;	/* device name */
	char			*de_mntto;	/* file system mount path */
	char			*de_mounted;	/* where it is mounted now */
	char			*de_mntops;	/* mount options */
	off_t			de_size;	/* file system size */

	char			*de_devname;	/* parent device name */
	char			*de_scheme;	/* partition scheme name */

	u_int			de_flags;	/* file system flags */
	void			*de_private;	/* file system private data */
};

struct de_ufs_priv {
	char			de_volname[MAXVOLLEN];	/* Volume label */

	int			de_ufs1:1;	/* UFS1 fs type */
	int			de_su:1;	/* Soft Updates enabled */
	int			de_suj:1;	/* Journalled Soft Updates enabled */
	int			de_gj:1;	/* GEOM Journal enabled */

	int			de_acl:1;	/* POSIX.1e ACL enabled */
	int			de_nfs4acl:1;	/* NFSv4 ACL enabled */
	int			de_mac:1;	/* MAC multilabel enabled */
};

/* device related functions */
int	de_devlist_get(struct de_devlist *pd);
int	de_devlist_partitioned_get(struct de_devlist *pd);
int	de_devlist_count(struct de_devlist *pd);
void	de_devlist_free(struct de_devlist *pd);
struct de_device *
	de_dev_find(struct de_devlist *pd, const char *name);
int	de_dev_scheme_create(struct de_device *pdev, const char *scheme);
int	de_dev_scheme_destroy(struct de_device *pdev);
int	de_dev_bootcode(struct de_device *pdev, const char *path);
int	de_dev_undo(struct de_device *pdev);
int	de_dev_commit(struct de_device *pdev);

char	**de_dev_aliases_get(const char *devname);
void	de_dev_aliases_free(char **labels);

/* partition related functions */
char	*de_error(void);
int	de_partlist_get(struct de_device *pdev);
int	de_partlist_count(struct de_partlist *partlist);
void	de_partlist_free(struct de_partlist *partlist);
void	de_dev_partlist_free(struct de_device *pdev);

int	de_part_setattr(struct de_device *pdev, const char *name, int idx);
int	de_part_unsetattr(struct de_device *, const char *name, int idx);
int	de_part_add(struct de_device *pdev, const char *type, off_t start,
    off_t size, const char* label, int idx);
//int	de_part_resize(struct de_device *pdev, const char *size, int idx);
int	de_part_del(struct de_device *pdev, int idx);
int	de_part_mod(struct de_device *pdev, const char *type,
    const char *label, int idx);
int	de_part_bootcode(struct de_part *ppart, const char *path);

/* file system related */
const char *de_fstypestr(enum de_fstype type);
int	de_fslist_get(struct de_fslist *fslist);
void	de_fslist_free(struct de_fslist *fslist);
int	de_fslist_count(struct de_fslist *fslist);


/* geom helpers */
struct gclass	*find_class(struct gmesh *mesh, const char *name);
struct ggeom	*find_geom(struct gclass *classp, const char *name);
const char	*find_geomcfg(struct ggeom *gp, const char *cfg);
const char	*find_provcfg(struct gprovider *pp, const char *cfg);
struct gprovider *find_provider(struct ggeom *gp, unsigned long long minsector);

#endif /* _SADE_H_ */
