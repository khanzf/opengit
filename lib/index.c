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
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "index.h"

/* Write and update the SHA Context */
void
write_sha(SHA1_CTX *ctx, int fd, void *data, int len)
{
	write(fd, data, len);
	SHA1_Update(ctx, data, len);
}

/*
 * Description: Captures the cache tree data
 * ToFree: Requires treeleaf->subtree to be freed
 */
static struct treeleaf *
tree_entry(unsigned char *indexmap, off_t *offset, int ext_size)
{
	struct treeleaf *treeleaf;
	unsigned char *endptr;
	int entry_count, trees;
	int x, s;

	treeleaf = malloc(sizeof(struct treeleaf));

	treeleaf->ext_size = ext_size;
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
read_dirc(unsigned char *indexmap, off_t *offset, int entries)
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
		/*
		 * This line of code is commented out but left in. This is because git's
		 * documentation erroneously has a 2-byte flag. However in practice this
		 * is not used in git version 2.0
		 */
		// dircleaf[i].flags = dircentry->flags;
	}

	return (dircleaf);
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
	indextree->version = htonl(indexhdr->version);
	indextree->entries = htonl(indexhdr->entries);

	offset += sizeof(struct indexhdr);
//	indextree->dircleaf = read_dirc(indexmap, &offset, indextree->entries);

	/*
	 * This calculation is derived from GPL git
	 * The 'HASH_SIZE/2 - 8' is based on the trailing hash length
	 * and adding the padding of up to 8 characters.
	 */
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

/*
 * Writes the tree cache portion of the index file
 * Requires a populated indextree and index file descriptor
 * ToFree; Nothing
 */
static void
write_tree(SHA1_CTX *indexctx, struct indextree *indextree, int indexfd)
{
	int x;
	char tmp[100];
	struct treeleaf *treeleaf = indextree->treeleaf;

	write_sha(indexctx, indexfd, "TREE", 4);

	x = htonl(treeleaf->ext_size);
	write_sha(indexctx, indexfd, &x, 4);

	write_sha(indexctx, indexfd, "\x00", 1);
	x = snprintf(tmp, 100, "%u %u\n", treeleaf->entry_count, treeleaf->local_tree_count);
	write_sha(indexctx, indexfd, tmp, x);

	write_sha(indexctx, indexfd, treeleaf->sha, HASH_SIZE/2);

	for(int s = 0;s<treeleaf->total_tree_count;s++) {
		x = snprintf(tmp, 100, "%s", treeleaf->subtree[s].path);
		write_sha(indexctx, indexfd, tmp, x);
		write_sha(indexctx, indexfd, "\x00", 1);
		x = snprintf(tmp, 100, "%u %u\n", treeleaf->subtree[s].entries, treeleaf->subtree[s].sub_count);
		write_sha(indexctx, indexfd, tmp, x);
		write_sha(indexctx, indexfd, treeleaf->subtree[s].sha, HASH_SIZE/2);
	}
}

/*
 * Writes the index file
 * Requires a populated indextree and index file descriptor
 * ToFree: Nothing
 */
void
index_write(struct indextree *indextree, int indexfd)
{
	struct dircleaf *dircleaf = indextree->dircleaf;
	SHA1_CTX indexctx;
	uint8_t sha[20];
	uint32_t convert;
	char null = 0x00;
	int padding;
	uint32_t fourbyte;
	uint16_t twobyte;
	uint8_t onebyte;

	SHA1_Init(&indexctx);

	/* DIRC signature */
	write_sha(&indexctx, indexfd, "DIRC", 4);
	/* Write version */
	convert = htonl(indextree->version);
	write_sha(&indexctx, indexfd, &convert, 4);
	/* Write version */
	convert = htonl(indextree->entries);
	write_sha(&indexctx, indexfd, &convert, 4);

	for(int i=0;i<indextree->entries;i++) {
		fourbyte = htonl(dircleaf[i].ctime_sec);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		fourbyte = htonl(dircleaf[i].ctime_nsec);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		fourbyte = htonl(dircleaf[i].mtime_sec);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		fourbyte = htonl(dircleaf[i].mtime_nsec);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		fourbyte = htonl(dircleaf[i].dev);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		fourbyte = htonl(dircleaf[i].ino);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		fourbyte = htonl(dircleaf[i].mode);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		fourbyte = htonl(dircleaf[i].uid);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		fourbyte = htonl(dircleaf[i].gid);
		write_sha(&indexctx, indexfd, &dircleaf[i].gid, 4);

		fourbyte = htonl(dircleaf[i].size);
		write_sha(&indexctx, indexfd, &fourbyte, 4);

		for(int v=0;v<HASH_SIZE/2;v++){
			onebyte = (dircleaf[i].sha[v] << 4) | (dircleaf[i].sha[v] >> 4);
			write_sha(&indexctx, indexfd, &onebyte, 1);
		}
		twobyte = strlen(dircleaf[i].name);
		twobyte = htons(twobyte);
		write_sha(&indexctx, indexfd, &twobyte, 2);

		write_sha(&indexctx, indexfd, &dircleaf[i].name, strlen(dircleaf[i].name));

		/*
		 * This determines how much null character padding to add after the filename
		 * Another possible function:
		 * padding = 8 - ((strlen(dircleaf[i].name) + 6) % 8);
		 */
		padding = (0x7 & ~(strlen(dircleaf[i].name) - 2)) + 1;

		for(int z=0;z<padding;z++)
			write_sha(&indexctx, indexfd, &null, 1);
	}

	if (indextree->treeleaf)
		write_tree(&indexctx, indextree, indexfd);

	/* Write the trailing SHA */
	SHA1_Final((unsigned char *)sha, &indexctx);
	write(indexfd, sha, HASH_SIZE/2);
}

