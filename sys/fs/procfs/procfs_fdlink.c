/*-
 * Copyright (c) 2015 Dmitry Chagin
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/vnode.h>
#include <sys/uio.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>


int
procfs_dofdlink(PFS_FILL_ARGS)
{
	char *fullpath, *freepath, *endfileno;
	struct filedesc *fdp;
	struct vnode *vp;
	struct file *fp;
	int fileno, error;

	if (vnode_name == NULL)
		return (ENOENT);

	fileno = (int)strtol(vnode_name, &endfileno, 10);
	if (fileno == 0 && (vnode_namelen > 1 ||
	    (vnode_namelen == 1 && vnode_name[0] != '0')))
		return (ENOENT);
	if (vnode_namelen != endfileno - vnode_name)
		return (ENOENT);

	fdp = fdhold(p);
	if (fdp == NULL)
		return (ENOENT);

	error = fget_unlocked(fdp, fileno, NULL, &fp, NULL);
	if (error != 0)
		goto out;

	freepath = NULL;
	fullpath = "-";
	vp = fp->f_vnode;
	if (vp != NULL) {
		vref(vp);
		error = vn_fullpath(td, vp, &fullpath, &freepath);
		vrele(vp);
	}
	if (error == 0)
		error = sbuf_printf(sb, "%s", fullpath);
	if (freepath != NULL)
		free(freepath, M_TEMP);
	fdrop(fp, td);

 out:
	fddrop(fdp);
	return (error);
}
