/*-
 * Copyright (c) 2009 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "distill.h"

static svn_error_t *
open_tmp_file(apr_file_t **fp,
    void *callback_baton,
    apr_pool_t *pool)
{

	(void)callback_baton;
	(void)pool;
	SVNSUP_DEBUG("%s()\n", __func__);
	*fp = NULL;
	return (SVN_NO_ERROR);
}

static svn_error_t *
get_wc_prop(void *baton,
    const char *path,
    const char *name,
    const svn_string_t **value,
    apr_pool_t *pool)
{

	(void)baton;
	(void)pool;
	SVNSUP_DEBUG("%s(%s, %s)\n", __func__, path, name);
	*value = NULL;
	return (SVN_NO_ERROR);
}

static svn_error_t *
set_wc_prop(void *baton,
    const char *path,
    const char *name,
    const svn_string_t *value,
    apr_pool_t *pool)
{

	(void)baton;
	(void)pool;
	(void)value;
	SVNSUP_DEBUG("%s(%s, %s)\n", __func__, path, name);
	return (SVN_NO_ERROR);
}

static svn_error_t *
push_wc_prop(void *baton,
    const char *path,
    const char *name,
    const svn_string_t *value,
    apr_pool_t *pool)
{

	(void)baton;
	(void)pool;
	(void)value;
	SVNSUP_DEBUG("%s(%s, %s)\n", __func__, path, name);
	return (SVN_NO_ERROR);
}

static svn_error_t *
invalidate_wc_props(void *baton,
    const char *path,
    const char *name,
    apr_pool_t *pool)
{

	(void)baton;
	(void)pool;
	SVNSUP_DEBUG("%s(%s, %s)\n", __func__, path, name);
	return (SVN_NO_ERROR);
}

svn_error_t *
cancel_func(void *baton)
{

	(void)baton;
	if (verbose)
		SVNSUP_DEBUG("%s()\n", __func__);
	return (SVN_NO_ERROR);
}

void
progress_notify(apr_off_t progress,
    apr_off_t total,
    void *baton,
    apr_pool_t *pool)
{

	(void)baton;
	(void)pool;
	if (verbose)
		SVNSUP_DEBUG("%s(%ld, %ld)\n", __func__,
		    (long)progress, (long)total);
}

svn_error_t *
get_client_string(void *baton,
    const char **name,
    apr_pool_t *pool)
{

	(void)baton;
	(void)pool;
	*name = "svnsup";
	SVNSUP_DEBUG("%s()\n", __func__);
	return (SVN_NO_ERROR);
}

struct svn_ra_callbacks2_t ra_callbacks = {
	.open_tmp_file = open_tmp_file,
	.get_wc_prop = get_wc_prop,
	.set_wc_prop = set_wc_prop,
	.push_wc_prop = push_wc_prop,
	.invalidate_wc_props = invalidate_wc_props,
	.progress_func = progress_notify,
	.cancel_func = cancel_func,
	.get_client_string = get_client_string,
};
