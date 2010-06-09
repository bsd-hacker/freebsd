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
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sysexits.h>
#include <errno.h>
#include <err.h>
#include <libsade.h>
#include <assert.h>

static const char *class_name = "PART";
static const char *sade_flags = "sade";
static char de_errstr[BUFSIZ];

char*
de_error(void)
{
	return ((char *)de_errstr);
}

static int
de_partlist_add_part(struct de_device *pdev, struct gprovider *pp,
    off_t start, off_t end)
{
	struct de_part *ppart;
	const char *s;

	ppart = malloc(sizeof(struct de_part));
	if (!ppart)
		return (ENOMEM);
	bzero(ppart, sizeof(struct de_part));

	ppart->de_name = strdup(pp->lg_name);
	ppart->de_device = pdev;
	ppart->de_start = start;
	ppart->de_end = end;

	s = find_provcfg(pp, "type");
	if (s)
		ppart->de_type = strdup(s);
	s = find_provcfg(pp, "label");
	if (s)
		ppart->de_label = strdup(s);
	s = find_provcfg(pp, "attrib");
	if (s)
		ppart->de_private = (void *)strdup(s);
	s = find_provcfg(pp, "index");
	ppart->de_index = atoi(s);

	TAILQ_INSERT_TAIL(&pdev->de_part, ppart, de_part);
	return (0);
}

static int
de_partlist_add_unused(struct de_device *pdev, off_t start, off_t end)
{
	struct de_part *ppart;

	ppart = malloc(sizeof(struct de_part));
	if (!ppart)
		return (ENOMEM);
	bzero(ppart, sizeof(struct de_part));

	ppart->de_device = pdev;
	ppart->de_start = start;
	ppart->de_end = end;
	TAILQ_INSERT_TAIL(&pdev->de_part, ppart, de_part);
	return (0);
}

int
de_partlist_get(struct de_device *pdev)
{
	struct gmesh mesh;
	struct gclass *cp;
	struct ggeom *gp;
	struct gprovider *pp;
	unsigned long long first, last, start, end;
	const char *s;
	int error;

	error = geom_gettree(&mesh);
	if (error)
		return (error);
	/* check for PART class */
	cp = find_class(&mesh, class_name);
	if (cp == NULL) {
		error = ENODEV;
		goto done;
	}
	gp = find_geom(cp, pdev->de_name);
	if (gp == NULL) {
		/* device didn't partitioned */
		error = de_partlist_add_unused(pdev, 0,
		    pdev->de_mediasize / pdev->de_sectorsize - 1);
		goto done;
	}
	s = find_geomcfg(gp, "scheme");
	pdev->de_scheme = strdup(s);

        s = find_geomcfg(gp, "first");
        first = atoll(s);
        s = find_geomcfg(gp, "last");
        last = atoll(s);
	while ((pp = find_provider(gp, first)) != NULL) {
		s = find_provcfg(pp, "start");
		if (s == NULL) {
			s = find_provcfg(pp, "offset");
			start = atoll(s) / pdev->de_sectorsize;
		} else
			start = atoll(s);

		s = find_provcfg(pp, "end");
		if (s == NULL) {
			s = find_provcfg(pp, "length");
			end = start + atoll(s) / pdev->de_sectorsize - 1;
		} else
			end = atoll(s);

		if (first < start) {
			error = de_partlist_add_unused(pdev, first, start - 1);
			if (error)
				goto done;
		}
		error = de_partlist_add_part(pdev, pp, start, end);
		if (error)
			goto done;
		first = end + 1;
	}
	if (first <= last)
		error = de_partlist_add_unused(pdev, first, last);
done:
	geom_deletetree(&mesh);
	return (error);
}

int
de_partlist_count(struct de_partlist *partlist)
{
	int count = 0;
	struct de_part *ppart;

	TAILQ_FOREACH(ppart, partlist, de_part) {
		count++;
	}
	return (count);
}

void
de_dev_partlist_free(struct de_device *pdev)
{
	de_partlist_free(&pdev->de_part);
	free(pdev->de_scheme);
	pdev->de_scheme = NULL;
	TAILQ_INIT(&pdev->de_part);
}

void
de_partlist_free(struct de_partlist *partlist)
{
	struct de_part *ppart;

	while (!TAILQ_EMPTY(partlist)) {
		ppart = TAILQ_FIRST(partlist);
		free(ppart->de_name);
		free(ppart->de_type);
		free(ppart->de_label);
		free(ppart->de_private);
		TAILQ_REMOVE(partlist, ppart, de_part);
		free(ppart);
	}
}

static int
de_gpart_issue(struct gctl_req *req)
{
	const char *errstr;
	char *errmsg;
	int error = 0;

	errstr = gctl_issue(req);
	if (errstr != NULL && errstr[0] != '\0') {
		error = strtol(errstr, &errmsg, 0);
		if (errstr == errmsg)
			error = -1;
		snprintf(de_errstr, BUFSIZ, "%s", errmsg);
	}
	return (error);
}

