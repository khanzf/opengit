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
#include "lib/common.h"
#include "lib/index.h"
#include "lib/ini.h"

static struct option long_options[] =
{
	{"add", no_argument, NULL, 0},
	{"cacheinfo", required_argument, NULL, 1},
	{NULL, 0, NULL, 0}
};

int
update_index_usage(int type)
{
	fprintf(stderr, "usage: git update-index [<options>] [--] [<file>...]\n");
	return 0;
}

int
update_index_open_index(FILE **indexptr)
{
	char indexpath[PATH_MAX];

	snprintf(indexpath, sizeof(indexpath), "%s/index", dotgitpath);
	printf("File: %s\n", indexpath);

	*indexptr = fopen(indexpath, "rw");

	return 0;
}

int
update_index_parse(FILE **indexptr)
{
	unsigned char *indexmap;
	struct stat sb;

	fstat(fileno(*indexptr), &sb);

	indexmap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fileno(*indexptr), 0);
	if (indexmap == MAP_FAILED) {
		fprintf(stderr, "mmap failed on index file\n");
		exit(1);
	}

	parse_index(indexmap, sb.st_size);
	return 0;
}

int
update_index_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, ":", long_options, NULL)) != -1)
		switch(ch) {
		case 0:
			printf("0\n");
			break;
		case 1:
			printf("1\n");
			break;
		default:
			printf("Currently not implemented\n");
			return -1;
		}
	argc = argc - q;
	argv = argv + q;

	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}
	config_parser();

	FILE *indexptr;

	printf("update_index\n");
	update_index_open_index(&indexptr);
	update_index_parse(&indexptr);

	return (ret);
}

