/* 
 * Copyright (C) 2012 Simon Wunderlich <siwu@hrz.tu-chemnitz.de>
 * Copyright (C) 2012 Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * 
 */

/*
 * This program has been created to aid open source spectrum
 * analyzer development for Qualcomm/Atheros AR92xx and AR93xx
 * based chipsets.
 */

#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "fft_eval.h"

#include "fft_linux.h"

/* read_file - reads an file into a big buffer and returns it
 *
 * @fname: file name
 *
 * returns the buffer with the files content
 */
static char *
read_file(char *fname, size_t *size)
{
	FILE *fp;
	char *buf = NULL;
	size_t ret;

	fp = fopen(fname, "r");

	if (!fp)
		return NULL;

	*size = 0;
	while (!feof(fp)) {

		buf = realloc(buf, *size + 4097);
		if (!buf)
			return NULL;

		ret = fread(buf + *size, 1, 4096, fp);
		*size += ret;
	}
	fclose(fp);

	buf[*size] = 0;

	return buf;
}

/*
 * read_scandata - reads the fft scandata and compiles a linked list of datasets
 *
 * @fname: file name
 *
 * returns 0 on success, -1 on error.
 */
int
read_scandata_linux(char *fname, struct scanresult **result_list)
{
	int scanresults_n = 0;
	char *pos, *scandata;
	size_t len, sample_len;
	struct scanresult *result;
	struct fft_sample_tlv *tlv;
	struct scanresult *tail = *result_list;

	scandata = read_file(fname, &len);
	if (!scandata)
		return -1;

	pos = scandata;

	while (pos - scandata < len) {
		tlv = (struct fft_sample_tlv *) pos;
		sample_len = sizeof(*tlv) + tlv->length;
		pos += sample_len;
		if (tlv->type != ATH_FFT_SAMPLE_HT20) {
			fprintf(stderr, "unknown sample type (%d)\n", tlv->type);
			continue;
		}

		if (sample_len != sizeof(result->sample)) {
			fprintf(stderr, "wrong sample length (have %d, expected %d)\n", sample_len, sizeof(result->sample));
			continue;
		}

		result = malloc(sizeof(*result));
		if (!result)
			continue;

		memset(result, 0, sizeof(*result));
		memcpy(&result->sample, tlv, sizeof(result->sample));
		fprintf(stderr, "copy %d bytes\n", sizeof(result->sample));
		
		if (tail)
			tail->next = result;
		else
			(*result_list) = result;

		tail = result;

		scanresults_n++;
	}

	fprintf(stderr, "read %d scan results\n", scanresults_n);
	return (scanresults_n);
}

