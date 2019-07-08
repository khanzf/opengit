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

int longopt;

static struct option long_options[] =
{
	{"quiet", no_argument, NULL, 'q'},
	{"bare", no_argument, &longopt, INIT_BARE},
	{"template", required_argument, NULL, 0},
	{"separate-git-dir", required_argument, NULL, 0},
	{"shared", required_argument, NULL, 0},
	{NULL, 0, NULL, 0}
};

char *init_dirs[] = {
	"objects",
	"objects/pack",
	"objects/info",
	"refs",
	"refs/tags",
	"refs/heads",
	"branches",
	"hooks",
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
 * The 'path' variable must end in a trailing '/'
 */
int
init_dirinit(int flags)
{
	struct stat sb;
	char *suffix;
	char path[PATH_MAX];
	int reinit = 0;
	int fd;
	int ret;
	int x;

	x = strlcpy(path, dotgitpath, PATH_MAX);

	suffix = path + x;

	/* This block checks if the file already exists. If so, sets reinit */
	for(x = 0; x < nitems(init_dirs); x++) {
		strlcat(path, init_dirs[x], PATH_MAX);
		if (!stat(path, &sb))
			reinit = 1;
		else {
			ret = mkdir(path, 0755);
			if (ret == -1) {
				fprintf(stderr, "1 Cannot create %s\n", path);
				exit(ret);
			}
		}
		suffix[0] = '\0';
	}

	/* Create an empty 'description' file */
	strlcat(path, "description", PATH_MAX);
	if (!stat(path, &sb))
		reinit = 1;
	else {
		fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
		if (fd == -1) {
			fprintf(stderr, "Cannot create description file\n");
			return (-1);
		}
		close(fd);
	}
	suffix[0] = '\0';

	strlcat(path, "HEAD", PATH_MAX);
	if (!stat(path, &sb))
		reinit = 1;
	else {
		fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd != -1)
			write(fd, "ref: refs/heads/master\n", 23);
		close(fd);
	}
	suffix[0] = '\0';

	return (reinit);
}

int
init_main(int argc, char *argv[])
{
	struct section core;
	int reinit;
	int fd;
	int q = 0;
	int ch;
	struct stat sb;
	char *directory = NULL;
	uint8_t flags = 0;
	char path[PATH_MAX];

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "q", long_options, NULL)) != -1) {
		switch(ch) {
		case 'q':
			flags |= INIT_QUIET;
			break;
		case 0:
			flags |= INIT_BARE;
			break;
		default:
			init_usage();
			break;
		}
		q++;
	}

	argc = argc - q;
	argv = argv + q;

	if (argc >= 2)
		directory = argv[1];

	if (directory) {
		strlcpy(repodir, directory, PATH_MAX);
		strlcat(repodir, "/", PATH_MAX);
		if (stat(repodir, &sb)) {
			if (mkdir(repodir, 0755)) {
				fprintf(stderr, "1 Unable to create %s\n", repodir);
				exit(EXIT_FAILURE);
			}
		}
	}
	else
		repodir[0] = '\0';

	/* Create .git or a bare repo directory */
	if (!(flags & INIT_BARE)) {
		strncpy(dotgitpath, repodir, PATH_MAX);
		strncat(dotgitpath, ".git/", PATH_MAX);
		if (stat(dotgitpath, &sb)) {
			if (mkdir(dotgitpath, 0755)) {
				fprintf(stderr, "2 Unable to create %s\n", dotgitpath);
				exit(EXIT_FAILURE);
			}
		}
		else
			reinit = 1;
	}
	else
		strlcat(dotgitpath, repodir, PATH_MAX);


	reinit = init_dirinit(flags);

	strlcpy(path, dotgitpath, PATH_MAX);
	strlcat(path, "config", PATH_MAX);
	if (!stat(path, &sb))
		reinit = 1;
	else {
		fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
		if (fd == -1) {
			fprintf(stderr, "Unable to open %s: %s\n", repodir, strerror(errno));
			exit(errno);
		}

		core.type = CORE;
		core.repositoryformatversion = 0;
		core.filemode = TRUE;
		if (flags & INIT_BARE)
			core.bare = TRUE;
		else
			core.bare = FALSE;
		core.logallrefupdates = TRUE;
		core.next = NULL;
		ini_write_config(fd, &core);
	}

	getcwd((char *)&path, PATH_MAX);

	/* Construct the trailing message */
	if (!(flags & INIT_QUIET)) {
		if (reinit)
			printf("Reinitialized existing ");
		else
			printf("Initialized empty ");
		printf("Git repository in %s", path);
		if (!(flags & INIT_BARE))
			printf("/.git/\n");
		else
			printf("/\n");
	}

	return (EXIT_SUCCESS);
}
