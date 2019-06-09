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
    GPL git has at least 2 things called "index" in Git.
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
	unsigned int	addr;
};

struct checksum {
	unsigned int	val;
};

struct entry {
	unsigned char	sha[20];
};

struct fan {
	int		count[256];
};

// pack file headers
struct packfileinfo {
	int		version;
	int		nobjects;
	unsigned char	sha[20];
	unsigned char	ctx[20];
};

struct objectinfo {
	unsigned long	offset;		// The object header from the file's start
	uint32_t	crc;

	unsigned long	psize;		// Size of the object content
	unsigned long	isize;		// Inflated size
	unsigned long	used;		// Bytes the header consumes
	unsigned int	ftype;		// Final type
	unsigned int	ptype;		// Pack type

	/* Values used by ofs_delta objects */
	unsigned long	deflated_size;
	unsigned long	ofsbase;	// Offset of object + object hdr
	unsigned long	ofshdrsize;	// The sizeof the ofs hdr 
	unsigned long	*deltas;	// Offset of deltas + delta hdrs
	int		ndeltas;	// Number of deltas

	unsigned char	*data;		// Pointer to inflated data

};

/* Used to store object information when creating the index */
struct index_entry {
	int		offset;
	int		type;
	uint32_t	crc;
	unsigned char	digest[20];
};

/* Used in the callback to get index information */
struct index_generate_arg {
	int		bytes;
	SHA1_CTX	shactx;
};

typedef void 	 packhandler(int, struct objectinfo *, void *);

ssize_t		 sha_write(int fd, const void *buf, size_t nbytes, SHA1_CTX *idxctx);
int		 pack_find_sha_offset(unsigned char *sha, unsigned char *idxmap);
int		 pack_get_packfile_offset(char *sha_str, char *filename);
int		 pack_parse_header(int packfd, struct packfileinfo *packfileinfo, SHA1_CTX *packctx);
void		 pack_object_header(int packfd, int offset, struct objectinfo *objectinfo, SHA1_CTX *packctx);
int		 pack_get_object_meta(int packfd, int offset, struct packfileinfo *packfileinfo, struct index_entry *index_entry,
		     SHA1_CTX *packctx, SHA1_CTX *idxctx);
unsigned char	*pack_get_index_bytes_cb(unsigned char *buf, int size, int deflated_bytes, void *arg);
void		 pack_delta_content(int packfd, struct objectinfo *objectinfo, SHA1_CTX *packctx);
void		 write_index_header(int idxfd, SHA1_CTX *idxctx);
void		 write_hash_count(int idxfd, struct index_entry *index_entry, SHA1_CTX *idxctx);
void		 write_hashes(int idxfd, struct packfileinfo *packfileinfo, struct index_entry *index_entry, SHA1_CTX *idxctx);
void		 write_crc_table(int idxfd, struct packfileinfo *packfileinfo, struct index_entry *index_entry, SHA1_CTX *idxctx);
void		 write_32bit_table(int idxfd, struct packfileinfo *packfileinfo, struct index_entry *index_entry, SHA1_CTX *idxctx);
void		 write_checksums(int idxfd, struct packfileinfo *packfileinfo, SHA1_CTX *idxctx);
void		 pack_build_index(int idxfd, struct packfileinfo *packfileinfo, struct index_entry *index_entry, SHA1_CTX *idxctx);
int		 sortindexentry(const void *a, const void *b);
int		 read_sha_update(void *buf, size_t count, void *arg);
void		 pack_content_handler(char *sha, packhandler packhandler, void *args);
void		 pack_buffer_cb(int packfd, struct objectinfo *objectinfo, void *pargs);

#endif
