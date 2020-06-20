/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tape.c	8.4 (Berkeley) 5/1/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <protocols/dumprestore.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dump.h"

ino_t	curino;			/* current inumber; used globally */
int	newtape;		/* new tape flag */
union	u_spcl u_spcl;		/* mapping of variables in a control block */

static	int tapefd;		/* tape file descriptor */
static	long asize;		/* number of 0.1" units written on cur tape */
static	int writesize;		/* size of malloc()ed buffer for tape */
static	int64_t lastspclrec = -1; /* tape block number of last written header */
static	int trecno = 0;		/* next record to write in current block */
static	long blocksthisvol;	/* number of blocks on current output file */
static	char *nexttape;
static	FILE *popenfp = NULL;

static	int atomic(ssize_t (*)(), int, char *, int);
static	void dominion(int, int);
static	void enminion(void);
static	void flushtape(void);
static	void killall(void);
static	void rollforward(void);

/*
 * Concurrent dump mods (Caltech) - disk block reading and tape writing
 * are exported to several minion processes.  While one minion writes the
 * tape, the others read disk blocks; they pass control of the tape in
 * a ring via signals. The parent process traverses the file system and
 * sends writeheader()'s and lists of daddr's to the minions via pipes.
 * The following structure defines the instruction packets sent to minions.
 */
struct req {
	ufs2_daddr_t dblk;
	int count;
};
static int reqsiz;

#define MINIONS 3		/* 1 minion writing, 1 reading, 1 for slack */
static struct minion {
	int64_t tapea;		/* header number at start of this chunk */
	int64_t firstrec;	/* record number of this block */
	int count;		/* count to next header (used for TS_TAPE */
				/* after EOT) */
	int inode;		/* inode that we are currently dealing with */
	int fd;			/* FD for this minion */
	int pid;		/* PID for this minion */
	int sent;		/* 1 == we've sent this minion requests */
	char (*tblock)[TP_BSIZE]; /* buffer for data blocks */
	struct req *req;	/* buffer for requests */
} minions[MINIONS+1];
static struct minion *mlp;

static char	(*nextblock)[TP_BSIZE];

static int master;	/* pid of master, for sending error signals */
static int tenths;	/* length of tape used per block written */
static volatile sig_atomic_t caught; /* have we caught the signal to proceed? */
static volatile sig_atomic_t ready; /* reached the lock point without having */
			/* received the SIGUSR2 signal from the prev minion? */
static jmp_buf jmpbuf;	/* where to jump to if we are ready when the */
			/* SIGUSR2 arrives from the previous minion */

int
alloctape(void)
{
	int pgoff = getpagesize() - 1;
	char *buf;
	int i;

	writesize = ntrec * TP_BSIZE;
	reqsiz = (ntrec + 1) * sizeof(struct req);
	/*
	 * CDC 92181's and 92185's make 0.8" gaps in 1600-bpi start/stop mode
	 * (see DEC TU80 User's Guide).  The shorter gaps of 6250-bpi require
	 * repositioning after stopping, i.e, streaming mode, where the gap is
	 * variable, 0.30" to 0.45".  The gap is maximal when the tape stops.
	 */
	if (blocksperfile == 0 && !unlimited)
		tenths = writesize / density +
		    (cartridge ? 16 : density == 625 ? 5 : 8);
	/*
	 * Allocate tape buffer contiguous with the array of instruction
	 * packets, so flushtape() can write them together with one write().
	 * Align tape buffer on page boundary to speed up tape write().
	 */
	for (i = 0; i <= MINIONS; i++) {
		buf = (char *)
		    malloc((unsigned)(reqsiz + writesize + pgoff + TP_BSIZE));
		if (buf == NULL)
			return(0);
		minions[i].tblock = (char (*)[TP_BSIZE])
		    (((long)&buf[ntrec + 1] + pgoff) &~ pgoff);
		minions[i].req = (struct req *)minions[i].tblock - ntrec - 1;
	}
	mlp = &minions[0];
	mlp->count = 1;
	mlp->tapea = 0;
	mlp->firstrec = 0;
	nextblock = mlp->tblock;
	return(1);
}

