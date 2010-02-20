/* $Id: fragc.c,v 1.9 2010/02/07 14:32:22 kostik Exp kostik $ */

/* /usr/local/opt/gcc-4.4.3/bin/gcc -g -Wall -Wextra -O -o fragc fragc.c -lufs */

#include <sys/param.h>
#include <sys/mount.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <libufs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const int blocksz = 512;

static int verbose;

static void
usage(void)
{

	fprintf(stderr, "Usage: fragc [-v] devname\n");
}

static ufs2_daddr_t blks_total;
static ufs2_daddr_t blks_breaks;

static void
block_pair(struct fs *fs, ufs2_daddr_t *prev, ufs2_daddr_t curr)
{

	blks_total++;
	if (curr != 0) {
		if (*prev != 0 &&
		    (*prev) + fs->fs_bsize / fs->fs_fsize != curr) {
			blks_breaks++;
			if (verbose)
				putchar('|');
		}
		if (verbose)
			printf(" %jd", (intmax_t)curr);
	}
	*prev = curr;
}

static void
count_indir(struct uufsd *u, struct fs *fs, int level, int maxlevel,
    ufs2_daddr_t ib, ufs2_daddr_t *prev)
{
	ufs2_daddr_t *b;
	unsigned i;

	if (ib == 0)
		return;
	b = malloc(fs->fs_bsize);
	if (bread(u, ib * fs->fs_fsize / blocksz, b, fs->fs_bsize) == -1) {
		printf("\nRead block %jd: %s\n", (intmax_t)ib, u->d_error);
		goto out;
	}
	for (i = 0; i < fs->fs_bsize / sizeof(ufs2_daddr_t); i++) {
		if (level == maxlevel)
			block_pair(fs, prev, b[i]);
		else
			count_indir(u, fs, level + 1, maxlevel, b[i], prev);
	}
 out:
	free(b);
}

static void
count_ino_ufs1(struct uufsd *u, struct fs *fs, struct ufs1_dinode *dp)
{
	ufs2_daddr_t prev;
	unsigned i;

	if (dp->di_size == 0)
		return;
	if ((dp->di_mode & IFMT) == IFLNK && dp->di_size <
	    (u_int64_t)fs->fs_maxsymlinklen)
		return;

	prev = 0;
	for (i = 0; i < NDADDR; i++)
		block_pair(fs, &prev, dp->di_db[i]);
	for (i = 0; i < NIADDR; i++) {
		if (0 && verbose)
			printf(" [%d]", dp->di_ib[i]);
		count_indir(u, fs, 0, i, dp->di_ib[i], &prev);
	}
}

static void
count_ino_ufs2(struct uufsd *u, struct fs *fs, struct ufs2_dinode *dp)
{
	ufs2_daddr_t prev;
	unsigned i;

	if (dp->di_size == 0)
		return;
	if ((dp->di_mode & IFMT) == IFLNK && dp->di_size <
	    (u_int64_t)fs->fs_maxsymlinklen)
		return;

	prev = 0;
	for (i = 0; i < NDADDR; i++)
		block_pair(fs, &prev, dp->di_db[i]);
	for (i = 0; i < NIADDR; i++) {
		if (0 && verbose)
			printf(" [%jd]", (intmax_t)(dp->di_ib[i]));
		count_indir(u, fs, 0, i, dp->di_ib[i], &prev);
	}
}

static void
frag_calc(struct uufsd *u)
{
	struct fs *fs;
	struct cg *cg;
	void *dino;
	int32_t cgno;
	uint32_t ino, inoused, cgino, next_cg_ino;
	int mode;
	u_int8_t *cp;

	fs = &u->d_fs;
	if (verbose)
		printf("%s UFS%d\n", u->d_name, u->d_ufs);
	ino = 0;
	for (cgno = 0; cgread(u); cgno++) {
		cg = &u->d_cg;
		if (u->d_ufs == 1)
			inoused = fs->fs_ipg;
		else
			inoused = cg->cg_initediblk;
		if (verbose)
			printf("cg %d inodes %u\n", cgno, inoused);
		cp = cg_inosused(cg);
		next_cg_ino = ino + fs->fs_ipg;
		for (cgino = 0; cgino < inoused; cgino++, ino++) {
			if ((cp[cgino / CHAR_BIT] & (1 << (cgino % CHAR_BIT)))
			    != 0 && ino != 0 && ino != 1) {
				if (verbose)
					printf("  ino %u:", ino);
				if (getino(u, &dino, ino, &mode) == -1) {
					printf("\nReading ino %u: %s\n",
					    ino, u->d_error);
					return;
				}
				if (mode == 0) {
					printf(
"\nIno %u/%u is allocated in bitmap, but mode is 0\n",
					ino, ino % fs->fs_ipg);
					continue;
				}
				if (mode != IFDIR && mode != IFREG &&
				    mode != IFLNK)
					continue;

				if (u->d_ufs == 1)
					count_ino_ufs1(u, fs, dino);
				else
					count_ino_ufs2(u, fs, dino);
				if (verbose)
					putchar('\n');
			}
		}
		ino = next_cg_ino;
	}
}

int
main(int argc, char *argv[])
{
	struct uufsd ufsd;
	int c;

	verbose = 0;
	while ((c = getopt(argc, argv, "hv")) != -1) {
		switch (c) {
		case 'h':
			usage();
			return (0);
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
			return (2);
		}
	}			
	if (optind + 1 != argc) {
		usage();
		return (2);
	}

	if (ufs_disk_fillout(&ufsd, argv[optind]) == -1) {
		fprintf(stderr, "Fillout: %s\n", ufsd.d_error);
		return (1);
	}

	frag_calc(&ufsd);

	if (ufs_disk_close(&ufsd) == -1) {
		fprintf(stderr, "Disk close: %s\n", ufsd.d_error);
		return (1);
	}

	printf("Total %jd data blocks, %jd breaks, %02.2f%% fragmentation.\n",
	    (intmax_t)blks_total, (intmax_t)blks_breaks,
	    (double)blks_breaks * 100.0 / blks_total);

	return (0);
}
