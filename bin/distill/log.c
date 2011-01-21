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

svn_error_t *
log_entry_receiver(void *baton,
    svn_log_entry_t *log_entry,
    apr_pool_t *pool)
{
	static const char *revprops[] = { "svn:author", "svn:date", "svn:log", NULL };
	svnsup_delta_t sd = (svnsup_delta_t)baton;
	svn_string_t *value;
	const char **p;
	int ret;

	(void)pool;
	SVNSUP_DEBUG("%s(r%lu)\n", __func__, (long)log_entry->revision);

	if (!extended)
		return (SVN_NO_ERROR);

	for (p = revprops; *p != NULL; ++p) {
		value = apr_hash_get(log_entry->revprops, *p, APR_HASH_KEY_STRING);
		if (value != NULL) {
			ret = svnsup_delta_meta(sd, *p, "%s", value->data);
			SVNSUP_SVNSUP_ERROR(ret, "svnsup_delta_meta()");
		}
	}
	return (SVN_NO_ERROR);
}
