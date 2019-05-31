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

#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>

#include "common.h"
#include "ini.h"

static regex_t re_core_header;
static regex_t re_remote_header;
static regex_t re_variable;

struct section *sections = NULL;

int
config_parser()
{
	FILE* fp;
	char line[1000];
	regmatch_t pmatch[10];
	struct section *current_section = sections;
	struct section *new_section;
	char ini_file[PATH_MAX];

	char tmp[1000];
	char tmpvar[1000];
	char *tmpval;

	current_section = NULL;

	ini_init_regex();

	sprintf(ini_file, "%s/config", dotgitpath);
	fp = fopen(ini_file, "r");
	if (!fp) {
		printf("Unable to open file: %s\n", ini_file);
		return (-1);
	}

	while (fgets(line, 1000, fp) != NULL) {
		line[strlen(line)-1] = '\0'; // chomp()

		if (regexec(&re_core_header, line, 2, pmatch, 0) != REG_NOMATCH ||
		    regexec(&re_remote_header, line, 4, pmatch, 0) != REG_NOMATCH) {
			new_section = calloc(1, sizeof(struct section));
			new_section->logallrefupdates = 0xFF;

			strncpy(tmp, line + pmatch[1].rm_so,
			    pmatch[1].rm_eo - pmatch[1].rm_so);
			if (strncmp(tmp, "core", 4) == 0) {
				new_section->type = CORE;
			}
			else if (strncmp(tmp, "remote", 6) == 0) {
				new_section->type = REMOTE;
				tmpval = malloc(pmatch[2].rm_eo - pmatch[2].rm_so);
				strncpy(tmpval, line + pmatch[2].rm_so,
				    pmatch[2].rm_eo - pmatch[2].rm_so);
				new_section->repo_name = tmpval;
			}

			new_section->next = NULL;
			if (sections == NULL) {
				sections = new_section;
				current_section = sections;
			}
			else {
				current_section->next = new_section;
				current_section = new_section;
			}

			continue;
		}
		/* Capture variables */
		else if (regexec(&re_variable, line, 3, pmatch, 0) != REG_NOMATCH) {

			if (current_section == NULL) {
				fprintf(stderr,
				    "Parsing error, file did not start with a "
				    "section header\n");
				exit(1);
			}
		
			strncpy(tmpvar,
			    line + pmatch[1].rm_so,
			    pmatch[1].rm_eo - pmatch[1].rm_so);

			tmpval = malloc(pmatch[2].rm_eo - pmatch[2].rm_so + 1);
			strncpy(tmpval,
			    line + pmatch[2].rm_so,
			    pmatch[2].rm_eo - pmatch[2].rm_so);

			tmpval[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';

			/* Matches for Core */
			if (!strncmp(tmpvar, "repositoryformatversion", 23)) {
				current_section->repositoryformatversion = atoi(tmpval);
				free(tmpval);
			}
			else if (!strncmp("filemode", tmpvar, 8)) {
				if (!strncmp(tmpval, "true", 4))
					current_section->filemode = TRUE;
				else if (!strncmp(tmpval, "false", 5))
					current_section->filemode = FALSE; 
				free(tmpval);
			}
			else if (!strncmp(tmpvar, "bare", 4)) {
				if (!strncmp(tmpval, "true", 4))
					current_section->bare = TRUE;
				else if (!strncmp(tmpval, "false", 5))
					current_section->bare = TRUE;
				free(tmpval);
			}
			else if (!strncmp("logallrefupdates", tmpvar, 16)) {
				if (!strncmp(tmpval, "true", 4))
					current_section->logallrefupdates = TRUE;
				else if (!strncmp(tmpval, "false", 5))
					current_section->logallrefupdates = FALSE;
				free(tmpval);
			}
			/* Matches for Remote */
			else if (strncmp("url", tmpvar, 3) == 0)
				current_section->url = tmpval;
			else if (strncmp("fetch", tmpvar, 5) == 0)
				current_section->fetch = tmpval;

			continue;
		}
	}

	return (0);
}

int
ini_get_config(char *repodir)
{
	int fd;
	char path[PATH_MAX];

	strncpy(path, repodir, strlen(repodir));
	strncat(path, "/.git/config", 12);
	printf("config path: %s\n", path);

	fd = open(path, O_WRONLY | O_CREAT);
	if (fd == -1) {
		fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
		exit(errno);
	}

	return (fd);

}

void
ini_write_config(int fd, struct section *sections)
{
	struct section *cur_section = sections;

	while (cur_section) {
		if (cur_section->type == CORE) {
			dprintf(fd, "[core]\n");
			if (cur_section->repositoryformatversion != 0xFF)
				dprintf(fd, "\trepositoryformatversion = %d\n",
				    cur_section->repositoryformatversion);
			if (cur_section->filemode)
				dprintf(fd, "\tfilemode = %s\n",
				    (cur_section->filemode == TRUE ? "true" : "false"));
			if (cur_section->bare)
				dprintf(fd, "\tbare = %s\n",
				    (cur_section->bare == TRUE ? "true" : "false"));
			if (cur_section->logallrefupdates != 0xFF)
				dprintf(fd, "\tlogallrefupdates = %s\n",
				    (cur_section->logallrefupdates == TRUE ? "true" : "false"));
		}
		else if (cur_section->type == REMOTE) {
			dprintf(fd, "[remote \"%s\"]\n", cur_section->repo_name);
			if (cur_section->url)
				dprintf(fd, "\turl = %s\n", cur_section->url);
			if (cur_section->fetch)
				dprintf(fd, "\tfetch = %s\n", cur_section->fetch);
		}
		else if (cur_section->type == BRANCH) {
			dprintf(fd, "[branch \"\"]\n");//, cur_section->repo_name);
			if (cur_section->remote)
				dprintf(fd, "\tremote = %s\n", cur_section->remote);
			if (cur_section->merge)
				dprintf(fd, "\tmerge = %s\n", cur_section->merge);
		}
	cur_section = cur_section->next;
	}
}

void
ini_init_regex()
{
	if (regcomp(&re_core_header, "^\\[(core)\\]", REG_EXTENDED)) {
		printf("Unable to compile regex: 1\n");
		return;
	}
	if (regcomp(&re_remote_header, "^\\[(remote) \"([a-zA-Z0-9_]+)\"\\]", REG_EXTENDED)) {
		printf("Unable to compile regex: 2\n");
		return;
	}
	if (regcomp(&re_variable, "([A-Za-z0-9_]+)[\\s ]*=[\\s ]*([A-Za-z0-9_$&+,:;=?@#|'<>.^*()%!-/]+)", REG_EXTENDED)) {
		printf("Unable to compile regex: 3\n");
	}
//		[\\t\\s ]*([A-Za-z]*)[\\t\\s ]*=[\\t\\s ]*(.*)
}
