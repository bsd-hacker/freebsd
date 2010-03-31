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

#include "distill.h"

svn_error_t *
set_target_revision(void *edit_baton,
    svn_revnum_t target_revision,
    apr_pool_t *pool)
{

	(void)edit_baton;
	(void)pool;
	SVNSUP_DEBUG("%s(r%ld)\n", __func__, (long)target_revision);
	return (SVN_NO_ERROR);
}

svn_error_t *
open_root(void *edit_baton,
    svn_revnum_t base_revision,
    apr_pool_t *dir_pool,
    void **root_baton)
{
	svnsup_delta_t sd = (svnsup_delta_t)edit_baton;

	(void)dir_pool;
	SVNSUP_DEBUG("%s(%ld)\n", __func__, (long)base_revision);
	*root_baton = sd;
	return (SVN_NO_ERROR);
}

svn_error_t *
delete_entry(const char *path,
    svn_revnum_t revision,
    void *parent_baton,
    apr_pool_t *pool)
{
	svnsup_delta_t sd = (svnsup_delta_t)parent_baton;

	(void)pool;
	SVNSUP_DEBUG("%s(%ld, %s)\n", __func__, (long)revision, path);
	svnsup_delta_remove(sd, path);
	return (SVN_NO_ERROR);
}

svn_error_t *
add_directory(const char *path,
    void *parent_baton,
    const char *copyfrom_path,
    svn_revnum_t copyfrom_revision,
    apr_pool_t *dir_pool,
    void **child_baton)
{
	svnsup_delta_t sd = (svnsup_delta_t)parent_baton;

	(void)dir_pool;
	SVNSUP_DEBUG("%s(%s, %s, %ld)\n", __func__, path,
	    copyfrom_path, (long)copyfrom_revision);
	svnsup_delta_create_directory(sd, path);
	*child_baton = sd; /* XXX */
	return (SVN_NO_ERROR);
}

svn_error_t *
open_directory(const char *path,
    void *parent_baton,
    svn_revnum_t base_revision,
    apr_pool_t *dir_pool,
    void **child_baton)
{
	svnsup_delta_t sd = (svnsup_delta_t)parent_baton;

	(void)dir_pool;
	SVNSUP_DEBUG("%s(%s, %ld)\n", __func__, path,
	    (long)base_revision);
	*child_baton = sd; /* XXX */
	return (SVN_NO_ERROR);
}

svn_error_t *
change_dir_prop(void *dir_baton,
    const char *name,
    const svn_string_t *value,
    apr_pool_t *pool)
{

	(void)dir_baton;
	(void)pool;
	SVNSUP_DEBUG("%s(%s, %s)\n", __func__, name, value->data);
	return (SVN_NO_ERROR);
}

svn_error_t *
close_directory(void *dir_baton,
    apr_pool_t *pool)
{

	(void)dir_baton;
	(void)pool;
	SVNSUP_DEBUG("%s()\n", __func__);
	return (SVN_NO_ERROR);
}

svn_error_t *
absent_directory(const char *path,
    void *parent_baton,
    apr_pool_t *pool)
{
	svnsup_delta_t sd = (svnsup_delta_t)parent_baton;

	(void)sd;
	(void)pool;
	SVNSUP_DEBUG("%s(%s)\n", __func__, path);
	return (SVN_NO_ERROR);
}

svn_error_t *
add_file(const char *path,
    void *parent_baton,
    const char *copyfrom_path,
    svn_revnum_t copyfrom_revision,
    apr_pool_t *file_pool,
    void **file_baton)
{
	svnsup_delta_t sd = (svnsup_delta_t)parent_baton;
	svnsup_delta_file_t sdf;

	(void)file_pool;
	SVNSUP_DEBUG("%s(%s, %s, %ld)\n", __func__, path,
	    copyfrom_path, (long)copyfrom_revision);
	svnsup_delta_create_file(sd, &sdf, path);
	*file_baton = sdf;
	return (SVN_NO_ERROR);
}

svn_error_t *
open_file(const char *path,
    void *parent_baton,
    svn_revnum_t base_revision,
    apr_pool_t *file_pool,
    void **file_baton)
{
	svnsup_delta_t sd = (svnsup_delta_t)parent_baton;
	svnsup_delta_file_t sdf;

	(void)sd;
	(void)file_pool;
	SVNSUP_DEBUG("%s(%s, %ld)\n", __func__, path,
	    (long)base_revision);
	svnsup_delta_open_file(sd, &sdf, path);
	*file_baton = sdf;
	return (SVN_NO_ERROR);
}

svn_error_t *
apply_textdelta(void *file_baton,
    const char *base_checksum,
    apr_pool_t *pool,
    svn_txdelta_window_handler_t *handler,
    void **handler_baton)
{
	svnsup_delta_file_t sdf = (svnsup_delta_file_t)file_baton;

	(void)sdf;
	(void)pool;
	SVNSUP_DEBUG("%s(%s)\n", __func__, base_checksum);
	if (base_checksum)
		svnsup_delta_file_checksum(sdf, base_checksum);
	*handler = txdelta_window_handler;
	*handler_baton = file_baton;
	return (SVN_NO_ERROR);
}

svn_error_t *
change_file_prop(void *file_baton,
    const char *name,
    const svn_string_t *value,
    apr_pool_t *pool)
{
	svnsup_delta_file_t sdf = (svnsup_delta_file_t)file_baton;

	(void)sdf;
	(void)pool;
	SVNSUP_DEBUG("%s(%s, %s)\n", __func__, name, value->data);
	return (SVN_NO_ERROR);
}

svn_error_t *
close_file(void *file_baton,
    const char *text_checksum,
    apr_pool_t *pool)
{
	svnsup_delta_file_t sdf = (svnsup_delta_file_t)file_baton;

	(void)pool;
	SVNSUP_DEBUG("%s(%s)\n", __func__, text_checksum);
	svnsup_delta_close_file(sdf, text_checksum);
	return (SVN_NO_ERROR);
}

svn_error_t *
absent_file(const char *path,
    void *parent_baton,
    apr_pool_t *pool)
{
	svnsup_delta_t sd = (svnsup_delta_t)parent_baton;

	(void)sd;
	(void)pool;
	SVNSUP_DEBUG("%s(%s)\n", __func__, path);
	return (SVN_NO_ERROR);
}

svn_error_t *
close_edit(void *edit_baton,
    apr_pool_t *pool)
{

	(void)edit_baton;
	(void)pool;
	SVNSUP_DEBUG("%s()\n", __func__);
	return (SVN_NO_ERROR);
}

svn_error_t *
abort_edit(void *edit_baton,
    apr_pool_t *pool)
{

	(void)edit_baton;
	(void)pool;
	SVNSUP_DEBUG("%s()\n", __func__);
	return (SVN_NO_ERROR);
}

struct svn_delta_editor_t delta_editor = {
	.set_target_revision = set_target_revision,
	.open_root = open_root,
	.delete_entry = delete_entry,
	.add_directory = add_directory,
	.open_directory = open_directory,
	.change_dir_prop = change_dir_prop,
	.close_directory = close_directory,
	.absent_directory = absent_directory,
	.add_file = add_file,
	.open_file = open_file,
	.apply_textdelta = apply_textdelta,
	.change_file_prop = change_file_prop,
	.close_file = close_file,
	.absent_file = absent_file,
	.close_edit = close_edit,
	.abort_edit = abort_edit,
};
