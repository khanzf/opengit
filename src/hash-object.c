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
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include "lib/zlib-handler.h"
#include "lib/common.h"
#include "lib/ini.h"
#include "hash-object.h"

static struct option long_options[] =
{
	{"stdin", no_argument, NULL, 0},
	{"stdin-paths", no_argument, NULL, 0},
	{"no-filters", no_argument, NULL, 0},
	{"literally", no_argument, NULL, 0},
	{"path", required_argument, NULL, 0},
	{NULL, 0, NULL, 0}
};

static int
hash_object_usage(int type)
{
	fprintf(stderr, "usage: git hash-object [-t <type>] [-w] [--path=<file> | --no-filters] [--stdin] [--] <file>...\n");
	fprintf(stderr, "   or: git hash-object  --stdin-paths\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -t <type>\t\tobject type\n");
	fprintf(stderr, "    -w\t\t\twrite the object into the object database\n");
	fprintf(stderr, "    --stdin\t\tread the object from stdin\n");
	fprintf(stderr, "    --stdin-paths\tread file names from stdin\n");
	fprintf(stderr, "    --no-filters\tstore file as is without filters\n");
	fprintf(stderr, "    --literally\t\tjust hash any random garbage to create corrupt objects for debugging Git\n");
	fprintf(stderr, "    --path <file>\tprocess file as it were from this path\n");
	fprintf(stderr, "\n");
	return (0);
}

static int
hash_object_compute_checksum(FILE *source, char *checksum, char *header)
{
	SHA1_CTX context;
	char in[4096];
	int l;

	SHA1_Init(&context);
	SHA1_Update(&context, header, strlen(header)+1);

	while((l = fread(&in, 1, 4096, source)) != -1 && l != 0) {
		SHA1_Update(&context, in, l);
	}
	SHA1_End(&context, checksum);

	/* Reset the stream */
	fseek(source, 0, SEEK_SET);

	return (0);
}

static int
hash_object_create_header(char *filepath, char *header)
{
	struct stat sb;
	int l;
	stat(filepath, &sb);

	l = sprintf(header, "blob %ld", sb.st_size);

	return (l);
}

static FILE *
hash_object_create_file(char *checksum)
{
	FILE *objectfileptr;
	char objectpath[PATH_MAX];

	// First create the directory
	snprintf(objectpath, sizeof(objectpath), "%s/objects/%c%c",
	    dotgitpath, checksum[0], checksum[1]);

	mkdir(objectpath, 0755);

	// Reusing objectpath variable
	snprintf(objectpath, sizeof(objectpath), "%s/objects/%c%c/%s",
	    dotgitpath, checksum[0], checksum[1], checksum+2);

	objectfileptr = fopen(objectpath, "w");

	return (objectfileptr);
}

static int
add_zlib_content(z_stream *strm, FILE *dest, int flush)
{
	int ret;
	unsigned have;
	Bytef out[CHUNK];

	do {
		strm->avail_out = CHUNK;
		strm->next_out = out;
		ret = deflate(strm, flush);
		assert(ret != Z_STREAM_ERROR);
		have = CHUNK - strm->avail_out;
		if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
			(void)deflateEnd(strm);
			exit(Z_ERRNO);
		}

	} while(strm->avail_out == 0);
	return ret;
}

static int
hash_object_write(char *filearg, uint8_t flags)
{
	FILE *source;
	int ret;
	FILE *dest;
	struct stat sb;
	char checksum[HEX_DIGEST_LENGTH];
	SHA1_CTX context;
	Bytef in[CHUNK];
	Bytef out[CHUNK];
	unsigned have;
	int flush;

	z_stream strm;

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = in;
	ret = deflateInit(&strm, Z_BEST_SPEED);
	if (ret != Z_OK) {
		printf("Z_OK error\n");
		return (ret);
	}

	SHA1_Init(&context);

	bzero(checksum, HEX_DIGEST_LENGTH);

	source = fopen(filearg, "r");
	dest = fopen("BBBB", "w");
	stat(filearg, &sb);

	strm.avail_in = snprintf(in, CHUNK, "blob %ld", sb.st_size) + 1;
	strm.next_in = in;
	SHA1_Update(&context, in, strm.avail_in);
	add_zlib_content(&strm, dest, Z_NO_FLUSH);

	do {
		strm.avail_in = fread(in, 1, CHUNK, source);

		/* Do the SHA1 first because it will be clobbered */
		SHA1_Update(&context, in, strm.avail_in);

		if (ferror(source)) {
			(void)deflateEnd(&strm);
			return Z_ERRNO;
		}

		flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = in;

		ret = add_zlib_content(&strm, dest, flush);

	} while (flush != Z_FINISH);
	assert(ret == Z_STREAM_END);

	SHA1_End(&context, checksum);
	(void)deflateEnd(&strm);

	printf("%s\n", checksum);
	return (0);
}

int
hash_object_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;

	int8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "wt:", long_options, NULL)) != -1)
		switch(ch) {
		case 'w':
			flags |= CMD_HASH_OBJECT_WRITE; 
			q++;
			break;
		default:
			printf("Currently not implemented\n");
			hash_object_usage(0);
			return (-1);
		}
	argc = argc - q;
	argv = argv + q;

	git_repository_path();
	if (argv[1])
		ret = hash_object_write(argv[1], flags);
	else
		return (hash_object_usage(0));

	return (ret);
}

