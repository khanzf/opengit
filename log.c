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

static struct option long_options[] =
{
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
log_print_commit_headers(struct logarg *logarg)
{
	char *token, *tmp;
	char *tofree;
	char *datestr;
	char author[255];
	long t;

	tofree = tmp = strdup(logarg->headers);
	printf("\e[0;33mcommit %s\e[0m\n", logarg->sha);

	while((token = strsep(&tmp, "\n")) != NULL) {
		if (strncmp(token, "parent ", 7) == 0) {
			strncpy(logarg->sha, token+7, 40);
			logarg->status |= LOG_STATUS_PARENT;
		}
		else if (strncmp(token, "author ", 7) == 0) {
			t = strtol(token + 38, NULL, 10);
			datestr = ctime(&t);
			datestr[24] = '\0';
			strncpy(author, token+7, strlen(token)-23);
			printf("Author:\t%s\n", author);
			printf("Date:\t%s %s\n", datestr, token + strlen(token)-5);
			logarg->status |= LOG_STATUS_AUTHOR;
		}
	}

	free(tmp);
	free(tofree);

}

unsigned char *
log_display_cb(unsigned char *buf, int size, int __unused deflated_bytes, void *arg)
{
	char *content;
	struct logarg *logarg = arg;
	int offset = 0;

	if (logarg->status == LOG_STATUS_COMMIT) {
		logarg->status = 1;
		offset = 11;
	}

	if (logarg->status == LOG_STATUS_HEADERS) {
		// Added content to headers
		logarg->headers = realloc(logarg->headers, logarg->size + size - offset);
		strncpy(logarg->headers + logarg->size, (char *)buf + offset, size - offset);

		content = strstr(logarg->headers, "\n\n");
		if (content != NULL) {
			logarg->status = 2; // Found
			content = content + 2;

			log_print_commit_headers(logarg);
			putchar('\n');
			printf("%s", content);
		}
	}
	else
		printf("%s", buf);

	return NULL;
}

void
log_get_start_sha(struct logarg *logarg)
{
	int headfd;
	char headfile[PATH_MAX];
	char refpath[PATH_MAX];
	char ref[PATH_MAX];
	int l;

	sprintf(headfile, "%s/HEAD", dotgitpath);
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
		sprintf(refpath, "%s/%s", dotgitpath, ref);
	}
	else {
		fprintf(stderr, "Currently not supporting the raw hash in HEAD\n");
		exit(128);
	}
	close(headfd);

	headfd = open(refpath, O_RDONLY);
	if(headfd == -1) {
		fprintf(stderr, "failed to open %s\n", refpath);
		exit(128);
	}

	read(headfd, logarg->sha, 40);
}

int
log_get_loose_object(struct logarg *logarg)
{
	int objectfd;
	char objectpath[PATH_MAX];

	sprintf(objectpath, "%s/objects/%c%c/%s",
	    dotgitpath, logarg->sha[0], logarg->sha[1],
	    logarg->sha+2);
	objectfd = open(objectpath, O_RDONLY);
	if (objectfd == -1) {
		close(objectfd);
		return 0;
	}
	deflate_caller(objectfd, NULL, NULL, log_display_cb, logarg);
	close(objectfd);

	return 1;
}

int
log_get_pack_object(struct logarg *logarg)
{
	char filename[PATH_MAX];
	int offset;
	int packfd;
	struct packfilehdr packfilehdr;
	struct objectinfo objectinfo;

	offset = pack_get_packfile_offset(logarg->sha, filename);
	if (offset == -1)
		return 0;


	strncpy(filename+strlen(filename)-4, ".pack", 6);
	packfd = open(filename, O_RDONLY);
	if (packfd == -1) {
		fprintf(stderr, "fatal: git log: could not get object info\n");
		exit(128);
	}
	pack_parse_header(packfd, &packfilehdr);

	lseek(packfd, offset, SEEK_SET);
	pack_object_header(packfd, offset, &objectinfo);

	lseek(packfd, offset + objectinfo.used, SEEK_SET);
	deflate_caller(packfd, NULL, NULL, log_display_cb, logarg);
	close(packfd);

	return 1;
}

void
log_display_commits()
{
	struct logarg logarg;

	bzero(&logarg, sizeof(struct logarg));
	log_get_start_sha(&logarg);

	logarg.status = LOG_STATUS_PARENT;
	while(logarg.status & LOG_STATUS_PARENT) {
		logarg.status = 0;
		if (!log_get_loose_object(&logarg) && !log_get_pack_object(&logarg)) {
			fprintf(stderr, "The object %s not found, git repository may be corrupt.\n", logarg.sha);
			exit(128);
		}
		if (logarg.status & LOG_STATUS_PARENT)
			putchar('\n');
	}
}

int
log_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1)
		switch(ch) {
		case 0:
			break;
		case 1:
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

	log_display_commits();

	return (ret);
}

