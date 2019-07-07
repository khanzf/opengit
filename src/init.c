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
	".git/objects",
	".git/objects/pack",
	".git/objects/info",
	".git/refs",
	".git/refs/tags",
	".git/refs/heads",
	".git/branches",
	".git/hooks",
};

int
init_usage(int type)
{
	return (0);
}

/*
 * Initializes the directory
 * Does not initialize the .git/config file
 */
int
init_dirinit(char *repodir)
{
	struct stat sb;
	char path[PATH_MAX];
	char *subpath;
	int fd;
	int ret;
	int x;
	int dirlen;

	/* Construct the "base" directory path */
	if (repodir) {
		mkdir(repodir, 0755);
		dirlen = strlen(repodir);
		strlcpy(path, repodir, PATH_MAX);
		subpath = path + dirlen;
		if (repodir[dirlen-1] != '/' && dirlen < PATH_MAX) {
			dirlen++;
			strlcat(path, "/", PATH_MAX);
			subpath++;
		}
	}
	else
		dirlen = 0;
	subpath = path + dirlen;

	for(x = 0; x < nitems(init_dirs); x++) {
		strlcpy(subpath, init_dirs[x], PATH_MAX-dirlen);
		fd = open(path, O_WRONLY | O_CREAT);
		if (fd != -1) {
			fprintf(stderr, "File or directory %s already exists\n", path);
			return (-1);
		}
	}

	strlcpy(subpath, ".git", PATH_MAX-dirlen);
	mkdir(path, 0755);

	for(x = 0; x < nitems(init_dirs); x++) {
		strlcpy(subpath, init_dirs[x], PATH_MAX-dirlen);
		ret = mkdir(path, 0755);
		if (ret == -1) {
			fprintf(stderr, "Cannot create %s\n", path);
			return(ret);
		}
	}

	strlcpy(subpath, ".git/description", PATH_MAX-dirlen);
	fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if (fd == -1 || fstat(fd, &sb)) {
		fprintf(stderr, "Cannot create description file\n");
		return (-1);
	}
	close(fd);

	strlcpy(subpath, ".git/HEAD", PATH_MAX-dirlen);
	fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd != -1 && fstat(fd, &sb) == 0) {
		write(fd, "ref: refs/heads/master\n", 23);
	}
	close(fd);

	return (0);
}

int
init_main(int argc, char *argv[])
{
	struct section core;
	int ret = 0;
	int fd;
	int ch;
	char *repodir = NULL;
	uint8_t flags = 0;
	char path[PATH_MAX];

	argc--; argv++;

	if (argc >= 2)
		repodir = argv[1];

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
		switch(ch) {
		default:
			flags = 0;
			init_usage(-1);
		}
	}

	ret = init_dirinit(repodir);
	strlcpy(path, ".git/config", 12);
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


	if (!ret) {
		if (repodir) {
			strlcpy(path, repodir, PATH_MAX);
			if (repodir[strlen(repodir)-1] == '/')
				path[strlen(repodir)-1] = '\0';
		}
		else {
			getcwd((char *)&path, PATH_MAX);
		}
		printf("Initialized empty Git repository in %s/.git/\n", path);
	}


	return (ret);
}