int
de_dev_scheme_create(struct de_device *pdev, const char *scheme)
{
	int error;
	struct gctl_req *req;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	if (scheme == NULL)
		return (EINVAL);

	req = gctl_get_handle();
	if (req == NULL)
		return (ENOMEM);

	gctl_ro_param(req, "verb", -1, "create");
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "scheme", -1, scheme);
	gctl_ro_param(req, "provider", -1, pdev->de_name);
	gctl_ro_param(req, "flags", -1, sade_flags);

	error = de_gpart_issue(req);
	gctl_free(req);
	return (error);
}

int
de_dev_scheme_destroy(struct de_device *pdev)
{
	int error;
	struct gctl_req *req;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	req = gctl_get_handle();
	if (req == NULL)
		return (ENOMEM);

	gctl_ro_param(req, "verb", -1, "destroy");
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "geom", -1, pdev->de_name);
	gctl_ro_param(req, "flags", -1, sade_flags);

	error = de_gpart_issue(req);
	gctl_free(req);
	return (error);
}

static void *
de_bootfile_read(const char *bootfile, ssize_t *size)
{
	struct stat sb;
	void *code;
	int fd;

	assert(bootfile != NULL);

	if (stat(bootfile, &sb) == -1) {
		snprintf(de_errstr, BUFSIZ, "%s: %s", bootfile,
		    strerror(errno));
		return (NULL);
	}
	if (!S_ISREG(sb.st_mode)) {
		snprintf(de_errstr, BUFSIZ, "%s: not a regular file",
		    bootfile);
		return (NULL);
	}
	if (sb.st_size == 0) {
		snprintf(de_errstr, BUFSIZ, "%s: empty file", bootfile);
		return (NULL);
	}
	if (*size > 0 && sb.st_size >= *size) {
		snprintf(de_errstr, BUFSIZ, "%s: file too big (%zu limit)",
		    bootfile, *size);
		return (NULL);
	}

	*size = sb.st_size;
	fd = open(bootfile, O_RDONLY);
	if (fd == -1) {
		snprintf(de_errstr, BUFSIZ, "%s: %s", bootfile,
		    strerror(errno));
		return (NULL);
	}
	code = malloc(*size);
	if (code == NULL) {
		snprintf(de_errstr, BUFSIZ, "%s", strerror(errno));
		close(fd);
		return (NULL);
	}
	if (read(fd, code, *size) != *size) {
		snprintf(de_errstr, BUFSIZ, "%s", strerror(errno));
		free(code);
		code = NULL;
	}
	close(fd);
	return (code);
}

#define BOOTCODE_MAXSIZE	(800 * 1024)
int
de_dev_bootcode(struct de_device *pdev, const char *path)
{
	int error = 0;
	struct gctl_req *req;
	size_t size;
	void *code;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	size = BOOTCODE_MAXSIZE;
	code = de_bootfile_read(path, &size);
	if (code == NULL)
		return (-1);

	req = gctl_get_handle();
	if (req == NULL) {
		error = ENOMEM;
		goto fail;
	}

	gctl_ro_param(req, "verb", -1, "bootcode");
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "geom", -1, pdev->de_name);
	gctl_ro_param(req, "bootcode", size, code);
	gctl_ro_param(req, "flags", -1, sade_flags);

	error = de_gpart_issue(req);
	gctl_free(req);
fail:
	free(code);
	return (error);
}

int
de_dev_undo(struct de_device *pdev)
{
	int error;
	struct gctl_req *req;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	req = gctl_get_handle();
	if (req == NULL)
		return (ENOMEM);

	gctl_ro_param(req, "verb", -1, "undo");
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "geom", -1, pdev->de_name);

	error = de_gpart_issue(req);
	gctl_free(req);
	return (error);
}

int
de_dev_commit(struct de_device *pdev)
{
	int error;
	struct gctl_req *req;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	req = gctl_get_handle();
	if (req == NULL)
		return (ENOMEM);

	gctl_ro_param(req, "verb", -1, "commit");
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "geom", -1, pdev->de_name);

	error = de_gpart_issue(req);
	gctl_free(req);
	return (error);
}

