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
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <fetch.h>
#include <zlib.h>
#include "lib/zlib-handler.h"
#include "lib/buffering.h"
#include "lib/common.h"
#include "lib/ini.h"
#include "lib/pack.h"
#include "index-pack.h"
#include "clone.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

/*
ssize_t
sha_write(int fd, const void *buf, size_t nbytes, SHA1_CTX *idxctx)
{
	SHA1_Update(idxctx, buf, nbytes);
	return write(fd, buf, nbytes);
}
*/

void
index_pack_usage(int type)
{
	fprintf(stderr, "usage: ogit index-pack [-v] [-o <index-file>] [--keep | --keep=<msg>] [--verify] [--strict] (<pack-file> | --stdin [--fix-thin] [<pack-file>])\n");
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
	int idxfd;
	struct packfilehdr packfilehdr;
	struct object_index_entry *object_index_entry;
	int offset;
	int x;
	SHA1_CTX packctx;
	SHA1_CTX idxctx;
	SHA1_Init(&packctx);
	SHA1_Init(&idxctx);

	packfd = open(argv[1], O_RDONLY);
	if (packfd == -1) {
		fprintf(stderr, "fatal: cannot open packfile '%s'\n", argv[1]);
		exit(128);
	}

	pack_parse_header(packfd, &packfilehdr, &packctx);
	offset = 4 * 3; 

	object_index_entry = malloc(sizeof(struct object_index_entry) * packfilehdr.nobjects);
	offset = pack_get_object_meta(packfd, offset, &packfilehdr, object_index_entry, &packctx, &idxctx);

	SHA1_Final(packfilehdr.sha, &packctx);
	close(packfd);

	// Now the idx file

	qsort(object_index_entry, packfilehdr.nobjects,
	    sizeof(struct object_index_entry), sortindexentry);

	idxfd = open("packout.idx", O_WRONLY | O_CREAT, 0660);
	if (idxfd == -1) {
		fprintf(stderr, "Unable to open packout.idx for writing\n");
		exit(idxfd);
	}

	// Write the header 
	pack_write_index_header(idxfd, &idxctx);

	/* Writing the Fan Table */

	/* Writing hash count */
	pack_write_hash_count(idxfd, object_index_entry, &idxctx);

	/* Writing hashes */
	for(x = 0; x < packfilehdr.nobjects; x++)
		sha_write(idxfd, object_index_entry[x].digest, 20, &idxctx);

	/* Write the crc32 table */
	uint32_t crc32tmp;
	for(x = 0; x < packfilehdr.nobjects; x++) {
		crc32tmp = htonl(object_index_entry[x].crc);
		sha_write(idxfd, &crc32tmp, 4, &idxctx);
	}

	/* Write the 32-bit offset table */
	uint64_t offsettmp;
	for(x = 0; x < packfilehdr.nobjects; x++) {
		offsettmp = htonl(object_index_entry[x].offset);
		sha_write(idxfd, &offsettmp, 4, &idxctx);
	}

	/* Currently does not write large files */

	/* Write the SHA1 checksum of the corresponding packfile */
	sha_write(idxfd, packfilehdr.sha, 20, &idxctx);
	SHA1_Final(packfilehdr.ctx, &idxctx);
	sha_write(idxfd, packfilehdr.ctx, 20, &idxctx);

	/* Output the SHA to the terminal */
	for(x=0;x<20;x++)
		printf("%02x", packfilehdr.sha[x]);
	printf("\n");

	close(idxfd);

	free(object_index_entry);
	return (ret);
}

