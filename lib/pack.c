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
#include <sys/mman.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include <sha.h>
#include "buffering.h"
#include "zlib-handler.h"
#include "pack.h"
#include "common.h"
#include "ini.h"

/*
 * This function is a wrapper for write(1) and also updates
 * a SHA context the data
 */
ssize_t
sha_write(int fd, const void *buf, size_t nbytes, SHA1_CTX *idxctx)
{
	SHA1_Update(idxctx, buf, nbytes);
	return (write(fd, buf, nbytes));
}

int
read_sha_update(void *buf, size_t count, void *arg)
{
	if (!arg)
		return (1);
	SHA1_CTX *context = arg;
	SHA1_Update(context, buf, count);

	return (0);
}

int
sortindexentry(const void *a, const void *b)
{
	struct index_entry *x = (struct index_entry *)a;
	struct index_entry *y = (struct index_entry *)b;
	return (memcmp(x->digest, y->digest, 20));
}

unsigned long
readvint(unsigned char **datap, unsigned char *top)
{
        unsigned char *data = *datap;
        unsigned long opcode, size = 0;
        int i = 0;
        do {
                opcode = *data++;
                size |= (opcode & 0x7f) << i;
                i += 7;
        } while (opcode & BIT(7) && data < top);
        *datap = data;
        return (size);
}

void
applypatch(struct decompressed_object *base, struct decompressed_object *delta, struct objectinfo *objectinfo)
{
	unsigned long size;
	unsigned char *data, *top;
	unsigned char *out, opcode;
	unsigned long cp_off, cp_size;

	data = delta->data;
	top = delta->data + delta->size;

	size = readvint(&data, top);
	if (size != base->size) {
		fprintf(stderr, "Error, bad original set\n");
		exit(0);
	}

	size = readvint(&data, top);

	objectinfo->data = malloc(size);
	objectinfo->isize = size;
	out = objectinfo->data;

	while (data < top) {
		opcode = *data++;
		if (opcode & BIT(7)) {
			cp_off = 0;
			cp_size = 0;

			/* Offset in base */
			if (opcode & BIT(0))
				cp_off |= (*data++ << 0);
			if (opcode & BIT(1))
				cp_off |= (*data++ << 8);
			if (opcode & BIT(2))
				cp_off |= (*data++ << 16);
			if (opcode & BIT(3))
				cp_off |= (*data++ << 24);

			/* Length to copy */
			if (opcode & BIT(4))
				cp_size |= (*data++ << 0);
			if (opcode & BIT(5))
				cp_size |= (*data++ << 8);
			if (opcode & BIT(6))
				cp_size |= (*data++ << 16);

			if (cp_size == 0)
				cp_size = 0x10000;

			if (cp_off + cp_size > base->size) { // || cp_size > size) {
				fprintf(stderr, "Bad length: First\n");
				exit(0);
			}
			memcpy(out, (char *) base->data + cp_off, cp_size);
			out += cp_size;
		} else if (opcode) {
			memcpy(out, data, opcode);
			out += opcode;
			data += opcode;
		} else {
			fprintf(stderr, "Unexpected opcode 0x00, exiting.\n");
			exit(0);
		}
	}
}

/*
 * Identifies the header information of each object in the pack file.
 * If will identify the following:
 * - The object type in objectinfo.ptype
 * - If its an OBJ_OFS_DELTA:
 *   - The real objectype in objectinfo.ftype
 *   - The delta offsets to reconstruct the object
 * - The start of the objects offset (not the header)
 */

