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
#include <unistd.h>
#include "common.h"
#include "index.h"

/*
 * Description: Captures the cache tree data
 * ToFree: Requires treeleaf->subtree to be freed
 */
static struct treeleaf *
tree_entry(unsigned char *indexmap, off_t *offset, int extsize)
{
	struct treeleaf *treeleaf;
	unsigned char *endptr;
	int entry_count, trees;
	int x, s;

	treeleaf = malloc(sizeof(struct treeleaf));

	/* Capture entry_count for the base */
	entry_count = strtol((char *)indexmap + *offset, (char **)&endptr, 10);
	*offset = endptr - indexmap;

	/* Capture subtree count for the base */
	trees = strtol((char *)indexmap + *offset, (char **)&endptr, 10);
	*offset = endptr - indexmap;

	/* Assign values to treeleaf */
	treeleaf->entry_count = entry_count;
	treeleaf->local_tree_count = trees;

	/* Skip past the new line */
	*offset=*offset+1;

	memcpy(treeleaf->sha, indexmap + *offset, HASH_SIZE/2);
	/* Skip past the SHA value */
	*offset = *offset + HASH_SIZE/2;

	/*
	 * This part captures the number of subtree data
	 * The treecache is catured as treeleaf->local_tree_count. However,
	 * the number of entries in the index file can be more due to the cache
	 * also listing subtrees of the trees. Therefore, the number is, very
	 * unfortunately, realloc()'ed in the loop.
	 */
	treeleaf->subtree = malloc(sizeof(struct subtree) * trees);
	for(s=0;s<trees;s++){
		x = strlcpy(treeleaf->subtree[s].path, (char *)indexmap + *offset, PATH_MAX);
		*offset = *offset + x + 1;

		treeleaf->subtree[s].entries = strtol((char *)indexmap + *offset, (char **)&endptr, 10);
		*offset = endptr - indexmap;
		treeleaf->subtree[s].sub_count = strtol((char *)indexmap + *offset, (char **)&endptr, 10);
		*offset = endptr - indexmap;

		/* Is this the best approach? The size of the trees grows */
		trees+=treeleaf->subtree[s].sub_count;
		treeleaf->subtree = realloc(treeleaf->subtree, sizeof(struct subtree) * trees);

		/* Skip past the new line */
		*offset=*offset+1;

		memcpy(treeleaf->subtree[s].sha, indexmap + *offset, HASH_SIZE/2);
		*offset = *offset + HASH_SIZE/2;
	}

	/* Assign the total number of subtrees in the cache */
	treeleaf->total_tree_count = trees;

	return (treeleaf);
}

/*
 * Must free: dircleaf
 */
static struct dircleaf *
dirc_entry(unsigned char *indexmap, off_t *offset, int entries)
{
	struct dircleaf *dircleaf;
	struct dircentry *dircentry;
	struct dircextentry *dircextentry;

	dircentry = (struct dircentry *)((char *)indexmap + *offset);
	dircleaf = malloc(sizeof(struct dircleaf) * entries);

	for(int i=0;i<entries;i++) {
		dircentry = (struct dircentry *)((char *)indexmap + *offset);
		if (ntohs(dircentry->flags) & DIRC_EXT_FLAG) {
			dircextentry = (struct dircextentry *)((char *)indexmap + *offset);
			dircleaf[i].isextended = true;
			dircleaf[i].flags2 = dircextentry->flags & ~0x0fff;
			strlcpy(dircleaf[i].name, dircextentry->name, strlen(dircextentry->name)+1);

			*offset += (DIRCEXTENTRYSIZE + strlen(dircextentry->name)) & ~0x7;
		}
		else {
			dircleaf[i].isextended = false;
			strlcpy(dircleaf[i].name, dircentry->name, strlen(dircentry->name)+1);
			*offset += (DIRCENTRYSIZE + strlen(dircentry->name)) & ~0x7;
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
	}

	return dircleaf;
}

/*
 * Description: Reads and index file and stores the result in the
 * struct indextree.
 * Arguments: 1. indextree must be pre-allocated
 *            2. indexmap is a buffer to the index file, typically an
 *               mmap(2) of the file
 *            3. indexsize is the size of the index file
 */
void
index_parse(struct indextree *indextree, unsigned char *indexmap, off_t indexsize)
{
	struct indexhdr *indexhdr;
	off_t offset = 0;
	unsigned extsize;

	indexhdr = (struct indexhdr *)((char *)indexmap + offset);
	if (memcmp(indexhdr->sig, "DIRC", 4)) {
		fprintf(stderr, "Does not match???\n");
		exit(0);
	}
	indextree->version = indexhdr->version;
	indextree->entries = indexhdr->entries;

	offset += sizeof(struct indexhdr);
	indextree->dircleaf = dirc_entry(indexmap, &offset, ntohl(indexhdr->entries));

	while (offset <= indexsize - HASH_SIZE/2 - 8) {
		indexhdr = (struct indexhdr *)((char *)indexmap + offset);

		/* Capture the extension size */
		offset = offset + 4;
		memcpy(&extsize, indexmap+offset, 4);
		extsize = htonl(extsize);

		/* Jump past the 4-byte size and NULL character */
		offset = offset + 5;

		/*
		 * GNU git pre-converted "TREE" to a 4-byte value and uses a
		 * switch-case. That might be every so slightly more efficient.
		 */
		if (!memcmp(indexhdr->sig, "TREE", 4))
			/* Skip over the extension size and newline */
			indextree->treeleaf = tree_entry(indexmap, &offset, extsize);
		else {
			fprintf(stderr, "Unknown data at the end of the index, exiting.\n");
			exit(0);
		}
	}
}

void
index_write(struct indextree *indextree, int indexfd)
{
	struct dircleaf *dircleaf = indextree->dircleaf;
	int c;

	c = write(indexfd, "DIRC", 4);
	if (c == -1) {
		fprintf(stderr, "Unable to write to index file, exiting.\n");
		exit(c);

	}
	write(indexfd, &indextree->version, 4);
	write(indexfd, &indextree->entries, 4);

	char null = 0x00;
	int padding;

	for(int i=0;i<ntohl(indextree->entries);i++) {
		write(indexfd, &dircleaf[i].ctime_sec, 4);
		write(indexfd, &dircleaf[i].ctime_nsec, 4);
		write(indexfd, &dircleaf[i].mtime_sec, 4);
		write(indexfd, &dircleaf[i].mtime_nsec, 4);
		write(indexfd, &dircleaf[i].dev, 4);
		write(indexfd, &dircleaf[i].ino, 4);
		write(indexfd, &dircleaf[i].mode, 4);
		write(indexfd, &dircleaf[i].uid, 4);
		write(indexfd, &dircleaf[i].gid, 4);
		write(indexfd, &dircleaf[i].size, 4);
		write(indexfd, &dircleaf[i].sha, 20);
		write(indexfd, &dircleaf[i].flags, 2);
		if (dircleaf[i].isextended == true)
			c = write(indexfd, &dircleaf[i].flags2, 2);

		write(indexfd, &dircleaf[i].name, strlen(dircleaf[i].name));

		/*
		 * This determines how much null character padding to add after the filename
		 * Another possible function:
		 * padding = 8 - ((strlen(dircleaf[i].name) + 6) % 8);
		 */
		padding = (0x7 & ~(strlen(dircleaf[i].name) - 2)) + 1;

		for(int z=0;z<padding;z++)
			write(indexfd, &null, 1);
	}

}