static int
de_part_add_calculate_size(const char *devname, off_t *startp, off_t *sizep)
{
	struct gmesh mesh;
	struct gclass *cp;
	struct ggeom *gp;
	struct gprovider *pp;
	unsigned long long first, last;
	unsigned long long size, start;
	unsigned long long lba, len, grade;
	const char *s;
	int error;

	size = *sizep;
	start = *startp;
	error = geom_gettree(&mesh);
	if (error) {
		snprintf(de_errstr, BUFSIZ, "Can not get GEOM tree: %s",
		    strerror(error));
		return (-1);
	}
	error = -1;
	cp = find_class(&mesh, class_name);
	if (cp == NULL) {
		snprintf(de_errstr, BUFSIZ, "Class %s not found.",
		    class_name);
		goto fail;
	}
	gp = find_geom(cp, devname);
	if (gp == NULL) {
		snprintf(de_errstr, BUFSIZ, "No such geom: %s.",
		    devname);
		goto fail;
	}
	first = strtoull(find_geomcfg(gp, "first"), NULL, 10);
	last = strtoull(find_geomcfg(gp, "last"), NULL, 10);
	grade = ~0ULL;
	while ((pp = find_provider(gp, first)) != NULL) {
		s = find_provcfg(pp, "start");
		if (s == NULL) {
			s = find_provcfg(pp, "offset");
			lba = strtoull(s, NULL, 10) / pp->lg_sectorsize;
		} else
			lba = strtoull(s, NULL, 10);

		if (first < lba) {
			/* Free space [first, lba> */
			len = lba - first;
			if (*sizep) {
				if (len >= size && len - size < grade) {
					start = first;
					grade = len - size;
				}
			} else if (*startp) {
				if (start >= first && start < lba) {
					size = lba - start;
					grade = start - first;
				}
			} else {
				if (grade == ~0ULL || len > size) {
					start = first;
					size = len;
					grade = 0;
				}
			}
		}

		s = find_provcfg(pp, "end");
		if (s == NULL) {
			s = find_provcfg(pp, "length");
			first = lba + strtoull(s, NULL, 10) / pp->lg_sectorsize;
		} else
			first = strtoull(s, NULL, 10) + 1;
	}
	if (first <= last) {
		/* Free space [first-last] */
		len = last - first + 1;
		if (*sizep) {
			if (len >= size && len - size < grade) {
				start = first;
				grade = len - size;
			}
		} else if (*startp) {
			if (start >= first && start <= last) {
				size = last - start + 1;
				grade = start - first;
			}
		} else {
			if (grade == ~0ULL || len > size) {
				start = first;
				size = len;
				grade = 0;
			}
		}
	}

	if (grade == ~0ULL) {
		error = ENOSPC;
		goto fail;
	}

	if (*sizep == 0)
		*sizep = (off_t)size;
	if (*startp == 0)
		*startp = (off_t)start;
	error = 0;
fail:
	geom_deletetree(&mesh);
	return (error);
}


int
de_part_add(struct de_device *pdev, const char *type, off_t start, off_t size,
    const char* label, int idx)
{
	int error;
	struct gctl_req *req;
	char *sindex = NULL;
	char *sstart = NULL, *ssize = NULL;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	if (type == NULL)
		return (EINVAL);

	if (start == 0 || size == 0) {
		error = de_part_add_calculate_size(pdev->de_name, &start,
		    &size);
		if (error)
			return (error);
	}
	if (idx > 0) {
		asprintf(&sindex, "%d", idx);
		if (sindex == NULL)
			return (ENOMEM);
	}
	error = ENOMEM;
	asprintf(&sstart, "%lu", start);
	if (sstart == NULL)
		goto fail;
	asprintf(&ssize, "%lu", size);
	if (ssize == NULL)
		goto fail;
	req = gctl_get_handle();
	if (req == NULL)
		goto fail;

	gctl_ro_param(req, "verb", -1, "add");
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "geom", -1, pdev->de_name);
	gctl_ro_param(req, "type", -1, type);
	gctl_ro_param(req, "start", -1, sstart);
	gctl_ro_param(req, "size", -1, ssize);
	gctl_ro_param(req, "flags", -1, sade_flags);
	if (idx > 0)
		gctl_ro_param(req, "index", -1, sindex);
	if (label)
		gctl_ro_param(req, "label", -1, label);

	error = de_gpart_issue(req);
	gctl_free(req);
fail:
	free(sindex);
	free(sstart);
	free(ssize);
	return (error);
}

int
de_part_del(struct de_device *pdev, int idx)
{
	int error;
	struct gctl_req *req;
	char *sindex;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	if (idx <= 0)
		return (EINVAL);
	asprintf(&sindex, "%d", idx);
	if (sindex == NULL)
		return (ENOMEM);

	req = gctl_get_handle();
	if (req == NULL)
		goto fail;

	gctl_ro_param(req, "verb", -1, "delete");
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "geom", -1, pdev->de_name);
	gctl_ro_param(req, "index", -1, sindex);
	gctl_ro_param(req, "flags", -1, sade_flags);

	error = de_gpart_issue(req);
	gctl_free(req);
fail:
	free(sindex);
	return (error);
}