void
writerec(char *dp, int isspcl)
{

	mlp->req[trecno].dblk = (ufs2_daddr_t)0;
	mlp->req[trecno].count = 1;
	/* Can't do a structure assignment due to alignment problems */
	bcopy(dp, *(nextblock)++, sizeof (union u_spcl));
	if (isspcl)
		lastspclrec = spcl.c_tapea;
	trecno++;
	spcl.c_tapea++;
	if (trecno >= ntrec)
		flushtape();
}

void
dumpblock(ufs2_daddr_t blkno, int size)
{
	int avail, tpblks;
	ufs2_daddr_t dblkno;

	dblkno = fsbtodb(sblock, blkno);
	tpblks = size >> tp_bshift;
	while ((avail = MIN(tpblks, ntrec - trecno)) > 0) {
		mlp->req[trecno].dblk = dblkno;
		mlp->req[trecno].count = avail;
		trecno += avail;
		spcl.c_tapea += avail;
		if (trecno >= ntrec)
			flushtape();
		dblkno += avail << (tp_bshift - dev_bshift);
		tpblks -= avail;
	}
}

int	nogripe = 0;

void
tperror(int signo __unused)
{

	if (pipeout) {
		msg("write error on %s\n", tape);
		quit("Cannot recover\n");
		/* NOTREACHED */
	}
	msg("write error %ld blocks into volume %d\n", blocksthisvol, tapeno);
	broadcast("DUMP WRITE ERROR!\n");
	if (!query("Do you want to restart?"))
		dumpabort(0);
	msg("Closing this volume.  Prepare to restart with new media;\n");
	msg("this dump volume will be rewritten.\n");
	killall();
	nogripe = 1;
	close_rewind();
	Exit(X_REWRITE);
}

void
sigpipe(int signo __unused)
{

	quit("Broken pipe\n");
}

static void
flushtape(void)
{
	int i, blks, got;
	int64_t lastfirstrec;

	int siz = (char *)nextblock - (char *)mlp->req;

	mlp->req[trecno].count = 0;			/* Sentinel */

	if (atomic(write, mlp->fd, (char *)mlp->req, siz) != siz)
		quit("error writing command pipe: %s\n", strerror(errno));
	mlp->sent = 1; /* we sent a request, read the response later */

	lastfirstrec = mlp->firstrec;

	if (++mlp >= &minions[MINIONS])
		mlp = &minions[0];

	/* Read results back from next minion */
	if (mlp->sent) {
		if (atomic(read, mlp->fd, (char *)&got, sizeof got)
		    != sizeof got) {
			perror("  DUMP: error reading command pipe in master");
			dumpabort(0);
		}
		mlp->sent = 0;

		/* Check for end of tape */
		if (got < writesize) {
			msg("End of tape detected\n");

			/*
			 * Drain the results, don't care what the values were.
			 * If we read them here then trewind won't...
			 */
			for (i = 0; i < MINIONS; i++) {
				if (minions[i].sent) {
					if (atomic(read, minions[i].fd,
					    (char *)&got, sizeof got)
					    != sizeof got) {
						perror("  DUMP: error reading command pipe in master");
						dumpabort(0);
					}
					minions[i].sent = 0;
				}
			}

			close_rewind();
			rollforward();
			return;
		}
	}

	blks = 0;
	if (spcl.c_type != TS_END && spcl.c_type != TS_CLRI &&
	    spcl.c_type != TS_BITS) {
		assert(spcl.c_count <= TP_NINDIR);
		for (i = 0; i < spcl.c_count; i++)
			if (spcl.c_addr[i] != 0)
				blks++;
	}
	mlp->count = lastspclrec + blks + 1 - spcl.c_tapea;
	mlp->tapea = spcl.c_tapea;
	mlp->firstrec = lastfirstrec + ntrec;
	mlp->inode = curino;
	nextblock = mlp->tblock;
	trecno = 0;
	asize += tenths;
	blockswritten += ntrec;
	blocksthisvol += ntrec;
	if (!pipeout && !unlimited && (blocksperfile ?
	    (blocksthisvol >= blocksperfile) : (asize > tsize))) {
		close_rewind();
		startnewtape(0);
	}
	timeest();
}

