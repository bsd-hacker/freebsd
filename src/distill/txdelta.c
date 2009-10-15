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
txdelta_window_handler(svn_txdelta_window_t *window, void *baton)
{
	const svn_txdelta_op_t *op;
	int i;

	(void)baton;
	SVNSUP_DEBUG("%s()\n", __func__);

	if (!debug)
		return (SVN_NO_ERROR);

	if (window == NULL) {
		fprintf(stderr, "end of delta\n");
		return (SVN_NO_ERROR);
	} else {
		fprintf(stderr, "delta\n");
	}

	fprintf(stderr, "  src off: %ld\n", (long)window->sview_offset);
	fprintf(stderr, "  src len: %ld\n", (long)window->sview_len);
	fprintf(stderr, "  tgt len: %ld\n", (long)window->tview_len);
	fprintf(stderr, "  ops: %d (%d src)\n", window->num_ops, window->src_ops);
	for (i = 0, op = window->ops; i < window->num_ops; ++i, ++op) {
		fprintf(stderr, "  op #%d: ", i);
		switch (op->action_code) {
		case svn_txdelta_source:
			fprintf(stderr, "src %ld:%ld\n",
			    (long)op->offset, (long)op->length);
			break;
		case svn_txdelta_target:
			fprintf(stderr, "tgt %ld:%ld\n",
			    (long)op->offset, (long)op->length);
			break;
		case svn_txdelta_new:
			fprintf(stderr, "new %ld:%ld\n",
			    (long)op->offset, (long)op->length);
			if (verbose)
				fprintf(stderr, "%.*s\n", (int)op->length,
				    window->new_data->data + op->offset);
			break;
		default:
			fprintf(stderr, "???\n");
			break;
		}
	}
	return (SVN_NO_ERROR);
}
