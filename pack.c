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

#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "zlib_handler.h"
#include "pack.h"
#include "common.h"
#include "ini.h"

#include <netinet/in.h>

void
pack_uncompress_object(int packfd)
{
	struct writer_args writer_args;
	writer_args.fd = STDOUT_FILENO;
	writer_args.sent = 0;
	deflate_caller(packfd, write_cb, &writer_args);
}

unsigned char *
pack_deflated_bytes_cb(unsigned char *buf, int __unused size, void *arg, int deflated_bytes)
{
	int *offset = arg;
	*offset += deflated_bytes;

	return buf;
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
		while((dir = readdir(d)) != NULL) {
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

	return offset;
}

void
pack_parse_header(int packfd, struct packfilehdr *packfilehdr)
{
	int version;
	int nobjects;
	unsigned char buf[4];

	read(packfd, buf, 4);

	if (memcmp(buf, "PACK", 4)) {
		fprintf(stderr, "error: bad object header. Git repository may be corrupt.\n");
		exit(128);
	}

	read(packfd, &version, 4);
	packfilehdr->version = ntohl(version);
	if (packfilehdr->version != 2) {
		fprintf(stderr, "error: unsupported version: %d\n", version);
		exit(128);
	}

	read(packfd, &nobjects, 4);
	packfilehdr->nobjects = ntohl(nobjects);
}

void
pack_object_header(int packfd, int offset, struct objectinfo *objectinfo)
{
	unsigned long sevenbit;

	objectinfo->used = 1;

	read(packfd, &sevenbit, sizeof(unsigned long));
	objectinfo->size = sevenbit & 0x0F;

	while(sevenbit & 0x80) {
		lseek(packfd, offset + objectinfo->used, SEEK_SET);
		read(packfd, &sevenbit, sizeof(unsigned long));
		objectinfo->size += (sevenbit & 0x7F) << (4 + (7*(objectinfo->used-1)));
		objectinfo->used++;
	}

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

	for(n=0;n<nelements;n++)
		if (!memcmp(entries[n].sha, sha, 20))
			break;

	if (n==nelements)
		return -1;

	// Move to Checksums
	idx_offset += (sizeof(struct entry) * nelements);
	// Point to checksums
	checksums = (struct checksum *)(idxmap + idx_offset);
	// Move to Offsets
	idx_offset += (nelements * 4);
	// Capture Offsets
	offsets = (struct offset *)(idxmap + idx_offset);

	return ntohl(offsets[n].addr);
}