void
trewind(void)
{
	struct stat sb;
	int f;
	int got;

	for (f = 0; f < MINIONS; f++) {
		/*
		 * Drain the results, but unlike EOT we DO (or should) care
		 * what the return values were, since if we detect EOT after
		 * we think we've written the last blocks to the tape anyway,
		 * we have to replay those blocks with rollforward.
		 *
		 * fixme: punt for now.
		 */
		if (minions[f].sent) {
			if (atomic(read, minions[f].fd, (char *)&got, sizeof got)
			    != sizeof got) {
				perror("  DUMP: error reading command pipe in master");
				dumpabort(0);
			}
			minions[f].sent = 0;
			if (got != writesize) {
				msg("EOT detected in last 2 tape records!\n");
				msg("Use a longer tape, decrease the size estimate\n");
				quit("or use no size estimate at all.\n");
			}
		}
		(void) close(minions[f].fd);
	}
	while (wait((int *)NULL) >= 0)	/* wait for any signals from minions */
		/* void */;

	if (pipeout)
		return;

	msg("Closing %s\n", tape);

	if (popenout) {
		tapefd = -1;
		(void)pclose(popenfp);
		popenfp = NULL;
		return;
	}
#ifdef RDUMP
	if (host) {
		rmtclose();
		while (rmtopen(tape, 0) < 0)
			sleep(10);
		rmtclose();
		return;
	}
#endif
	if (fstat(tapefd, &sb) == 0 && S_ISFIFO(sb.st_mode)) {
		(void)close(tapefd);
		return;
	}
	(void) close(tapefd);
	while ((f = open(tape, 0)) < 0)
		sleep (10);
	(void) close(f);
}

void
close_rewind()
{
	time_t tstart_changevol, tend_changevol;

	trewind();
	if (nexttape)
		return;
	(void)time((time_t *)&(tstart_changevol));
	if (!nogripe) {
		msg("Change Volumes: Mount volume #%d\n", tapeno+1);
		broadcast("CHANGE DUMP VOLUMES!\a\a\n");
	}
	while (!query("Is the new volume mounted and ready to go?"))
		if (query("Do you want to abort?")) {
			dumpabort(0);
			/*NOTREACHED*/
		}
	(void)time((time_t *)&(tend_changevol));
	if ((tstart_changevol != (time_t)-1) && (tend_changevol != (time_t)-1))
		tstart_writing += (tend_changevol - tstart_changevol);
}

void
rollforward(void)
{
	struct req *p, *q, *prev;
	struct minion *tmlp;
	int i, size, got;
	int64_t savedtapea;
	union u_spcl *ntb, *otb;
	tmlp = &minions[MINIONS];
	ntb = (union u_spcl *)tmlp->tblock[1];

	/*
	 * Each of the N minions should have requests that need to
	 * be replayed on the next tape.  Use the extra minion buffers
	 * (minions[MINIONS]) to construct request lists to be sent to
	 * each minion in turn.
	 */
	for (i = 0; i < MINIONS; i++) {
		q = &tmlp->req[1];
		otb = (union u_spcl *)mlp->tblock;

		/*
		 * For each request in the current minion, copy it to tmlp.
		 */

		prev = NULL;
		for (p = mlp->req; p->count > 0; p += p->count) {
			*q = *p;
			if (p->dblk == 0)
				*ntb++ = *otb++; /* copy the datablock also */
			prev = q;
			q += q->count;
		}
		if (prev == NULL)
			quit("rollforward: protocol botch");
		if (prev->dblk != 0)
			prev->count -= 1;
		else
			ntb--;
		q -= 1;
		q->count = 0;
		q = &tmlp->req[0];
		if (i == 0) {
			q->dblk = 0;
			q->count = 1;
			trecno = 0;
			nextblock = tmlp->tblock;
			savedtapea = spcl.c_tapea;
			spcl.c_tapea = mlp->tapea;
			startnewtape(0);
			spcl.c_tapea = savedtapea;
			lastspclrec = savedtapea - 1;
		}
		size = (char *)ntb - (char *)q;
		if (atomic(write, mlp->fd, (char *)q, size) != size) {
			perror("  DUMP: error writing command pipe");
			dumpabort(0);
		}
		mlp->sent = 1;
		if (++mlp >= &minions[MINIONS])
			mlp = &minions[0];

		q->count = 1;

		if (prev->dblk != 0) {
			/*
			 * If the last one was a disk block, make the
			 * first of this one be the last bit of that disk
			 * block...
			 */
			q->dblk = prev->dblk +
				prev->count * (TP_BSIZE / DEV_BSIZE);
			ntb = (union u_spcl *)tmlp->tblock;
		} else {
			/*
			 * It wasn't a disk block.  Copy the data to its
			 * new location in the buffer.
			 */
			q->dblk = 0;
			*((union u_spcl *)tmlp->tblock) = *ntb;
			ntb = (union u_spcl *)tmlp->tblock[1];
		}
	}
	mlp->req[0] = *q;
	nextblock = mlp->tblock;
	if (q->dblk == 0)
		nextblock++;
	trecno = 1;

	/*
	 * Clear the first minions' response.  One hopes that it
	 * worked ok, otherwise the tape is much too short!
	 */
	if (mlp->sent) {
		if (atomic(read, mlp->fd, (char *)&got, sizeof got)
		    != sizeof got) {
			perror("  DUMP: error reading command pipe in master");
			dumpabort(0);
		}
		mlp->sent = 0;

		if (got != writesize) {
			quit("EOT detected at start of the tape!\n");
		}
	}
}

