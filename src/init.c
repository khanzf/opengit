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

#define _WITH_DPRINTF
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "lib/common.h"
#include "lib/ini.h"
#include "init.h"

static struct option long_options[] =
{
	{"quiet",no_argument, NULL, 'q'},
	{"bare", no_argument, NULL, 0},
	{"template", required_argument, NULL, 0},
	{"separate-git-dir", required_argument, NULL, 0},
	{"shared", required_argument, NULL, 0},
	{NULL, 0, NULL, 0}
};

char *init_dirs[] = {
	".git/",
	".git/objects",
	".git/objects/pack",
	".git/objects/info",
	".git/refs",
	".git/refs/tags",
	".git/refs/heads",
	".git/branches",
	".git/hooks",
};

void
init_usage()
{
	printf("usage: ogit init [-q | --quiet] [--bare] [--template=<template-directory>] [--shared[=<permissions>]] [<directory>]\n\n");
	printf("\t--template <template-directory>\n");
	printf("\t\t\tdirectory from which templates will be used\n");
	printf("\t--bare\t\tcreate a bare repository\n");
	printf("\t--shared[=<permissions>]\n");
	printf("\t\t\tspecify that the git repository is to be shared amongst several users\n");
	printf("\t-q, --quiet\tbe quiet\n");
	printf("\t--separate-git-dir <gitdir>\n");
	printf("\t\t\tseparate git dir from working tree\n\n");
	exit(EXIT_INVALID_COMMAND);
}

/*
 * Initializes the directory
 * Does not initialize the .git/config file
 */
int
init_dirinit(char *path)
{
	struct stat sb;
	char *suffix;
	int reinit = 0;
	int fd;
	int ret;
	int x;

	/* Construct the "base" directory path */
	mkdir(path, 0755);
	if (path[0] != '\0') {
		x = strlcat(path, "/", PATH_MAX);
		suffix = path + x;
	}
	else
		suffix = path;

	/* This block checks if the file already exists. If so, sets reinit */
	for(x = 0; x < nitems(init_dirs); x++) {
		strlcpy(suffix, init_dirs[x], PATH_MAX);
		if (!stat(path, &sb))
			reinit = 1;
		else {
			ret = mkdir(path, 0755);
			if (ret == -1) {
				fprintf(stderr, "Cannot create %s\n", path);
				exit(ret);
			}
		}
		suffix[0] = '\0';
	}

	/* Create an empty 'description' file */
	strlcat(path, ".git/description", PATH_MAX);
	if (!stat(path, &sb))
		reinit = 1;
	else {
		fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
		if (fd == -1) {
			fprintf(stderr, "Cannot create description file\n");
			return (-1);
		}
		suffix[0] = '\0';
		close(fd);
	}

	strlcat(path, ".git/HEAD", PATH_MAX);
	if (!stat(path, &sb)) {
		reinit = 1;
	}
	else {
		fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd != -1) {
			write(fd, "ref: refs/heads/master\n", 23);
		}
		close(fd);
	}

	return (reinit);
}

int
init_main(int argc, char *argv[])
{
	struct section core;
	int ret;
	int reinit;
	int fd;
	int ch;
	struct stat sb;
	char *repodir = NULL;
	uint8_t flags = 0;
	char path[PATH_MAX];
	char *suffix;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "q", long_options, NULL)) != -1) {
		switch(ch) {
		case 'q':
			flags |= INIT_QUIET;
			argc--; argv++;
			break;
		default:
			init_usage();
			break;
		}
	}

	if (argc >= 2)
		repodir = argv[1];

	if (repodir)
		ret = strlcpy(path, repodir, PATH_MAX);
	else {
		path[0] = '\0';
		ret = 0;
	}
	suffix = path + ret;

	reinit = init_dirinit(path);
	suffix[0] = '\0';

	strlcat(path, ".git/config", PATH_MAX);
	if (!stat(path, &sb))
		reinit = 1;
	else {
		fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
		if (fd == -1) {
			fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
			exit(errno);
		}

		core.type = CORE;
		core.repositoryformatversion = 0;
		core.filemode = TRUE;
		core.bare = FALSE;
		core.logallrefupdates = TRUE;
		core.next = NULL;
		ini_write_config(fd, &core);
	}

	getcwd((char *)&path, PATH_MAX);

	if (!(flags & INIT_QUIET)) {
		if (reinit)
			printf("Reinitialized existing ");
		else
			printf("Initialized empty ");
		if (repodir)
			printf("Git repository in %s/%s/.git/\n", path, repodir);
		else
			printf("Git repository in %s/.git/\n", path);
	}

	return (EXIT_SUCCESS);
}