int
pack_get_object_meta(int packfd, int offset, struct packfileinfo *packfileinfo,
    struct index_entry *index_entry,
    SHA1_CTX *packctx, SHA1_CTX *idxctx)
{
	int x;
	struct objectinfo objectinfo;
	struct index_generate_arg index_generate_arg;
	char hdr[32];
	int hdrlen;
	char tmpref[2];
	struct two_darg two_darg;

	for (x = 0; x < packfileinfo->nobjects; x++) {
		objectinfo.crc = 0x00;
		lseek(packfd, offset, SEEK_SET);
		pack_object_header(packfd, offset, &objectinfo, packctx);

		switch (objectinfo.ptype) {
		case OBJ_REF_DELTA:
			fprintf(stderr, "OBJ_REF_DELTA: currently not implemented. Exiting.\n");
			exit(0);
			offset += objectinfo.used;
			lseek(packfd, offset, SEEK_SET);
			buf_read(packfd, tmpref, 2, read_sha_update, packctx);
			objectinfo.crc = crc32(objectinfo.crc, tmpref, 2);
			buf_read(packfd, index_entry[x].digest, 20,
			    read_sha_update, packctx);
			offset += 22; /* 20 bytes + 2 for the header */
			break;
		case OBJ_OFS_DELTA:
			SHA1_Init(&index_generate_arg.shactx);
			pack_delta_content(packfd, &objectinfo, packctx);
			hdrlen = sprintf(hdr, "%s %lu",
			    object_name[objectinfo.ftype],
			    objectinfo.isize) + 1;
			SHA1_Update(&index_generate_arg.shactx, hdr, hdrlen);
			SHA1_Update(&index_generate_arg.shactx,
			    objectinfo.data, objectinfo.isize);
			SHA1_Final(index_entry[x].digest,
			    &index_generate_arg.shactx);
			/* The next two are allocated in pack_delta_content */
			free(objectinfo.data);
			free(objectinfo.deltas);

			offset = objectinfo.offset + objectinfo.used +
			    objectinfo.ofshdrsize + objectinfo.deflated_size;
			break;
		case OBJ_COMMIT:
		case OBJ_TREE:
		case OBJ_BLOB:
		case OBJ_TAG:
		default:
			offset += objectinfo.used;
			lseek(packfd, offset, SEEK_SET);
			index_generate_arg.bytes = 0;
			SHA1_Init(&index_generate_arg.shactx);

			hdrlen = sprintf(hdr, "%s %lu",
			    object_name[objectinfo.ftype],
			    objectinfo.psize) + 1;
			SHA1_Update(&index_generate_arg.shactx, hdr, hdrlen);
			two_darg.crc = &objectinfo.crc;
			two_darg.sha = packctx;
			deflate_caller(packfd, zlib_update_crc_sha, &two_darg,
			    pack_get_index_bytes_cb, &index_generate_arg);

			SHA1_Final(index_entry[x].digest,
			    &index_generate_arg.shactx);
			offset += index_generate_arg.bytes;
			break;
		}

		index_entry[x].crc = objectinfo.crc;
		index_entry[x].offset = objectinfo.offset;
	}

	return (offset);
}

/*
 * Writes the header of the index file.
 */
inline void
write_index_header(int idxfd, SHA1_CTX *idxctx)
{
	sha_write(idxfd, "\377tOc", 4, idxctx);
	sha_write(idxfd, "\x00\x00\x00\x02", 4, idxctx);
}

inline void
write_hash_count(int idxfd, struct index_entry *index_entry,
    SHA1_CTX *idxctx)
{
	int hashnum;
	int reversed;
	int x;

	hashnum = 0;

	for (x=0;x<256;x++) {
		while (index_entry[hashnum].digest[0] == x)
			hashnum++;
		reversed = htonl(hashnum);
		sha_write(idxfd, &reversed, 4, idxctx);
	}
}

inline void
write_hashes(int idxfd, struct packfileinfo *packfileinfo,
    struct index_entry *index_entry, SHA1_CTX *idxctx)
{
	int x;

	for (x=0;x<packfileinfo->nobjects; x++)
		sha_write(idxfd, index_entry[x].digest, 20, idxctx);
}

inline void
write_crc_table(int idxfd, struct packfileinfo *packfileinfo,
    struct index_entry *index_entry, SHA1_CTX *idxctx)
{
	uint32_t crc32tmp;
	int x;

	for (x=0;x<packfileinfo->nobjects;x++) {
		crc32tmp = htonl(index_entry[x].crc);
		sha_write(idxfd, &crc32tmp, 4, idxctx);
	}
}

