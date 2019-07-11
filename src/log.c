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
#include "lib/zlib-handler.h"
#include "lib/common.h"
#include "lib/pack.h"
#include "lib/ini.h"
#include "log.h"
#include "ogit.h"

static int limit = -1;

static struct option long_options[] =
{
	{"color", optional_argument, NULL, 'c'},
	{"limit", required_argument, NULL, 'l'},
	{NULL, 0, NULL, 0}
};

void
log_usage(int type)
{
	fprintf(stderr, "usage: git log [<options>] [<revision-range>] [[--] <path>...]\n");
	fprintf(stderr, "   or: git show [<options>] <object>...\n\n");
	fprintf(stderr, "    -q, --quiet           suppress diff output\n");
	fprintf(stderr, "    --source              show source\n");
	fprintf(stderr, "    --use-mailmap         Use mail map file\n");
	fprintf(stderr, "    --decorate-refs <pattern>\n");
	fprintf(stderr, "                          only decorate refs that match <pattern>\n");
	fprintf(stderr, "    --decorate-refs-exclude <pattern>\n");
	fprintf(stderr, "                          do not decorate refs that match <pattern>\n");
	fprintf(stderr, "    --decorate[=...]      decorate options\n");
	fprintf(stderr, "    -L <n,m:file>         Process line range n,m in file, counting from 1\n\n");
	exit(128);
}

void
log_print_commit_headers(struct commitcontent *commitcontent)
{
	char datestr[50];

	ctime_r(&commitcontent->author_time, datestr);
	datestr[strlen(datestr)-1] = '\0';

	printf("%scommit %s%s\n", color ? "\e[0;33m" : "", commitcontent->commitsha,
	    color ? "\e[0m" : "");

	printf("Author:\t%s <%s>\n", commitcontent->author_name, commitcontent->author_email);
	printf("Date:\t%s %s\n\n", datestr, commitcontent->author_tz);
}

void
log_print_message(struct commitcontent *commitcontent)
{
	for(int x=0;x<commitcontent->lines;x++)
		printf("    %s\n", commitcontent->message[x]);
}

void
log_get_start_sha(struct logarg *logarg)
{
	int headfd;
	char headfile[PATH_MAX];
	char refpath[PATH_MAX];
	char ref[HASH_SIZE];
	int l;

	snprintf(headfile, sizeof(headfile), "%s/HEAD", dotgitpath);
	headfd = open(headfile, O_RDONLY);
	if (headfd == -1) {
		fprintf(stderr, "Error, no HEAD file found. This may not be a git directory\n");
		exit(128);
	}

	read(headfd, ref, 5);
	if (strncmp(ref, "ref: ", 5) == 0) {
		ref[6] = '\0';
		l = read(headfd, ref, PATH_MAX) - 1;
		if (ref[l] == '\n')
			ref[l] = '\0';
		snprintf(refpath, sizeof(refpath), "%s/%s", dotgitpath, ref);
	}
	else {
		read(headfd, ref + 5, HASH_SIZE - 5);
		strlcpy(logarg->sha, ref, sizeof(logarg->sha));
		close(headfd);
		return;
	}
	close(headfd);

	headfd = open(refpath, O_RDONLY);
	if(headfd == -1) {
		fprintf(stderr, "failed to open %s\n", refpath);
		exit(128);
	}

	read(headfd, logarg->sha, HASH_SIZE);
	close(headfd);
}

void
log_display_commits()
{
	struct decompressed_object decompressed_object;
	struct commitcontent commitcontent;
	struct logarg logarg;
	int source;
	int read;
	int hdr_offset = 0;

	bzero(&logarg, sizeof(struct logarg));
	log_get_start_sha(&logarg);

	logarg.status = LOG_STATUS_PARENT;
	read = 0;

	bzero(&commitcontent, sizeof(struct commitcontent));

	while(logarg.status & LOG_STATUS_PARENT) {
		decompressed_object.size = 0;
		decompressed_object.data = NULL;

		commitcontent.commitsha = logarg.sha;
		/*
		 * Expansion of the CONTENT_HANDLER macro to capture the source.
		 * This will be added to the macro if additional use-cases arise.
		 */
		if (loose_content_handler(commitcontent.commitsha, buffer_cb, &decompressed_object)) {
			source = SOURCE_PACK;
			pack_content_handler(commitcontent.commitsha, pack_buffer_cb, &decompressed_object);
		}
		else
			source = SOURCE_LOOSE;

		if (source == SOURCE_LOOSE) {
			// XXX This is an unnecessary variable, loose_get_headers's does two things
			// Better approach would be to calculate the hdr_offset from the type and size
			struct loosearg loosearg;
			hdr_offset = loose_get_headers(decompressed_object.data,
			    decompressed_object.size, &loosearg);
		}
		else
			hdr_offset = 0;
		parse_commitcontent(&commitcontent,
		    (char *)decompressed_object.data + hdr_offset,
		    decompressed_object.size - hdr_offset);

		log_print_commit_headers(&commitcontent);
		log_print_message(&commitcontent);
		if (commitcontent.numparent == 0) {
			logarg.status &= ~LOG_STATUS_PARENT;
			break;
		}

		strlcpy(commitcontent.commitsha, commitcontent.parent[0], HASH_SIZE+1);
		free_commitcontent(&commitcontent);
		free(decompressed_object.data);
	}
	exit(0);
}

int
log_main(int argc, char *argv[])
{
	int ret = 0;
	int ch, prevch;
	int q = 0;

	argc--; argv++;

	prevch = '\0';
	while((ch = getopt_long(argc, argv, "0123456789c::", long_options, NULL)) != -1) {
		switch(ch) {
		case 0:
			break;
		case 1:
			break;
		case 'c':
			parse_color_opt(optarg);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			switch (prevch) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				limit = limit * 10 + (ch - '0');
				break;
			default:
				limit = ch - '0';
				break;
			}

			break;
		default:
			printf("Currently not implemented\n");
			return (-1);
		}

		prevch = ch;
	}
	argc = argc - q;
	argv = argv + q;

	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}
	config_parser();

	log_display_commits();

	return (ret);
}