void
index_generate_indextree(char *mode, uint8_t type, char *sha, char *filename, void *arg)
{
	struct indexpath *indexpath = arg;
	struct indextree *indextree = indexpath->indextree;
	char *path = indexpath->path;
	char *fn = path + strlen(path);

	if (type == OBJ_TREE) {
		strlcat(path, filename, PATH_MAX);
		strlcat(path, "/", PATH_MAX);
		ITERATE_TREE(sha, index_generate_indextree, indexpath);
	}
	else {
		struct dircleaf *curleaf;
		struct stat sb;
		int ret;
		strlcat(path, filename, PATH_MAX);
		ret = stat(indexpath->fullpath, &sb);
		if (ret == -1) {
			fprintf(stderr, "Unable to generate index file, exiting.\n");
			exit(ret);
		}
		indextree->dircleaf = realloc(indextree->dircleaf, sizeof(struct dircleaf) * (indextree->entries+1));
		curleaf = &indextree->dircleaf[indextree->entries];
		curleaf->isextended = 0;
		curleaf->ctime_sec	= sb.st_ctime;
		curleaf->ctime_nsec 	= sb.st_ctim.tv_nsec;
		curleaf->mtime_sec	= sb.st_mtime;
		curleaf->mtime_nsec	= sb.st_mtim.tv_nsec;
		curleaf->dev		= sb.st_dev;
		curleaf->ino		= sb.st_ino;
		curleaf->mode		= sb.st_mode;
		curleaf->uid		= sb.st_uid;
		curleaf->gid		= sb.st_gid;
		curleaf->size		= sb.st_size;
		/* SHA assigner would go here */
		curleaf->flags		= 0x0000;
		curleaf->flags2		= 0x0000;

		sha_str_to_bin(sha, curleaf->sha);
		strlcpy(curleaf->name, path, PATH_MAX);

		indextree->entries++;
	}
	*fn = '\0';
}

/*
 * This function recursively iterates through a TREE to produce the data
 * necessary to write the index.
 * ToFree after function: treeleaf->subtree
 */
void
index_generate_treedata(char *mode, uint8_t type, char *sha, char *filename, void *arg)
{
	struct indexpath *indexpath = arg;
	struct indextree *indextree = indexpath->indextree;
	struct treeleaf *treeleaf = indextree->treeleaf;
	struct subtree *next_tree;

	if (type == OBJ_TREE) {
		int local_position, next_position;

		local_position = indexpath->current_position;
		treeleaf->total_tree_count++;
		treeleaf->subtree = realloc(treeleaf->subtree,
		    sizeof(struct subtree)*(treeleaf->total_tree_count));
		next_tree=&treeleaf->subtree[treeleaf->total_tree_count-1];
		next_tree->entries=0;
		next_tree->sub_count=0;
		sha_str_to_bin_network(sha, next_tree->sha);
		strlcpy(next_tree->path, filename, PATH_MAX);

		indexpath->current_position = next_position = treeleaf->total_tree_count;
		treeleaf->subtree[local_position-1].sub_count++;
		ITERATE_TREE(sha, index_generate_treedata, indexpath);
		indexpath->current_position = local_position;

		if (local_position > 0)
			treeleaf->subtree[local_position-1].entries += treeleaf->subtree[next_position-1].entries;
		else
			treeleaf->local_tree_count++;
	}
	else {
		treeleaf->entry_count++;
		if (indexpath->current_position > 0) {
			next_tree = &treeleaf->subtree[indexpath->current_position-1];
			next_tree->entries++;
		}
	}
}

/*
 * Calculate the size of the tree extension
 * The TREE header size is calculated with two parts.
 * 1. The main tree part:
 *    - The initial NULL char (1 byte)
 *    - The length of the entry_count in ASCII (variable size)
 *    - The space character (1 byte)
 *    - The length of the total_tree_count in ASCII (variable size)
 *    - The newline char (1 byte)
 *    - The SHA in bin format (20 bytes)
 * 2. The subtree sections:
 *    - The size of the tree name (variable size)
 *    - The closing NULL character (1 byte)
 *    - The size of the entries in ASCII (variable size)
 *    - The space character (1 byte)
 *    - The size of the subtrees in ASCII (variable size)
 *    - The newline char (1 byte)
 *    - The SHA in bin format (20 bytes)
 * I slightly rearranged these additions out of order, but the sum is the same
 * */
void
index_calculate_tree_ext_size(struct treeleaf *treeleaf)
{
	treeleaf->ext_size += count_digits(treeleaf->entry_count) + count_digits(treeleaf->total_tree_count);
	treeleaf->ext_size += EXT_SIZE_FIXED;

	for(int r=0;r<treeleaf->total_tree_count;r++) {
		treeleaf->ext_size += strlen(treeleaf->subtree[r].path);
		treeleaf->ext_size += count_digits(treeleaf->subtree[r].entries);
		treeleaf->ext_size += count_digits(treeleaf->subtree[r].sub_count);
		treeleaf->ext_size += EXT_SIZE_FIXED;
	}
}
