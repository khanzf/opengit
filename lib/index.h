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

#ifndef INDEX_H
#define INDEX_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include "common.h"

/* Header source Documentation/technical/index-format.txt */

#define	NULLCHAR	1
#define SPACECHAR	1
#define NEWLINE		1
#define EXT_SIZE_FIXED	NULLCHAR + SPACECHAR + NEWLINE + HASH_SIZE/2

struct indexhdr {
	char			sig[4];		/* Cache type */
	uint32_t		version;	/* Version Number */
	uint32_t		entries;	/* Number of extensions */
};

struct dircentry {
	uint32_t		ctime_sec;
	uint32_t		ctime_nsec;
	uint32_t		mtime_sec;
	uint32_t		mtime_nsec;
	uint32_t		dev;
	uint32_t		ino;
	uint32_t		mode;
	uint32_t		uid;
	uint32_t		gid;
	uint32_t		size;
	uint8_t			sha[HASH_SIZE/2];
	uint16_t		flags;
#define DIRCENTRYSIZE		70
	char			name[1];
} __packed;

#define DIRC_EXT_FLAG		BIT(14)

struct dircextentry {
	uint32_t		ctime_sec;
	uint32_t		ctime_nsec;
	uint32_t		mtime_sec;
	uint32_t		mtime_nsec;
	uint32_t		dev;
	uint32_t		ino;
	uint32_t		mode;
	uint32_t		uid;
	uint32_t		gid;
	uint32_t		size;
	uint8_t			sha[20];
	uint16_t		flags;
	uint16_t		flags2;
#define DIRCEXTENTRYSIZE	72
	char			name[1];
} __packed;

struct dircleaf {
	bool			 isextended;
	uint32_t		 ctime_sec;
	uint32_t		 ctime_nsec;
	uint32_t		 mtime_sec;
	uint32_t		 mtime_nsec;
	uint32_t		 dev;
	uint32_t		 ino;
	uint32_t		 mode;
	uint32_t		 uid;
	uint32_t		 gid;
	uint32_t		 size;
	uint8_t			 sha[HASH_SIZE/2];
	uint16_t		 flags;
	uint16_t		 flags2; /* Only for the extended type */
	char			 name[PATH_MAX];
};

struct subtree {
	char			path[PATH_MAX];
	uint8_t			sha[HASH_SIZE/2];
	int			entries;
	int			sub_count;
};

struct treeleaf {
	int			ext_size;
	uint8_t			sha[HASH_SIZE/2];
	unsigned int		entry_count;
	unsigned int		local_tree_count;
	unsigned int		total_tree_count;
	struct			subtree *subtree;
};

struct indextree {
	int			 version;
	int			 entries;
	struct dircleaf		*dircleaf;
	struct treeleaf		*treeleaf;
};

/*
 * This is used to facilitate converting
 * a tree object to an indextree
 */
struct indexpath {
	struct indextree *indextree;
	char *fullpath;
	char *path;

	int current_position;
};

void		index_parse(struct indextree *indextree, unsigned char *indexmap, off_t indexsize);
void		index_write(struct indextree *indextree, int indexfd);
void		index_generate_indextree(char *mode, uint8_t type, char *sha, char *filename, void *arg);
void		index_generate_treedata(char *mode, uint8_t type, char *sha, char *filename, void *arg);
void		index_calculate_tree_ext_size(struct treeleaf *treeleaf);

#endif
