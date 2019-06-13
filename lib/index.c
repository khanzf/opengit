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
#include "common.h"
#include "index.h"

static struct treeleaf *
tree_entry(unsigned char *indexmap, long *offset, int extsize)
{
	struct treeleaf *treeleaf;
	unsigned char *endptr;
	int entry_count, trees;
	uint8_t treeid[20];
	int x, s;

	treeleaf = malloc(sizeof(struct treeleaf));

	/* Capture entry_count for the base */
	entry_count = strtol(indexmap + *offset, (char **)&endptr, 10);
	*offset = endptr - indexmap;

	/* Capture subtree count for the base */
	trees = strtol(indexmap + *offset, (char **)&endptr, 10);
	*offset = endptr - indexmap;

	/* Assign values to treeleaf */
	treeleaf->entry_count = entry_count;
	treeleaf->local_tree_count = trees;

	/* Skip past the new line */
	*offset=*offset+1;

	memcpy(treeid, indexmap + *offset, 20);


	/* Skip past the SHA value */
	*offset = *offset + 20;

	treeleaf->subtree = malloc(sizeof(struct subtree) * trees);
	for(s=0;s<trees;s++){
		x = strlcpy(treeleaf->subtree[s].path, indexmap + *offset, PATH_MAX);
//		printf("Subpath: %s\t", treeleaf->subtree[s].path);
		*offset = *offset + x + 1;

		treeleaf->subtree[s].entries = strtol(indexmap + *offset, (char **)&endptr, 10);
		*offset = endptr - indexmap;
		treeleaf->subtree[s].sub_count = strtol(indexmap + *offset, (char **)&endptr, 10);
		*offset = endptr - indexmap;

		/* Is this the best approach? The size of the trees grows */
		trees+=treeleaf->subtree[s].sub_count;
		treeleaf->subtree = realloc(treeleaf->subtree, sizeof(struct subtree) * trees);

		/* Skip past the new line */
		*offset=*offset+1;

//		printf("entry_count: %d\tsubtrees: %d\t", treeleaf->subtree[s].entries, treeleaf->subtree[s].sub_count);

		memcpy(treeid, indexmap + *offset, 20);
//		for(x=0;x<20;x++)
//			printf("%02x", indexmap[*offset+x]);
//		printf("\n");
		*offset = *offset + 20;
	}

	/* Assign the total number of subtrees in the cache */
	treeleaf->total_tree_count = trees;

	return (treeleaf);
}

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

void
index_parse(struct indextree *indextree, unsigned char *indexmap, off_t indexsize)
{
	struct indexhdr *indexhdr;
	off_t offset = 0;
	unsigned extsize;

	while (offset < indexsize) {
		indexhdr = (struct indexhdr *)((char *)indexmap + offset);

		if (!memcmp(indexhdr->sig, "DIRC", 4)) {
			offset += sizeof(struct indexhdr);
			indextree->dircleaf = dirc_entry(indexmap, &offset, ntohl(indexhdr->entries));
		}
		else if (!memcmp(indexhdr->sig, "TREE", 4)) {
			printf("\nTree object\n");
			offset = offset + 4;

			memcpy(&extsize, indexmap+offset, 4);
			extsize = htonl(extsize);
			printf("Ext Size: %d\n", extsize);
			// Skip over the extension size
			offset = offset + 4;

			// Skip over the newline
			offset = offset + 1;
			indextree->treeleaf = tree_entry(indexmap, &offset, extsize);

			for(int xx = 0; xx < indextree->treeleaf->total_tree_count; xx++){
				printf("%s %d %d\n",
						indextree->treeleaf->subtree[xx].path,
						indextree->treeleaf->subtree[xx].entries,
						indextree->treeleaf->subtree[xx].sub_count);
			}
			/*
			*/
		}
		else {
			printf("Found something after DIRC\n");
			printf("Found this: %c%c%c%c\n", indexhdr->sig[0], indexhdr->sig[1], indexhdr->sig[2], indexhdr->sig[3]);
			printf("Exiting.\n");
			exit(0);
		}
	}
	printf("Comes here\n");
}
