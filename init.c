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
#include <fcntl.h>
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

int
init_usage(int type)
{
	return 0;
}

int
init_dirinit(char *project_directory)
{
	struct stat sb;
	struct section new_config;
	char path[PATH_MAX];
	char *subpath;
	int fd;
	int ret;
	int x;
	int dirlen;

	char *dirs[] = {
		    ".git/objects",
		    ".git/objects/pack",
		    ".git/objects/info",
		    ".git/refs",
		    ".git/refs/tags",
		    ".git/refs/heads",
		    ".git/branches"};

	/* Construct the "base" directory path */
	if (project_directory) {
		mkdir(project_directory, 0755);
		dirlen = strlen(project_directory);
		strncpy(path, project_directory, PATH_MAX);
		subpath = path + dirlen;
		if (project_directory[dirlen-1] != '/' && dirlen < PATH_MAX) {
			dirlen++;
			strncat(path, "/", PATH_MAX);
			subpath++;
		}
	}
	else
		dirlen = 0;
	subpath = path + dirlen;

	for(x = 0; x < 7; x++) {
		strncpy(subpath, dirs[x], PATH_MAX-dirlen);
		fd = open(path, O_WRONLY | O_CREAT);
		if (fd != -1) {
			fprintf(stderr, "File or directory %s already exists\n", path);
			return (-1);
		}
	}

	new_config.type = CORE;
	new_config.repositoryformatversion = 0;
	new_config.filemode = TRUE;
	new_config.bare = TRUE;
	new_config.logallrefupdates = TRUE;


	strncpy(subpath, ".git", PATH_MAX-dirlen);
	mkdir(path, 0755);

	strncpy(subpath, ".git/config", PATH_MAX-dirlen);
	fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dprintf(fd, "[core]\n");
	dprintf(fd, "\trepositoryformatversion = 0\n");
	dprintf(fd, "\tfilemode = true\n");
	dprintf(fd, "\tbare = false\n");
	dprintf(fd, "\tlogallrefupdates = true\n");
	close(fd);

	for(x = 0; x < 7; x++) {
		strncpy(subpath, dirs[x], PATH_MAX-dirlen);
		ret = mkdir(path, 0755);
		if (ret == -1) {
			fprintf(stderr, "Cannot create %s\n", path);
			return(ret);
		}
	}

	strncpy(subpath, ".git/description", PATH_MAX-dirlen);
	fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if (fd == -1 || fstat(fd, &sb)) {
		fprintf(stderr, "Cannot create description file\n");
		return -1;
	}
	close(fd);

	strncpy(subpath, ".git/HEAD", PATH_MAX-dirlen);
	fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd != -1 && fstat(fd, &sb) == 0) {
		write(fd, "ref: refs/heads/master\x0a", 23); 
	}
	close(fd);

	return 0;
}

int
init_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	char *project_directory = NULL;
	uint8_t flags = 0;
	char path[PATH_MAX];

	argc--; argv++;

	if (argc >= 2)
		project_directory = argv[1];

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
		switch(ch) {
		default:
			flags = 0;
			init_usage(-1);
		}
	}

	ret = init_dirinit(project_directory);
	if (!ret) {
		if (project_directory) {
			strncpy(path, project_directory, strlen(project_directory));
			if (project_directory[strlen(project_directory)-1] == '/')
				path[strlen(project_directory)-1] = '\0';
		}
		else {
			getcwd((char *)&path, PATH_MAX);
		}
		printf("Initialized empty Git repository in %s/.git/\n", path);
	}

	return (ret);
}
