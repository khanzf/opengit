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
#include "lib/common.h"
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

void
cat_file_get_content(char *sha_str, uint8_t flags)
{
	// Check if the loose file exists. If not, check the pack files
	if (cat_file_get_content_loose(sha_str, flags) == 0)
		cat_file_get_content_pack(sha_str, flags);
}

void
cat_file_print_type_by_id(int object_type)
{
	if (object_type <= 0 || object_type >= 8) {
		fprintf(stderr, "Unknown type, exiting.\n");
		exit(128);
	}
	printf("%s\n", object_name[object_type]);
}

int
cat_file_get_loose_headers(unsigned char *buf, int size, void *arg)
{
	struct loosearg *loosearg = arg;
	unsigned char *endptr;
	int hdr_offset = 0;

	// Is there a cleaner way to do this?
	if (!memcmp(buf, "commit ", 7)) {
		loosearg->type = OBJ_COMMIT;
		loosearg->size = strtol((char *)buf + 7, (char **)&endptr, 10);
		hdr_offset = 8 + (endptr - (buf+7));
	}
	else if (!memcmp(buf, "tree ", 5)) {
		loosearg->type = OBJ_TREE;
		loosearg->size = strtol((char *)buf + 5, (char **)&endptr, 10);
		hdr_offset = 6 + (endptr - (buf+5));
	}
	else if (!memcmp(buf, "blob ", 5)) {
		loosearg->type = OBJ_BLOB;
		loosearg->size = strtol((char *)buf + 5, (char **)&endptr, 10);
		hdr_offset = 6 + (endptr - buf);
	}
	else if (!memcmp(buf, "tag", 3)) {
		loosearg->type = OBJ_TAG;
		hdr_offset = 3;
	}
	else if (!memcmp(buf, "obj_ofs_delta", 13)) {
		loosearg->type = OBJ_REF_DELTA;
		hdr_offset = 15;
	}
	else if (!memcmp(buf, "obj_ref_delta", 13)) {
		loosearg->type = OBJ_REF_DELTA;
		hdr_offset = 15;
	}

	return hdr_offset;
}

unsigned char *
cat_loose_object_cb(unsigned char *buf, int size, int __unused deflated_bytes, void *arg)
{
	struct loosearg *loosearg = arg;
	int hdr_offset;

	if (loosearg->step == 0) {
		loosearg->step++;
		hdr_offset = cat_file_get_loose_headers(buf, size, arg);
		switch(loosearg->cmd) {
		case CAT_FILE_TYPE:
			cat_file_get_loose_headers(buf, size, arg);
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

int
cat_file_get_content_loose(char *sha_str, uint8_t flags)
{
	char objectpath[PATH_MAX];
	int objectfd;
	struct loosearg loosearg;

	sprintf(objectpath, "%s/objects/%c%c/%s", dotgitpath, sha_str[0], sha_str[1], sha_str+2);
	objectfd = open(objectpath, O_RDONLY);
	if (objectfd == -1)
		return 0;

	loosearg.fd = STDOUT_FILENO;
	loosearg.cmd = flags;
	loosearg.step = 0;
	loosearg.sent = 0;

	deflate_caller(objectfd, NULL, NULL, cat_loose_object_cb, &loosearg);

	return 1;
}

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
		pack_delta_content(packfd, objectinfo);
		write(STDOUT_FILENO, objectinfo->data, objectinfo->isize);
		free(objectinfo->data);
		free(objectinfo->deltas);
	}
}

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
		pack_delta_content(packfd, objectinfo);
		printf("%lu\n", objectinfo->isize);
		free(objectinfo->data);
		free(objectinfo->deltas);
	}
}

void
cat_file_get_content_pack(char *sha_str, uint8_t flags)
{
	char filename[PATH_MAX];
	unsigned long offset;
	int packfd;
	struct packfilehdr packfilehdr;
	struct objectinfo objectinfo;

	offset = pack_get_packfile_offset(sha_str, filename);
	if (flags == CAT_FILE_EXIT) {
		if (offset == -1)
			exit(1);
		else
			exit(0);
	}

	if (offset == -1) {
		fprintf(stderr, "fatal: git cat-file: could not get object info\n");
		exit(128);
	}

	strncpy(filename+strlen(filename)-4, ".pack", 6);
	packfd = open(filename, O_RDONLY);
	if (packfd == -1) {
		fprintf(stderr, "fatal: git cat-file: could not get object info\n");
		fprintf(stderr, "This The git repository may be corrupt.\n");
		exit(128);
	}
	pack_parse_header(packfd, &packfilehdr);

	lseek(packfd, offset, SEEK_SET);
	pack_object_header(packfd, offset, &objectinfo);

	// XXX This if-condition might not be necessary
	if (objectinfo.ftype == OBJ_OFS_DELTA) {
		int c;
		unsigned long r = 0;

		// Reads header of read ofs delta
		do {
			read(packfd, &c, 1);
			r |= c & 0x7f;
			if (!(c & BIT(7)))
				break;
			r++;
			r <<= 7;
		} while(c & BIT(7));

		lseek(packfd, offset - r, SEEK_SET);
	}
	else if (objectinfo.ftype == OBJ_REF_DELTA) {
		printf("obj_ref_delta --- supposed to apply patch\n");
	}

	switch(flags) {
		case CAT_FILE_PRINT:
			print_content(packfd, &objectinfo);
			break;
		case CAT_FILE_SIZE:
			print_size(packfd, &objectinfo);
			break;
		case CAT_FILE_TYPE:
			cat_file_print_type_by_id(objectinfo.ftype);
			break;
	}

	close(packfd);
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