/*
 * We implement taking and restoring checkpoints on the tape level.
 * When each tape is opened, a new process is created by forking; this
 * saves all of the necessary context in the parent.  The child
 * continues the dump; the parent waits around, saving the context.
 * If the child returns X_REWRITE, then it had problems writing that tape;
 * this causes the parent to fork again, duplicating the context, and
 * everything continues as if nothing had happened.
 */
void
startnewtape(int top)
{
	int	parentpid;
	int	childpid;
	int	status;
	char	*p;
	sig_t	interrupt_save;

	interrupt_save = signal(SIGINT, SIG_IGN);
	parentpid = getpid();

restore_check_point:
	(void)signal(SIGINT, interrupt_save);
	/*
	 *	All signals are inherited...
	 */
	setproctitle(NULL);	/* Restore the proctitle. */
	childpid = fork();
	if (childpid < 0) {
		msg("Context save fork fails in parent %d\n", parentpid);
		Exit(X_ABORT);
	}
	if (childpid != 0) {
		/*
		 *	PARENT:
		 *	save the context by waiting
		 *	until the child doing all of the work returns.
		 *	don't catch the interrupt
		 */
		signal(SIGINT, SIG_IGN);
#ifdef TDEBUG
		msg("Tape: %d; parent process: %d child process %d\n",
			tapeno+1, parentpid, childpid);
#endif /* TDEBUG */
		if (waitpid(childpid, &status, 0) == -1)
			msg("Waiting for child %d: %s\n", childpid,
			    strerror(errno));
		if (status & 0xFF) {
			msg("Child %d returns LOB status %o\n",
				childpid, status&0xFF);
		}
		status = (status >> 8) & 0xFF;
#ifdef TDEBUG
		switch(status) {
			case X_FINOK:
				msg("Child %d finishes X_FINOK\n", childpid);
				break;
			case X_ABORT:
				msg("Child %d finishes X_ABORT\n", childpid);
				break;
			case X_REWRITE:
				msg("Child %d finishes X_REWRITE\n", childpid);
				break;
			default:
				msg("Child %d finishes unknown %d\n",
					childpid, status);
				break;
		}
#endif /* TDEBUG */
		switch(status) {
			case X_FINOK:
				Exit(X_FINOK);
			case X_ABORT:
				Exit(X_ABORT);
			case X_REWRITE:
				goto restore_check_point;
			default:
				msg("Bad return code from dump: %d\n", status);
				Exit(X_ABORT);
		}
		/*NOTREACHED*/
	} else {	/* we are the child; just continue */
#ifdef TDEBUG
		sleep(4);	/* allow time for parent's message to get out */
		msg("Child on Tape %d has parent %d, my pid = %d\n",
			tapeno+1, parentpid, getpid());
#endif /* TDEBUG */
		/*
		 * If we have a name like "/dev/rmt0,/dev/rmt1",
		 * use the name before the comma first, and save
		 * the remaining names for subsequent volumes.
		 */
		tapeno++;               /* current tape sequence */
		if (nexttape || strchr(tape, ',')) {
			if (nexttape && *nexttape)
				tape = nexttape;
			if ((p = strchr(tape, ',')) != NULL) {
				*p = '\0';
				nexttape = p + 1;
			} else
				nexttape = NULL;
			msg("Dumping volume %d on %s\n", tapeno, tape);
		}
		if (pipeout) {
			tapefd = STDOUT_FILENO;
		} else if (popenout) {
			char volno[sizeof("2147483647")];

			(void)sprintf(volno, "%d", spcl.c_volume + 1);
			if (setenv("DUMP_VOLUME", volno, 1) == -1) {
				msg("Cannot set $DUMP_VOLUME.\n");
				dumpabort(0);
			}
			popenfp = popen(popenout, "w");
			if (popenfp == NULL) {
				msg("Cannot open output pipeline \"%s\".\n",
				    popenout);
				dumpabort(0);
			}
			tapefd = fileno(popenfp);
		} else {
#ifdef RDUMP
			while ((tapefd = (host ? rmtopen(tape, 2) :
				open(tape, O_WRONLY|O_CREAT, 0666))) < 0)
#else
			while ((tapefd =
			    open(tape, O_WRONLY|O_CREAT, 0666)) < 0)
#endif
			    {
				msg("Cannot open output \"%s\".\n", tape);
				if (!query("Do you want to retry the open?"))
					dumpabort(0);
			}
		}

		enminion();  /* Share open tape file descriptor with minions */
		if (popenout)
			close(tapefd);	/* Give up our copy of it. */
		signal(SIGINFO, infosch);

		asize = 0;
		blocksthisvol = 0;
		if (top)
			newtape++;		/* new tape signal */
		spcl.c_count = mlp->count;
		/*
		 * measure firstrec in TP_BSIZE units since restore doesn't
		 * know the correct ntrec value...
		 */
		spcl.c_firstrec = mlp->firstrec;
		spcl.c_volume++;
		spcl.c_type = TS_TAPE;
		writeheader((ino_t)mlp->inode);
		if (tapeno > 1)
			msg("Volume %d begins with blocks from inode %d\n",
				tapeno, mlp->inode);
	}
}

