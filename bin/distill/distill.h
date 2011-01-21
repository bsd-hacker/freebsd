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

#ifndef DISTILL_H_INCLUDED
#define DISTILL_H_INCLUDED

#include <apr_errno.h>
#include <apr_general.h>
#include <apr_hash.h>
#include <apr_pools.h>
#include <apr_tables.h>

#include <subversion-1/svn_auth.h>
#include <subversion-1/svn_client.h>
#include <subversion-1/svn_delta.h>
#include <subversion-1/svn_error.h>
#include <subversion-1/svn_path.h>
#include <subversion-1/svn_ra.h>

#include "svnsup/svnsup.h"

extern int debug;
extern int extended;
extern int verbose;

typedef struct svnsup_where {
	const char *file;
	int line;
	const char *func;
} svnsup_where_t;
#define SVNSUP_WHERE							\
	&((struct svnsup_where){ __FILE__, __LINE__, __func__ })

/* apr errors */
void svnsup_apr_error(svnsup_where_t *, apr_status_t, const char *, ...);
#define SVNSUP_APR_ERROR(status, ...)					\
	do {								\
		if ((status) != APR_SUCCESS)				\
			svnsup_apr_error(SVNSUP_WHERE,			\
			    (status), __VA_ARGS__);			\
	} while (0)

/* svn errors */
void svnsup_svn_error(svnsup_where_t *, svn_error_t *, const char *, ...);
#define SVNSUP_SVN_ERROR(error, ...)					\
	do {								\
		if ((error) != SVN_NO_ERROR)				\
			svnsup_svn_error(SVNSUP_WHERE,			\
			    (error), __VA_ARGS__);			\
	} while (0)

/* svnsup errors */
void svnsup_svnsup_error(svnsup_where_t *, svnsup_err_t, const char *, ...);
#define SVNSUP_SVNSUP_ERROR(error, ...)					\
	do {								\
		if ((error) != SVNSUP_ERR_NONE)				\
			svnsup_svnsup_error(SVNSUP_WHERE,		\
			    (error), __VA_ARGS__);			\
	} while (0)

/* assertions */
void svnsup_assert(svnsup_where_t *, const char *, const char *, ...);
#define SVNSUP_ASSERT(cond, ...)					\
	do {								\
		if (!(cond))						\
			svnsup_assert(SVNSUP_WHERE,			\
			    #cond, __VA_ARGS__);			\
	} while (0)

/* debugging messages */
#define SVNSUP_DEBUG(fmt, ...)						\
	do {								\
		if (debug)						\
			fprintf(stderr, fmt, __VA_ARGS__);		\
	} while (0)

/* authentication */
svn_error_t *username_prompt_callback(svn_auth_cred_username_t **,
    void *, const char *, svn_boolean_t, apr_pool_t *);

/* callback function for log entries */
svn_error_t *log_entry_receiver(void *, svn_log_entry_t *, apr_pool_t *);

/* callback function for file deltas */
svn_error_t *txdelta_window_handler(svn_txdelta_window_t *, void *);

/* callback lists */
extern struct svn_ra_callbacks2_t ra_callbacks;
extern struct svn_delta_editor_t delta_editor;

#endif
