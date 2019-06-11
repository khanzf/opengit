/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Farhan Khan. All rights reserved.
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

#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "index.h"

/*
    XXX This code could be cleaned up by having a local offset value
    and updating it at the end of the function
*/

/*
struct cache_tree *
parse_treeentries(unsigned char *indexmap, int *offset)
{
	struct cache_tree *cache_tree;
	unsigned char *endptr;
	int q=0;

	cache_tree = malloc(sizeof(struct cache_tree));

	while (indexmap[*offset])
		*offset = *offset + 1;
	*offset = *offset + 1;

	cache_tree->entry_count = strtol((const char *)(indexmap + (int)*offset), (char **)&endptr, 10);
	*offset = *offset + (endptr - (indexmap + *offset));

	*offset = *offset + 1; // Jump past the space

	cache_tree->subtree_count = strtol((const char *)(indexmap + *offset), (char **)&endptr, 10);
	*offset = *offset + (endptr - (indexmap + *offset));

	*offset = *offset + 1; // Jump past the new line

	// XXX rename from objectname to sha?
	memcpy(cache_tree->objectname, indexmap + *offset, 20); 
	printf("Tree: %x%x%x%x%x%x%x%x%x%x\n", cache_tree->objectname[0], cache_tree->objectname[1],
	cache_tree->objectname[2], cache_tree->objectname[3], cache_tree->objectname[4],
	cache_tree->objectname[5], cache_tree->objectname[6], cache_tree->objectname[7],
	cache_tree->objectname[8], cache_tree->objectname[9]);

	if (cache_tree->subtree_count >= 0)
		*offset = *offset + 20;

	cache_tree->subtree = malloc(sizeof(struct cache_tree *) * q);

	for(q=0;q<cache_tree->subtree_count;q++)
		cache_tree->subtree[q] = parse_treeentries(indexmap, offset);

	return (cache_tree);
}
*/

/*
 * Must free: dircleaf
 */
static struct dircleaf *
dirc_entry(unsigned char *indexmap, long *offset, int entries)
{
	struct dircleaf *dircleaf;
	struct dircentry *dircentry;
	struct dircextentry *dircextentry;
	int i;

	dircentry = (struct dircentry *)((char *)indexmap + *offset);

	dircleaf = malloc(sizeof(struct dircleaf) * entries);

	for(i=0;i<entries;i++) {
		dircentry = (struct dircentry *)((char *)indexmap + *offset);
		if (ntohs(dircentry->flags) & CE_EXTENDED) {
			dircextentry = (struct dircextentry *)((char *)indexmap + *offset);
			dircleaf[i].isextended = true;
			dircleaf[i].flags2 = dircextentry->flags;
			strlcpy(dircleaf[i].name, dircextentry->name, strlen(dircextentry->name)+1);

			*offset += (72 + strlen(dircextentry->name)) & ~0x7;
		}
		else {
			dircleaf[i].isextended = false;
			strlcpy(dircleaf[i].name, dircentry->name, strlen(dircentry->name)+1);
			*offset += (70 + strlen(dircentry->name)) & ~0x7;
		}

		dircleaf[i].ctime_sec = dircentry->ctime_sec;
		dircleaf[i].ctime_nsec = dircentry->ctime_nsec;
		dircleaf[i].mtime_sec = dircentry->mtime_sec;
		dircleaf[i].mtime_nsec = dircentry->mtime_nsec;
		dircleaf[i].dev = dircentry->dev;
		dircleaf[i].ino = dircentry->ino;
		dircleaf[i].mode = dircentry->mode;
		dircleaf[i].uid = dircentry->uid;
		dircleaf[i].gid = dircentry->gid;
		dircleaf[i].size = dircentry->size;
		memcpy(dircleaf[i].sha, dircentry->sha, 20);
		dircleaf[i].flags = dircentry->flags;
		

		printf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x %s\n",
				dircleaf[i].sha[0],
				dircleaf[i].sha[1],
				dircleaf[i].sha[2],
				dircleaf[i].sha[3],
				dircleaf[i].sha[4],
				dircleaf[i].sha[5],
				dircleaf[i].sha[6],
				dircleaf[i].sha[7],
				dircleaf[i].sha[8],
				dircleaf[i].sha[9],
				dircleaf[i].name);

	}

	return dircleaf;
}

struct indextree *
index_parse(unsigned char *indexmap, off_t indexsize)
{
	struct indextree *indextree = NULL;
	struct indextree *currentleaf = NULL;
	struct indexhdr *indexhdr;
	long offset = 0;

	indextree = malloc(sizeof(indextree));
	indextree->next = NULL;
	currentleaf = indextree;

	if (indextree == NULL) {
		fprintf(stderr, "Unable to allocate memory, exiting.\n");
		exit(0);
	}

	while (offset < indexsize) {
		indexhdr = (struct indexhdr *)((char *)indexmap + offset);

		if (!memcmp(indexhdr->sig, "DIRC", 4)) {
			currentleaf->type = DIRCACHE;
			currentleaf->entries = indexhdr->entries;

			offset += sizeof(struct indexhdr);
			currentleaf->data = dirc_entry(indexmap, &offset, ntohl(indexhdr->entries));
		}
		else if (!memcmp(indexhdr->sig, "TREE", 4)) {
			exit(0);
			/*
			uint32_t extsize;
			memcpy(&extsize, (struct indexhdr *)((char *)indexmap + offset + 4), 4);
			offset += 8;
			indexcache->cache_tree = parse_treeentries(indexmap, &offset);	
			offset += extsize + 8;
			*/
		}
		else {
			printf("Found something after DIRC\n");
			printf("Found this: %c%c%c%c\n", indexhdr->sig[0], indexhdr->sig[1], indexhdr->sig[2], indexhdr->sig[3]);
			printf("Exiting.\n");
			exit(0);
		}
		currentleaf->next = NULL;
		currentleaf = currentleaf->next;
	}

	return (indextree);
}