inline void
write_32bit_table(int idxfd, struct packfileinfo *packfileinfo,
    struct index_entry *index_entry, SHA1_CTX *idxctx)
{
	uint64_t offsettmp;
	int x;

	for (x = 0; x < packfileinfo->nobjects; x++) {
		offsettmp = htonl(index_entry[x].offset);
		sha_write(idxfd, &offsettmp, 4, idxctx);
	}
}

inline void
write_checksums(int idxfd, struct packfileinfo *packfileinfo, SHA1_CTX *idxctx)
{
	sha_write(idxfd, packfileinfo->sha, 20, idxctx);
	SHA1_Final(packfileinfo->ctx, idxctx);
	sha_write(idxfd, packfileinfo->ctx, 20, idxctx);
}

/*
 * Builds the idx file, combines the functions above.
 */
void
pack_build_index(int idxfd, struct packfileinfo *packfileinfo,
    struct index_entry *index_entry, SHA1_CTX *idxctx)
{
	/* Write pack header */
	write_index_header(idxfd, idxctx);
	/* Writing hash count */
	write_hash_count(idxfd, index_entry, idxctx);
	/* Writing hashes */
	write_hashes(idxfd, packfileinfo, index_entry, idxctx);
	/* Write the crc32 table */
	write_crc_table(idxfd, packfileinfo, index_entry, idxctx);
	/* Write the 32-bit offset table */
	write_32bit_table(idxfd, packfileinfo, index_entry, idxctx);
	/* Currently does not write large files */
	/* Write the SHA1 checksum of the corresponding packfile */
	write_checksums(idxfd, packfileinfo, idxctx);
}

/*
 * This function reassembles an OBJ_OFS_DELTA object. It requires that the
 * objectinfo variable has already passed through pack_object_header.
 * It starts by inflating the base object. Then, it loops through each delta
 * by offset, inflates the contents and applies the patch.
 *
 * Note: This is a memory-extensive function, as it requires a copy of the base
 * object and fully patched object in memory at once and I cannot think of any
 * way to only have one object in memory at once.
 *
 * This function also calculates the crc32 value. The code is not as clean as
 * it should be, but this is the best approach I had without using a file-scope
 * variable, which is GPL git's approach.
 */

void
pack_delta_content(int packfd, struct objectinfo *objectinfo, SHA1_CTX *packctx)
{
	struct decompressed_object base_object, delta_object;
	int q;

	base_object.data = NULL;
	base_object.size = 0;
	base_object.deflated_size = 0;

	lseek(packfd, objectinfo->ofsbase, SEEK_SET);
	deflate_caller(packfd, NULL, NULL, buffer_cb, &base_object);
	objectinfo->deflated_size = base_object.deflated_size;

	for (q=objectinfo->ndeltas-1;q>=0;q--) {
		lseek(packfd, objectinfo->deltas[q], SEEK_SET);

		delta_object.data = NULL;
		delta_object.size = 0;
		delta_object.deflated_size = 0;
		/* Only calculate the crc32 for the first iteration */
		if (q == 0) {
			struct two_darg two_darg;
			two_darg.crc =  &objectinfo->crc;
			two_darg.sha = packctx;
			deflate_caller(packfd, zlib_update_crc_sha, &two_darg, buffer_cb, &delta_object);
		}
		else
			deflate_caller(packfd, NULL, NULL, buffer_cb, &delta_object);


		applypatch(&base_object, &delta_object, objectinfo);
		free(base_object.data);
		free(delta_object.data);
		base_object.data = objectinfo->data;
		base_object.size = objectinfo->isize;
	}

	/*
	 * This instance of deflated_size is the 0th in the list which
	 * means it is the deflated_size of the current OBJ_OFS_DELTA,
	 * not of the parent deltas.
	 */
	objectinfo->deflated_size = delta_object.deflated_size;

}

/* Used by index-pack to compute SHA and get offset bytes */
unsigned char *
pack_get_index_bytes_cb(unsigned char *buf, int size, int deflated_bytes, void *arg)
{
	struct index_generate_arg *index_generate_arg = arg;
	SHA1_Update(&index_generate_arg->shactx, buf, size);
	index_generate_arg->bytes += deflated_bytes;
	return (buf);
}

