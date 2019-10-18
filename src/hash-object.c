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


#include <sys/mman.h>
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
	{"stdin", no_argument, NULL, 1},
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

/* May move this to lib/zlib-handler.[ch] */
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
	int r;
	FILE *dest;
	struct stat sb;
	char checksum[HEX_DIGEST_LENGTH];
	char tpath[PATH_MAX];
	char objpath[PATH_MAX];
	SHA1_CTX context;
	Bytef in[CHUNK];
	int flush;
	int destfd;

	z_stream strm;

	strlcpy(tpath, dotgitpath, PATH_MAX);
	strlcat(tpath, "/objects/obj.XXXXXX", PATH_MAX);

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = in;
	r = deflateInit(&strm, Z_BEST_SPEED);
	if (r != Z_OK) {
		printf("Z_OK error\n");
		return (r);
	}

	SHA1_Init(&context);

	source = fopen(filearg, "r");
	destfd = mkstemp(tpath);
	if (destfd == -1) {
		fprintf(stderr, "Unable to temporary file %s, exiting.\n", tpath);
		exit(0);
	}
	dest = fdopen(destfd, "w");
	if (dest == NULL) {
		fprintf(stderr, "Unable to fdopen() file, exiting.\n");
	}
	stat(filearg, &sb);

	strm.avail_in = snprintf((char*)in, CHUNK, "blob %ld", sb.st_size) + 1;
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

		r = add_zlib_content(&strm, dest, flush);

	} while (flush != Z_FINISH);
	assert(r == Z_STREAM_END);

	SHA1_End(&context, checksum);
	(void)deflateEnd(&strm);

	/* Build the object directory prefix */
	snprintf(objpath, PATH_MAX, "%s/objects/%c%c/", dotgitpath,
	    checksum[0], checksum[1]);
	mkdir(objpath, 0755);

	/* Add the rest of the checksum path */
	strlcat(objpath+3, checksum+2, PATH_MAX);
	rename(tpath, objpath);

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
		case 1:
			flags |= CMD_HASH_OBJECT_STDIN;
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

	if (flags & CMD_HASH_OBJECT_STDIN) {
		char data[CHUNK];
		int fsize;
		printf("stdin, currently unimplemented, exiting.\n");
		exit(0);
	}

	if (argv[1]) {
		int fd;
		char *data;
		struct stat sb;

		fd = open(argv[1], O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "Unable to open file %s, exiting.\n", argv[1]);
			exit(0);
		}

		fstat(fd, &sb);
		data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

		ret = hash_object_write(argv[1], flags);
	}

	return (ret);
}

