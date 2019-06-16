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
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <fcntl.h>
#include <zlib.h>
#include "lib/zlib-handler.h"
#include "lib/buffering.h"
#include "lib/common.h"
#include "lib/loose.h"
#include "lib/pack.h"
#include "lib/ini.h"
#include "cat-file.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

int
cat_file_usage(int type)
{
	fprintf(stderr, "usage: git cat-file (-t [--allow-unknown-type] | -s [--allow-unknown-type] | -e | -p | <type> | --textconv | --filters) [--path=<path>] <object>\n");
	fprintf(stderr, "\n");
	exit(type);
}

/* Just print object type */
void
cat_file_print_type_by_id(int object_type)
{
	if (object_type <= 0 || object_type >= 8) {
		fprintf(stderr, "Unknown type, exiting.\n");
		exit(128);
	}
	printf("%s\n", object_name[object_type]);
}

/* Handles loose object the callback, used by cat_file_get_content */
unsigned char *
cat_loose_object_cb(unsigned char *buf, int size, int __unused deflated_bytes, void *arg)
{
	struct loosearg *loosearg = arg;
	int hdr_offset;

	if (loosearg->step == 0) {
		loosearg->step++;
		hdr_offset = loose_get_headers(buf, size, arg);
		switch(loosearg->cmd) {
		case CAT_FILE_TYPE:
			loose_get_headers(buf, size, loosearg);
			cat_file_print_type_by_id(loosearg->type);
			return NULL;
		case CAT_FILE_SIZE:
			printf("%lu\n", loosearg->size);
			return NULL;
		case CAT_FILE_PRINT:
			size -= hdr_offset;
			buf += hdr_offset;
			break;
		}
	}

	write(loosearg->fd, buf, size);
	return buf;
}

/* Print out content of pack objects */
void
print_content(int packfd, struct objectinfo *objectinfo)
{
	if (objectinfo->ptype != OBJ_OFS_DELTA) {
		struct writer_args writer_args;
		writer_args.fd = STDOUT_FILENO;
		writer_args.sent = 0;
		lseek(packfd, objectinfo->offset + objectinfo->used, SEEK_SET);
		deflate_caller(packfd, NULL, NULL, write_cb, &writer_args);
	}
	else {
		pack_delta_content(packfd, objectinfo, NULL);
		write(STDOUT_FILENO, objectinfo->data, objectinfo->isize);
		free(objectinfo->data);
		free(objectinfo->deltas);
	}
}

/* Print out the size of pack objects */
void
print_size(int packfd, struct objectinfo *objectinfo)
{
	if (objectinfo->ptype != OBJ_OFS_DELTA) {
		struct decompressed_object decompressed_object;
		decompressed_object.size = 0;
		decompressed_object.data = NULL;
		lseek(packfd, objectinfo->offset + objectinfo->used, SEEK_SET);
		deflate_caller(packfd, NULL, NULL, buffer_cb, &decompressed_object);
		printf("%lu\n", decompressed_object.size);
	}
	else {
		pack_delta_content(packfd, objectinfo, NULL);
		printf("%lu\n", objectinfo->isize);
		free(objectinfo->data);
		free(objectinfo->deltas);
	}
}

/* Used by pack_content_handler to output packfile by flags */
void
cat_file_pack_handler(int packfd, struct objectinfo *objectinfo, void *pargs)
{
	struct loosearg *loosearg = pargs;

	switch(loosearg->cmd) {
		case CAT_FILE_PRINT:
			print_content(packfd, objectinfo);
			break;
		case CAT_FILE_SIZE:
			print_size(packfd, objectinfo);
			break;
		case CAT_FILE_TYPE:
			cat_file_print_type_by_id(objectinfo->ftype);
			break;
	}
}

/*
 * This will first attempt to locate a loose object.
 * If not found, it will search through the pack files.
 */
void
cat_file_get_content(char *sha_str, uint8_t flags)
{
	struct loosearg loosearg;

	loosearg.fd = STDOUT_FILENO;
	loosearg.cmd = flags;
	loosearg.step = 0;
	loosearg.sent = 0;

	CONTENT_HANDLER(sha_str, cat_loose_object_cb, cat_file_pack_handler, &loosearg);
}

int
cat_file_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	char *sha_str;
	uint8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "p:t:s:", long_options, NULL)) != -1)
		switch(ch) {
		case 'p':
			argc--;
			argv++;
			sha_str = argv[1];
			flags = CAT_FILE_PRINT;
			break;
		case 't':
			argc--;
			argv++;
			sha_str = argv[1];
			flags = CAT_FILE_TYPE;
			break;
		case 's':
			argc--;
			argv++;
			sha_str = argv[1];
			flags = CAT_FILE_SIZE;
			break;
		default:
			printf("cat-file: Currently not implemented\n");
			cat_file_usage(0);
		}

	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}

	switch(flags) {
		case CAT_FILE_PRINT:
		case CAT_FILE_TYPE:
		case CAT_FILE_SIZE:
			cat_file_get_content(sha_str, flags);
	}

	return (ret);
}