int
pack_get_packfile_offset(char *sha_str, char *filename)
{
	DIR *d;
	struct dirent *dir;
	char packdir[PATH_MAX];
	int packfd;
	char *file_ext;
	unsigned char *idxmap;
	struct stat sb;
	int offset = 0;
	uint8_t sha_bin[20];
	int i;

	for (i=0;i<20;i++)
		sscanf(sha_str+i*2, "%2hhx", &sha_bin[i]);

	sprintf(packdir, "%s/objects/pack", dotgitpath);
	d = opendir(packdir);

	/* Find hash in idx file or die */
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			file_ext = strrchr(dir->d_name, '.');
			if (!file_ext || strncmp(file_ext, ".idx", 4))
				continue;
			sprintf(filename, "%s/objects/pack/%s", dotgitpath, dir->d_name);

			packfd = open(filename, O_RDONLY);
			fstat(packfd, &sb);
			idxmap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, packfd, 0);

			if (idxmap == NULL) {
				fprintf(stderr, "mmap(2) error, exiting.\n");
				exit(0);
			}
			close(packfd);

			offset = pack_find_sha_offset(sha_bin, idxmap);

			munmap(idxmap, sb.st_size);

			if (offset != -1)
				break;
		}
	}

	closedir(d);

	return (offset);
}

int
pack_parse_header(int packfd, struct packfileinfo *packfileinfo, SHA1_CTX *packctx)
{
	int version;
	int nobjects;
	unsigned char buf[4];

	buf_read(packfd, buf, 4, read_sha_update, packctx);

	if (memcmp(buf, "PACK", 4)) {
		fprintf(stderr, "error: bad object header. Git repository may be corrupt.\n");
		exit(128);
	}

	buf_read(packfd, &version, 4, read_sha_update, packctx);
	packfileinfo->version = ntohl(version);
	if (packfileinfo->version != 2) {
		fprintf(stderr, "error: unsupported version: %d\n", version);
		exit(128);
	}

	buf_read(packfd, &nobjects, 4, read_sha_update, packctx);
	packfileinfo->nobjects = ntohl(nobjects);

	/* This hardcoded value is because we read 4*3 bytes from the header */
	return (12);
}

/*
 * The function pack_object_header checks the object type of the current object.
 * If the type is not deltified, it will immediately return and the variable
 * objectinfo will contain the type as objectinfo->ftype. Additionally, we will
 * have the objectinfo->isize.
 *
 * If the object is deltified, it will iteratively call object_header_ofs to
 * locate the following values:
 * A. The base object type, stored in objectinto->ftype
 * B. The base offset, stored in objectinfo->base
 * C. All delta offsets, stored in objectinfo->deltas
 * D. Number of deltas, stored in objectinfo->ndelta
 * Note: The objectinfo->isize value is NOT captured
 *
 * If the application needs the objectinfo->isize or objectinfo->data, it must
 * run pack_delta_content. This function will consumed values B-D to produce
 * the final non-deltified data.
 */

/* Starts at a new object header, not the delta */
void
object_header_ofs(int packfd, int offset, int layer,
    struct objectinfo *objectinfo, struct objectinfo *childinfo,
    SHA1_CTX *packctx)
{
	uint8_t c;
	unsigned shift;
	unsigned long used;
	lseek(packfd, offset, SEEK_SET);

	read(packfd, &c, 1);
	used = 1;
	childinfo->psize = c & 15;
	childinfo->ptype = (c >> 4) & 7;

	shift = 4;

	while (c & 0x80) {
		read(packfd, &c, 1);
		childinfo->psize += (c & 0x7f) << shift;
		shift += 7;
		used++;
	}

	if (childinfo->ptype != OBJ_OFS_DELTA) {
		objectinfo->ftype = childinfo->ptype;
		objectinfo->deltas = malloc(sizeof(unsigned long) * layer);
		objectinfo->ofsbase = offset + used;
		objectinfo->ndeltas = layer;
	}
	else {
		unsigned long delta;
		unsigned long ofshdr = 1;

		read(packfd, &c, 1);
		delta = c & 0x7f;
		while (c & 0x80) {
			delta++;
			ofshdr++;
			read(packfd, &c, 1);
			delta = (delta << 7) + (c & 0x7f);
		}
		object_header_ofs(packfd, offset - delta, layer+1, objectinfo, childinfo, packctx);
		objectinfo->deltas[layer] = offset + used + ofshdr;
	}
}

