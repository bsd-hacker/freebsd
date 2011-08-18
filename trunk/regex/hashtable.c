/* $FreeBSD$ */

/*-
 * Copyright (C) 2011 Gabor Kovesdan <gabor@FreeBSD.org>
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

#include <sys/hash.h>
#include <hashtable.h>
#include <stdlib.h>
#include <string.h>

hashtable
*hashtable_init(size_t table_size, size_t key_size, size_t value_size)
{
	hashtable *tbl;

	tbl = malloc(sizeof(hashtable));
	if (tbl == NULL)
		return (NULL);

	tbl->entries = calloc(sizeof(hashtable_entry *), table_size);
	if (tbl->entries == NULL) {
		free(tbl);
		return (NULL);
	}

	tbl->table_size = table_size;
	tbl->usage = 0;
	tbl->key_size = key_size;
	tbl->value_size = value_size;

	return (tbl);
}

int
hashtable_put(hashtable *tbl, const void *key, const void *value)
{
	uint32_t hash = 0;

	if (tbl->table_size == tbl->usage)
		return (-1);

	hash = hash32_buf(key, tbl->key_size, hash);
	hash %= tbl->table_size;

	while (tbl->entries[hash] != NULL)
		hash = (hash >= tbl->table_size) ? 0 : hash + 1;

	tbl->entries[hash] = malloc(sizeof(hashtable_entry));
	if (tbl->entries[hash] == NULL)
		return (-1);

	tbl->entries[hash]->key = malloc(tbl->key_size);
	if (tbl->entries[hash]->key == NULL) {
		free(tbl->entries[hash]);
		return (-1);
	}

	tbl->entries[hash]->value = malloc(tbl->value_size);
	if (tbl->entries[hash]->value == NULL) {
		free(tbl->entries[hash]->key);
		free(tbl->entries[hash]);
		return (-1);
	}

	memcpy(&tbl->entries[hash]->key, key, tbl->key_size);
	memcpy(&tbl->entries[hash]->value, value, tbl->value_size);
	tbl->usage++;

	return (0);
}

static hashtable_entry
*hashtable_lookup(const hashtable *tbl, const void *key)
{
	uint32_t hash = 0;

	hash = hash32_buf(key, tbl->key_size, hash);
	hash %= tbl->table_size;

	for (;;) {
		if (tbl->entries[hash] == NULL)
			return (NULL);
		else if (memcmp(key, &tbl->entries[hash]->key,
		    tbl->key_size) == 0)
			return (tbl->entries[hash]);

		hash = (hash == tbl->table_size) ? 0 : hash + 1;
  	}
}

int
hashtable_get(hashtable *tbl, const void *key, void *value)
{
	hashtable_entry *entry;

	entry = hashtable_lookup(tbl, key);
	if (entry == NULL)
		return (-1);

	memcpy(value, &entry->value, tbl->value_size);
	return (0);
}

int
hashtable_remove(hashtable *tbl, const void *key)
{
	hashtable_entry *entry;

	entry = hashtable_lookup(tbl, key);
	if (entry == NULL)
		return (-1);

//	free(entry->key);
//	free(entry->value);
	free(entry);

	tbl->usage--;

	return (0);
}

void
hashtable_free(hashtable *tbl)
{

	if (tbl == NULL)
		return;

	for (unsigned int i = 0; i < tbl->table_size; i++)
		if (tbl->entries[i] != NULL) {
//			free(tbl->entries[i]->key);
//			free(tbl->entries[i]->value);
//			free(tbl->entries[i]);
	}
	free(tbl->entries);
}