void
dumpabort(int signo __unused)
{

	if (master != 0 && master != getpid())
		/* Signals master to call dumpabort */
		(void) kill(master, SIGTERM);
	else {
		killall();
		msg("The ENTIRE dump is aborted.\n");
	}
#ifdef RDUMP
	rmtclose();
#endif
	Exit(X_ABORT);
}

void
Exit(status)
	int status;
{

#ifdef TDEBUG
	msg("pid = %d exits with status %d\n", getpid(), status);
#endif /* TDEBUG */
	exit(status);
}

/*
 * proceed - handler for SIGUSR2, used to synchronize IO between the minions.
 */
void
proceed(int signo __unused)
{

	if (ready)
		longjmp(jmpbuf, 1);
	caught++;
}

void
enminion(void)
{
	int cmd[2];
	int i, j;

	master = getpid();

	signal(SIGTERM, dumpabort);  /* Minion sends SIGTERM on dumpabort() */
	signal(SIGPIPE, sigpipe);
	signal(SIGUSR1, tperror);    /* Minion sends SIGUSR1 on tape errors */
	signal(SIGUSR2, proceed);    /* Minion sends SIGUSR2 to next minion */

	for (i = 0; i < MINIONS; i++) {
		if (i == mlp - &minions[0]) {
			caught = 1;
		} else {
			caught = 0;
		}

		if (socketpair(AF_UNIX, SOCK_STREAM, 0, cmd) < 0 ||
		    (minions[i].pid = fork()) < 0)
			quit("too many minions, %d (recompile smaller): %s\n",
			    i, strerror(errno));

		minions[i].fd = cmd[1];
		minions[i].sent = 0;
		if (minions[i].pid == 0) { 	    /* Minion starts up here */
			for (j = 0; j <= i; j++)
			        (void) close(minions[j].fd);
			signal(SIGINT, SIG_IGN);    /* Master handles this */
			dominion(cmd[0], i);
			Exit(X_FINOK);
		}
	}

	for (i = 0; i < MINIONS; i++)
		(void) atomic(write, minions[i].fd,
			      (char *) &minions[(i + 1) % MINIONS].pid,
		              sizeof minions[0].pid);

	master = 0;
}

