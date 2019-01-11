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

#include <sys/param.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <fetch.h>
#include "lib/zlib_handler.h"
#include "lib/common.h"
#include "lib/ini.h"
#include "lib/pack.h"
#include "clone.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

void
index_pack_usage(int type)
{
	fprintf(stderr, "usage: git index-pack [-v] [-o <index-file>] [--keep | --keep=<msg>] [--verify] [--strict] (<pack-file> | --stdin [--fix-thin] [<pack-file>])\n");
	exit(128);
}

int
index_pack_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1)
		switch(ch) {
		case 0:
			break;
		case 1:
			break;
		default:
			printf("Currently not implemented\n");
			return -1;
		}
	argc = argc - q;
	argv = argv + q;

	if (strncmp(".pack", argv[1] + strlen(argv[1])-5, 5) != 0) {
		fprintf(stderr, "fatal: packfile name '%s' does not end with '.pack'\n", argv[1]);
		exit(128);
	}

	int packfd;
	struct packfilehdr packfilehdr;
	int offset;
	struct objectinfo objectinfo;
	int x;

	packfd = open(argv[1], O_RDONLY);
	if (packfd == -1) {
		fprintf(stderr, "fatal: cannot open packfile '%s'\n", argv[1]);
		exit(128);
	}
	pack_parse_header(packfd, &packfilehdr);
	offset = 4 * 3; 

	struct object_index_entry *object_index_entry;
	struct index_generate_arg index_generate_arg;
	off_t base_offset;
	unsigned char p;
	char hdr[32];
	int hdrlen;

	object_index_entry = malloc(sizeof(struct object_index_entry) * packfilehdr.nobjects);

	/* Same as GNU git's parse_pack_objects, first pass */
	for(x = 0; x < packfilehdr.nobjects; x++) {

		lseek(packfd, offset, SEEK_SET);
		pack_object_header(packfd, offset, &objectinfo);

		offset += objectinfo.used;
		lseek(packfd, offset, SEEK_SET);

		switch(objectinfo.ftype) {
		case OBJ_REF_DELTA:
			lseek(packfd, 2, SEEK_CUR);
			read(packfd, object_index_entry[x].sha, 20);
			object_index_entry[x].sha[40] = '\0';
			offset += 22; /* 20 bytes + 2 for the header */
			break;
		case OBJ_OFS_DELTA:
			bzero(object_index_entry[x].sha, 41);
			read(packfd, &p, 1);
			offset += 1;

			base_offset = p & 127;
			while(p & 128) {
				base_offset += 1;
				read(packfd, &p, 1);
				offset += 1;

				base_offset = (base_offset << 7) + (p & 127);
			}

			SHA1_Init(&index_generate_arg.shactx);
			index_generate_arg.bytes = 0;
			deflate_caller(packfd, pack_get_index_bytes_cb, &index_generate_arg);
			offset += index_generate_arg.bytes;
			break;
		case OBJ_COMMIT:
		case OBJ_TREE:
		case OBJ_BLOB:
		case OBJ_TAG:
		default:
			index_generate_arg.bytes = 0;
			SHA1_Init(&index_generate_arg.shactx);

			hdrlen = sprintf(hdr, "%s %lu", object_name[objectinfo.ftype],
			    objectinfo.psize) + 1;
			SHA_Update(&index_generate_arg.shactx, hdr, hdrlen);
			deflate_caller(packfd, pack_get_index_bytes_cb, &index_generate_arg);
			object_index_entry[x].offset = index_generate_arg.bytes;

			SHA1_End(&index_generate_arg.shactx, object_index_entry[x].sha);

			offset += index_generate_arg.bytes;
			break;
		}
	}

	free(object_index_entry);
	close(packfd);

	return (ret);
}