static int
de_part_attr(struct de_device *pdev, int act, const char *name, int idx)
{
	int error;
	struct gctl_req *req;
	const char *cmdstr = act ? "set": "unset";
	char *sindex;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	if (idx <= 0)
		return (EINVAL);
	asprintf(&sindex, "%d", idx);
	if (sindex == NULL)
		return (ENOMEM);

	req = gctl_get_handle();
	if (req == NULL)
		goto fail;

	gctl_ro_param(req, "verb", -1, cmdstr);
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "geom", -1, pdev->de_name);
	gctl_ro_param(req, "attrib", -1, name);
	gctl_ro_param(req, "index", -1, sindex);
	gctl_ro_param(req, "flags", -1, sade_flags);

	error = de_gpart_issue(req);
	gctl_free(req);
fail:
	free(sindex);
	return (error);
}

int
de_part_setattr(struct de_device *pdev, const char *name, int idx)
{
	return de_part_attr(pdev, 1, name, idx);
}

int
de_part_unsetattr(struct de_device *pdev, const char *name, int idx)
{
	return de_part_attr(pdev, 0, name, idx);
}

int
de_part_mod(struct de_device *pdev, const char *type, const char *label,
    int idx)
{
	int error;
	struct gctl_req *req;
	char *sindex = NULL;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	if (idx <= 0)
		return (EINVAL);
	if (type == NULL && label == NULL)
		return (EINVAL);
	asprintf(&sindex, "%d", idx);
	if (sindex == NULL)
		return (ENOMEM);

	req = gctl_get_handle();
	if (req == NULL)
		goto fail;

	gctl_ro_param(req, "verb", -1, "modify");
	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "geom", -1, pdev->de_name);
	gctl_ro_param(req, "index", -1, sindex);
	gctl_ro_param(req, "flags", -1, sade_flags);
	if (label)
		gctl_ro_param(req, "label", -1, label);
	if (type)
		gctl_ro_param(req, "type", -1, type);

	error = de_gpart_issue(req);
	gctl_free(req);
fail:
	free(sindex);
	return (error);
}

int
de_part_bootcode(struct de_part *ppart, const char *path)
{
	int error, fd;
	char dsf[128];
	size_t size;
	off_t bsize;
	void *code;
	char *buf;

	assert(ppart != NULL);
	assert(ppart->de_name != NULL);
	assert(ppart->de_device != NULL);
	assert(ppart->de_device->de_sectorsize != 0);

	error = -1;
	code = de_bootfile_read(path, &size);
	if (code == NULL)
		return (error);

	snprintf(dsf, sizeof(dsf), "/dev/%s", ppart->de_name);
	fd = open(dsf, O_WRONLY);
	if (fd == -1) {
		snprintf(de_errstr, BUFSIZ, "%s: %s", dsf,
		    strerror(errno));
		goto fail;
	}
	if (lseek(fd, size, SEEK_SET) != size) {
		snprintf(de_errstr, BUFSIZ, "%s: not enough space.", dsf);
		goto fail;
	}
	if (lseek(fd, 0, SEEK_SET) != 0) {
		snprintf(de_errstr, BUFSIZ, "%s: %s", dsf,
		    strerror(errno));
		goto fail;
	}
	/* align to sector size */
	bsize = (size + ppart->de_device->de_sectorsize - 1) /
	    ppart->de_device->de_sectorsize * ppart->de_device->de_sectorsize;
	buf = calloc(1, bsize);
	if (buf == NULL) {
		error = ENOMEM;
		goto fail;
	}
	bcopy(code, buf, size);
	if (write(fd, buf, bsize) != bsize) {
		snprintf(de_errstr, BUFSIZ, "Can not write %s: %s", dsf,
		    strerror(errno));
	} else
		error = 0;
	free(buf);
fail:
	close(fd);
	free(code);
	return (error);
}

#if 0
int
de_part_resize(struct de_device *pdev, const char *size, int idx)
{
	int error;
	struct gctl_req *req;
	const char *autofill = "*";
	char *sindex = NULL;

	assert(pdev != NULL);
	assert(pdev->de_name != NULL);

	if (idx <= 0)
		return (EINVAL);
	asprintf(&sindex, "%d", idx);
	if (sindex == NULL)
		return (ENOMEM);
	if (size == NULL)
		size = autofill;

	req = gctl_get_handle();
	if (req == NULL)
		return (ENOMEM);

	gctl_ro_param(req, "class", -1, class_name);
	gctl_ro_param(req, "verb", -1, "resize");
	gctl_ro_param(req, "geom", -1, pdev->de_name);
	gctl_ro_param(req, "size", -1, size);
	gctl_ro_param(req, "index", -1, sindex);
	gctl_ro_param(req, "flags", -1, sade_flags);

	error = de_gpart_issue(req);
	gctl_free(req);
	free(sindex);
	return (error);
}
#endif
