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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "distill.h"

int debug;			/* print debugging information */
int extended;			/* generated extended delta */
int verbose;			/* show messages from libsvn */

static int
distill(const char *url, unsigned long revision)
{
	apr_hash_t *config;
	apr_pool_t *pool;
	apr_status_t status;
	svn_auth_provider_object_t *auth_provider;
	apr_array_header_t *auth_providers;
	svn_ra_session_t *ra_session;
	const char *root, *subdir, *uuid;
	svn_error_t *error;
	svnsup_delta_t sd;
	svnsup_err_t err;

	/* our root pool */
	status = apr_pool_create(&pool, NULL);
	SVNSUP_APR_ERROR(status, "apr_pool_create()");

	/* canonicalize URL */
	url = svn_path_canonicalize(url, pool);

	/* set up our authentication system */
	/* XXX check for errors */
	auth_providers = apr_array_make(pool, 1, sizeof auth_provider);
	svn_auth_get_username_prompt_provider(&auth_provider,
	    username_prompt_callback, NULL, 0, pool);
	APR_ARRAY_PUSH(auth_providers, svn_auth_provider_object_t *) =
	    auth_provider;
	svn_auth_open(&ra_callbacks.auth_baton, auth_providers, pool);

	/* open a connection to the repo */
	config = apr_hash_make(pool);
	error = svn_ra_open3(&ra_session, url, NULL, &ra_callbacks,
	    NULL, config, pool);
	SVNSUP_SVN_ERROR(error, "svn_ra_open3()");

	/* get repo uuid */
	error = svn_ra_get_uuid2(ra_session, &uuid, pool);
	SVNSUP_SVN_ERROR(error, "svn_ra_get_uuid2()");

	/* get repo root */
	error = svn_ra_get_repos_root2(ra_session, &root, pool);
	SVNSUP_SVN_ERROR(error, "svn_ra_get_repos_root2()");

	/* get subdirectory; libsvn guarantees root is a prefix of url */
	assert(strstr(url, root) == url);
	subdir = url + strlen(root);
	assert(*subdir == '/' || *subdir == '\0');
	if (*subdir == '/')
		++subdir;

	/* XXX create delta */
	err = svnsup_create_delta(&sd);
	SVNSUP_SVNSUP_ERROR(err, "svnsup_delta_create()");
	err = svnsup_delta_root(sd, root);
	SVNSUP_SVNSUP_ERROR(err, "svnsup_delta_root()");
	err = svnsup_delta_uuid(sd, uuid);
	SVNSUP_SVNSUP_ERROR(err, "svnsup_delta_uuid()");
	err = svnsup_delta_path(sd, subdir);
	SVNSUP_SVNSUP_ERROR(err, "svnsup_delta_path()");

	/* get revision metadata */
	error = svn_ra_get_log2(ra_session, NULL, revision, revision, 0,
	    TRUE, TRUE, FALSE, NULL, log_entry_receiver, sd, pool);
	SVNSUP_SVN_ERROR(error, "svn_ra_get_log()");

	/* replay the requested revision */
	error = svn_ra_replay(ra_session, revision, revision - 1, TRUE,
	    &delta_editor, sd, pool);
	SVNSUP_SVN_ERROR(error, "svn_ra_replay()");

	/* finish off the delta */
	err = svnsup_close_delta(sd);
	SVNSUP_SVNSUP_ERROR(err, "svnsup_delta_close()");

	/* clean up */
	apr_pool_destroy(pool);
	return (0);
}

static void
usage(void)
{

	fprintf(stderr, "usage: svnsup-distill [-dev] url rev\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	apr_status_t status;
	const char *url;
	char *end, *revstr;
	unsigned long rev;
	int opt, ret;

	while ((opt = getopt(argc, argv, "dev")) != -1)
		switch (opt) {
		case 'd':
			++debug;
			break;
		case 'e':
			++extended;
			break;
		case 'v':
			++verbose;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	url = argv[0];
	revstr = argv[1];
	if (*revstr == 'r')
		++revstr;
	rev = strtoul(revstr, &end, 10);
	if (rev == 0 || end == revstr || *end != '\0')
		usage();

	status = apr_initialize();
	if (status != APR_SUCCESS)
		return (1);

	ret = distill(url, rev);

	apr_terminate();
	return (ret);
}
