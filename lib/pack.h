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
#define OBJ_NONE		0
#define OBJ_COMMIT		1
#define OBJ_TREE		2
#define OBJ_BLOB		3
#define OBJ_TAG			4
// Reserved			5
#define OBJ_OFS_DELTA		6
#define OBJ_REF_DELTA		7

static const char *object_name[] = {
	NULL,
	"commit",
	"tree",
	"blob",
	"tag",
	NULL,
	"obj_ofs_delta",
	"obj_ref_delta"
};

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
	int version;
	int nobjects;
};

struct objectinfo {
	unsigned long offset; // The object header from the file's start
	uint32_t crc;

	unsigned long psize; 	// Size of the object content
	unsigned long isize; 	// Inflated size
	unsigned long used; 	// Bytes the header consumes
	unsigned int ftype; 	// Final type
	unsigned int ptype; 	// Pack type

	/* Values used by ofs_delta objects */
	unsigned long deflated_size;
	unsigned long ofsbase;	// Offset of object + object hdr
	unsigned long ofshdrsize; // The sizeof the ofs hdr 
	unsigned long *deltas;	// Offset of deltas + delta hdrs
	int ndeltas;		// Number of deltas

	unsigned char *data;	// Pointer to inflated data

};

// Shared by both idx and pack files
struct packhdr {
	uint8_t		sig[4];
};

/* Used to store object information when creating the index */
struct object_index_entry {
	int offset;
	int type;
	uint32_t crc;
	unsigned char digest[20];
};

/* Used in the callback to get index information */
struct index_generate_arg {
	int bytes;
	SHA1_CTX shactx;
};

int pack_find_sha_offset(unsigned char *sha, unsigned char *idxmap);
int pack_get_packfile_offset(char *sha_str, char *filename);
void pack_parse_header(int packfd, struct packfilehdr *packfilehdr);
void pack_object_header(int packfd, int offset, struct objectinfo *objectinfo);
unsigned char *pack_get_index_bytes_cb(unsigned char *buf, int size, int deflated_bytes, void *arg);
void pack_delta_content(int packfd, struct objectinfo *objectinfo);
int sortindexentry(const void *a, const void *b);


#endif
