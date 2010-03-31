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

// XXX documentation + error handling
svn_error_t *
txdelta_window_handler(svn_txdelta_window_t *window, void *baton)
{
	svnsup_delta_file_t sdf = (svnsup_delta_file_t)baton;
	const svn_txdelta_op_t *op;
	unsigned int txtid = 0;
	int i, ret;

	SVNSUP_DEBUG("%s()\n", __func__);

	if (window == NULL)
		return (SVN_NO_ERROR);

	if (window->new_data != NULL && window->new_data->len > 0) {
		SVNSUP_DEBUG("%lu bytes of data\n",
		    (unsigned long)window->new_data->len);
		ret = svnsup_delta_file_text(sdf, window->new_data->data,
		    window->new_data->len, &txtid);
		SVNSUP_SVNSUP_ERROR(ret, "svnsup_delta_file_text()");
	}

	for (i = 0, op = window->ops; i < window->num_ops; ++i, ++op) {
		switch (op->action_code) {
		case svn_txdelta_source:
			svnsup_delta_file_copy(sdf, op->offset, op->length);
			break;
		case svn_txdelta_target:
			svnsup_delta_file_repeat(sdf, op->offset, op->length);
			break;
		case svn_txdelta_new:
			svnsup_delta_file_insert(sdf, txtid, op->offset,
			    op->length);
			break;
		default:
			SVNSUP_ASSERT(0, "invalid window operation");
			break;
		}
	}
	return (SVN_NO_ERROR);
}