void
pack_object_header(int packfd, int offset, struct objectinfo *objectinfo,
    SHA1_CTX *packctx)
{
	uint8_t c;
	unsigned shift;

	lseek(packfd, offset, SEEK_SET);

	objectinfo->offset = offset;
	objectinfo->used = 1;

	buf_read(packfd, &c, 1, read_sha_update, packctx);

	objectinfo->crc = crc32(objectinfo->crc, &c, 1);
	objectinfo->ptype = (c >> 4) & 7;
	objectinfo->psize = c & 15;
	shift = 4;

	while (c & 0x80) { 
		buf_read(packfd, &c, 1, read_sha_update, packctx);
		objectinfo->crc = crc32(objectinfo->crc, &c, 1);
		objectinfo->psize += (c & 0x7f) << shift;
		shift += 7;
		objectinfo->used++;
	}

	if (objectinfo->ptype != OBJ_OFS_DELTA && objectinfo->ptype != OBJ_REF_DELTA) {
		objectinfo->ftype = objectinfo->ptype;
		objectinfo->ndeltas = 0;
	}
	else {
		/* We have to dig deeper */
		unsigned long delta;
		unsigned long ofshdrsize = 1;
		struct objectinfo childinfo;

		buf_read(packfd, &c, 1, read_sha_update, packctx);
		objectinfo->crc = crc32(objectinfo->crc, &c, 1);
		delta = c & 0x7f;
		
		while (c & 0x80) {
			ofshdrsize++;
			delta += 1;
			buf_read(packfd, &c, 1, read_sha_update, packctx);
			objectinfo->crc = crc32(objectinfo->crc, &c, 1);
			delta = (delta << 7) + (c & 0x7f);
		}

		objectinfo->ofshdrsize = ofshdrsize;
		object_header_ofs(packfd, offset - delta, 1, objectinfo, &childinfo, packctx);
		objectinfo->deltas[0] = offset + objectinfo->used + ofshdrsize;

	}

	return;
}

int
pack_find_sha_offset(unsigned char *sha, unsigned char *idxmap)
{
	struct fan *fans;
	struct entry *entries;
	struct checksum *checksums;
	struct offset *offsets;
	int idx_offset;
	char idx_version;
	int nelements;
	int n;

	if (memcmp(idxmap, "\xff\x74\x4f\x63", 4)) {
		fprintf(stderr, "Header signature does not match index version 2.\n");
		exit(0);
	}
	idx_offset = 4;

	idx_version = *(idxmap + idx_offset+3);
	if (idx_version != 2) {
		fprintf(stderr, "opengit currently only supports version 2. Exiting.\n");
		exit(0);
	}
	idx_offset += 4;

	// Get the fan table and capture last element
	fans = (struct fan *)(idxmap + idx_offset);
	nelements = ntohl(fans->count[255]);
	// Move to SHA entries
	idx_offset += sizeof(struct fan);
	// Point to SHA entries
	entries = (struct entry *)(idxmap + idx_offset);

	for (n=0;n<nelements;n++)
		if (!memcmp(entries[n].sha, sha, 20))
			break;

	if (n==nelements)
		return (-1);

	// Move to Checksums
	idx_offset += (sizeof(struct entry) * nelements);
	// Point to checksums
	checksums = (struct checksum *)(idxmap + idx_offset);
	// Move to Offsets
	idx_offset += (nelements * 4);
	// Capture Offsets
	offsets = (struct offset *)(idxmap + idx_offset);

	return (ntohl(offsets[n].addr));
}
