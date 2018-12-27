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

#ifndef PACK_H
#define PACK_H

#include <sys/types.h>
#include <stdint.h>
#include <sha.h>


/*
Header source Documentation/technical/multi-pack-index.txt
*/

/*
    GNU git has at least 2 things called "index" in Git.
    These are for files located in .git/objects/pack/[*].idx
*/

/* From Documentation/technical/pack-format.txt */
#define OBJ_COMMIT		1
#define OBJ_TREE		2
#define OBJ_BLOB		3
#define OBJ_TAG			4
#define OBJ_OFS_DELTA		6
#define OBJ_REF_DELTA		7

// idx file headers
struct offset {
	unsigned int addr;
};

struct checksum {
	unsigned int val;
};

struct entry {
	unsigned char sha[20];
};

struct fan {
	int count[256];
};

// pack file headers
struct packfilehdr {
//	char sig[4];
	int version;
	int nobjects;
};

struct objectinfohdr {
	uint8_t	size:4;
	uint8_t	type:3;
	uint8_t	more:1;
	uint8_t	unused[56];
} __packed;

struct objectinfo {
	int offset; // The object header from the file's start

	unsigned long size; // Size of the object content
	unsigned long used; // Bytes the header consumes
};

// Shared by both idx and pack files
struct packhdr {
	uint8_t		sig[4];
};

/* Used to store object information when creating the index */
struct object_index_entry {
	int offset;
	int type;
	char sha[41];
};

/* Used in the callback to get index information */
struct index_generate_arg {
	int bytes;
	SHA1_CTX *shactx;
};

int pack_find_sha_offset(unsigned char *sha, unsigned char *idxmap);
void pack_uncompress_object(int packfd);
int pack_get_packfile_offset(char *sha_str, char *filename);
void pack_parse_header(int packfd, struct packfilehdr *packfilehdr);
void pack_object_header(int packfd, int offset, struct objectinfo *objectinfo);
unsigned char *pack_get_index_bytes_cb(unsigned char *buf, int size, void *arg, \
	int deflated_bytes);


#endif