void
killall(void)
{
	int i;

	for (i = 0; i < MINIONS; i++)
		if (minions[i].pid > 0) {
			(void) kill(minions[i].pid, SIGKILL);
			minions[i].sent = 0;
		}
}

/*
 * Synchronization - each process has a lockfile, and shares file
 * descriptors to the following process's lockfile.  When our write
 * completes, we release our lock on the following process's lock-
 * file, allowing the following process to lock it and proceed. We
 * get the lock back for the next cycle by swapping descriptors.
 */
static void
dominion(int cmd, int minion_number)
{
	int nread;
	int nextminion, size, wrote, eot_count;

	/*
	 * Need our own seek pointer.
	 */
	(void) close(diskfd);
	if ((diskfd = open(disk, O_RDONLY)) < 0)
		quit("minion couldn't reopen disk: %s\n", strerror(errno));

	/*
	 * Need the pid of the next minion in the loop...
	 */
	if ((nread = atomic(read, cmd, (char *)&nextminion, sizeof nextminion))
	    != sizeof nextminion) {
		quit("master/minion protocol botched - didn't get pid of next minion.\n");
	}

	/*
	 * Get list of blocks to dump, read the blocks into tape buffer
	 */
	while ((nread = atomic(read, cmd, (char *)mlp->req, reqsiz)) == reqsiz) {
		struct req *p = mlp->req;

		for (trecno = 0; trecno < ntrec;
		     trecno += p->count, p += p->count) {
			if (p->dblk) {
				blkread(p->dblk, mlp->tblock[trecno],
					p->count * TP_BSIZE);
			} else {
				if (p->count != 1 || atomic(read, cmd,
				    (char *)mlp->tblock[trecno],
				    TP_BSIZE) != TP_BSIZE)
				       quit("master/minion protocol botched.\n");
			}
		}
		if (setjmp(jmpbuf) == 0) {
			ready = 1;
			if (!caught)
				(void) pause();
		}
		ready = 0;
		caught = 0;

		/* Try to write the data... */
		eot_count = 0;
		size = 0;

		wrote = 0;
		while (eot_count < 10 && size < writesize) {
#ifdef RDUMP
			if (host)
				wrote = rmtwrite(mlp->tblock[0]+size,
				    writesize-size);
			else
#endif
				wrote = write(tapefd, mlp->tblock[0]+size,
				    writesize-size);
#ifdef WRITEDEBUG
			printf("minion %d wrote %d\n", minion_number, wrote);
#endif
			if (wrote < 0)
				break;
			if (wrote == 0)
				eot_count++;
			size += wrote;
		}

#ifdef WRITEDEBUG
		if (size != writesize)
		 printf("minion %d only wrote %d out of %d bytes and gave up.\n",
		     minion_number, size, writesize);
#endif

		/*
		 * Handle ENOSPC as an EOT condition.
		 */
		if (wrote < 0 && errno == ENOSPC) {
			wrote = 0;
			eot_count++;
		}

		if (eot_count > 0)
			size = 0;

		if (wrote < 0) {
			(void) kill(master, SIGUSR1);
			for (;;)
				(void) sigpause(0);
		} else {
			/*
			 * pass size of write back to master
			 * (for EOT handling)
			 */
			(void) atomic(write, cmd, (char *)&size, sizeof size);
		}

		/*
		 * If partial write, don't want next minion to go.
		 * Also jolts him awake.
		 */
		(void) kill(nextminion, SIGUSR2);
	}
	if (nread != 0)
		quit("error reading command pipe: %s\n", strerror(errno));
}

/*
 * Since a read from a pipe may not return all we asked for,
 * or a write may not write all we ask if we get a signal,
 * loop until the count is satisfied (or error).
 */
static int
atomic(ssize_t (*func)(), int fd, char *buf, int count)
{
	int got, need = count;

	while ((got = (*func)(fd, buf, need)) > 0 && (need -= got) > 0)
		buf += got;
	return (got < 0 ? got : count - need);
}
