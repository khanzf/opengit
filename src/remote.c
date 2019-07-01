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
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib/common.h"
#include "lib/ini.h"
#include "remote.h"

static struct option long_options[] =
{
	{"verbose", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

int
remote_usage(int type)
{
	if (type == REMOTE_USAGE_DEFAULT) {
		printf("Remote usage statement\n");
		return (0);
	}
	else if (type == REMOTE_USAGE_REMOVE) {
		printf("usage: ogit remote remove <name>\n");
		return (129);
	}

	return (0);
}

int
remote_remove(int argc, char *argv[], uint8_t flags)
{
	char tmpconfig[PATH_MAX];
	char *repopath;
	struct section *cur_section = sections;
	int fd;
	int match = 0;

	if (argc != 2)
		return (remote_usage(REMOTE_USAGE_REMOVE));

	repopath = argv[1];

	while (cur_section) {
		if (cur_section->type == REMOTE && \
		    !strncmp(cur_section->repo_name, repopath, strlen(repopath))) {
			match = 1;
			break;
		}
		cur_section = cur_section->next;
	}

	if (match == 0) {
		fprintf(stderr, "fatal: No such remote: %s\n", repopath);
		return (128);
	}

	cur_section = sections;

	snprintf(tmpconfig, sizeof(tmpconfig), "%s/.config.XXXXXX", dotgitpath);
	fd = mkstemp(tmpconfig);
	if (fd == -1) {
		fprintf(stderr, "Unable to open temporary file: %s\n", tmpconfig);
		return (-1);
	}

	/* Rewrite config file */
	ini_write_config(fd, cur_section);
	close(fd);

	return (0);
}

int
remote_list(int argc, char *argv[], uint8_t flags)
{
	struct section *cur_section = sections;

	/* Usage if the default list command syntax is broken */
	if (argc > 1 && flags != OPT_VERBOSE)
	        remote_usage(REMOTE_USAGE_DEFAULT);

	while (cur_section) {
		if (cur_section->type == REMOTE) {
			if (!(flags & OPT_VERBOSE))
				printf("%s\n", cur_section->repo_name);
			else {
				printf("%s\t%s (fetch)\n",
				    cur_section->repo_name,
				    cur_section->url);
				printf("%s\t%s (push)\n",
				    cur_section->repo_name,
				    cur_section->url);
			}
		}
		cur_section = cur_section->next;
	}

	return (0);
}

int
remote_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;

	uint8_t cmd = 0;
	uint8_t flags = 0;

	argc--; argv++;

	if (argc > 1) {
		if (strncmp(argv[1], "add", 3) == 0) {
			argc--;
			argv++;
			cmd = CMD_ADD;
		}
		else if (strncmp(argv[1], "remove", 6) == 0) {
			argc--;
			argv++;
			cmd = CMD_REMOVE;
		}
	}

	while ((ch = getopt_long(argc, argv, "v", long_options, NULL)) != -1)
		switch (ch) {
		case 'v':
			flags |= OPT_VERBOSE;
			continue;
		case '?':
		default:
			remote_usage(REMOTE_USAGE_DEFAULT);
		}


	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}

	config_parser();

	switch (cmd) {
		case CMD_REMOVE:
			remote_remove(argc, argv, flags);
			break;
		case CMD_DEFAULT:
		default:
			remote_list(argc, argv, flags);
			break;
	}
	

	return (ret);
}
