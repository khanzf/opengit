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

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <fetch.h>
#include <errno.h>
#include "lib/pack.h"
#include "clone.h"

static int
push_ref(struct smart_head *smart_head, const char *refspec)
{
	char *ref, *sep;
	struct symref *symref;
	int ret;

	ref = strdup(refspec);
	if (ref == NULL)
		return (ENOMEM);

	symref = malloc(sizeof(*symref));
	if (symref == NULL) {
		ret = ENOMEM;
		goto out;
	}

	sep = strchr(ref, ':');
	if (sep == NULL) {
		ret = EINVAL;
		goto out;
	}

	*sep++ = '\0';
	symref->symbol = strdup(ref);
	symref->path = strdup(sep);
	STAILQ_INSERT_HEAD(&smart_head->symrefs, symref, link);

	ret = 0;
out:
	if (ret != 0)
		free(symref);
	free(ref);
	return (ret);
}

/* This requests a HEAD sha and parse out the results */
static int
clone_http_get_head(char *url, struct smart_head *smart_head)
{
	FILE *web;
	char fetchurl[1000];
	char out[1024];
	char *response, *position;
	char *token, *string, *tofree;
	int r;
	long offset;
	int count;

	snprintf(fetchurl, sizeof(fetchurl), "%s/info/refs?service=git-upload-pack",
	    url);
	if ((web = fetchGetURL(fetchurl, NULL)) == NULL)
		return (ENOENT);

	offset = 0;
	smart_head->cap = 0;
	response = NULL;
	do {
		r = fread(out, 1, 1024, web);
		response = realloc(response, offset+r);
		memcpy(response+offset, out, r);
		offset += r;
	} while(r >= 1024);

	position = (char *)response;
	sscanf(position, "%04lx", &offset);
	position += offset;

	/* The first four bytes are 0000, check and skip ahead */
	if (strncmp(position, "0000", 4))
		return (EINVAL);

	position += 4;

	sscanf(position, "%04lx", &offset);
	position += 4;
	strncpy(smart_head->sha, position, 40);

	tofree = string = strndup(position+41+strlen(position+41)+1,
	    offset-(47+strlen(position+41)));

	while((token = strsep(&string, " \n")) != NULL) {
		if (!strncmp(token, "multi_ack", 9))
			smart_head->cap |= CLONE_MULTI_ACK;
		else if (!strncmp(token, "multi_ack_detailed", 18))
			smart_head->cap |= CLONE_MULTI_ACK_DETAILED;
		else if (!strncmp(token, "no-done", 7))
			smart_head->cap |= CLONE_MULTI_NO_DONE;
		else if (!strncmp(token, "thin-pack", 9))
			smart_head->cap |= CLONE_THIN_PACK;
		else if (!strncmp(token, "side-band", 9))
			smart_head->cap |= CLONE_SIDE_BAND;
		else if (!strncmp(token, "side-band-64k", 13))
			smart_head->cap |= CLONE_SIDE_BAND_64K;
		else if (!strncmp(token, "ofs-delta", 9))
			smart_head->cap |= CLONE_OFS_DELTA;
		else if (!strncmp(token, "agent", 5))
			smart_head->cap |= CLONE_AGENT;
		else if (!strncmp(token, "shallow", 7))
			smart_head->cap |= CLONE_SHALLOW;
		else if (!strncmp(token, "deepen-since", 12))
			smart_head->cap |= CLONE_DEEPEN_SINCE;
		else if (!strncmp(token, "deepen-not", 10))
			smart_head->cap |= CLONE_DEEPEN_NOT;
		else if (!strncmp(token, "deepen-relative", 15))
			smart_head->cap |= CLONE_DEEPEN_RELATIVE;
		else if (!strncmp(token, "no-progress", 11))
			smart_head->cap |= CLONE_NO_PROGRESS;
		else if (!strncmp(token, "include-tag", 11))
			smart_head->cap |= CLONE_INCLUDE_TAG;
		else if (!strncmp(token, "report-status", 13))
			smart_head->cap |= CLONE_REPORT_STATUS;
		else if (!strncmp(token, "delete-refs", 11))
			smart_head->cap |= CLONE_DELETE_REFS;
		else if (!strncmp(token, "quiet", 5))
			smart_head->cap |= CLONE_QUIET;
		else if (!strncmp(token, "atomic", 6))
			smart_head->cap |= CLONE_ATOMIC;
		else if (!strncmp(token, "push-options", 12))
			smart_head->cap |= CLONE_PUSH_OPTIONS;
		else if (!strncmp(token, "allow-tip-sha1-in-want", 22))
			smart_head->cap |= CLONE_ALLOW_TIP_SHA1_IN_WANT;
		else if (!strncmp(token, "allow-reachable-sha1-in-want", 28))
			smart_head->cap |= CLONE_ALLOW_REACHABLE_SHA1_IN_WANT;
		else if (!strncmp(token, "push-cert", 9))
			smart_head->cap |= CLONE_PUSH_CERT;
		else if (!strncmp(token, "filter", 6))
			smart_head->cap |= CLONE_FILTER;
		else if (!strncmp(token, "symref", 6))
			push_ref(smart_head, strstr(token, "=") + 1);
	}
	free(tofree);

	position += offset - 4;

	/* Iterate through the refs */
	count = 0;
	while(strncmp(position, "0000", 4)) {
		smart_head->refs = realloc(smart_head->refs, sizeof(struct smart_head) * (count+1));
		sscanf(position, "%04lx", &offset);
		strncpy(smart_head->refs[count].sha, position+4, 40);
		smart_head->refs[count].sha[40] = '\0';

		smart_head->refs[count].path = strndup(position+4+41,
		    offset-(4+42));

		position += offset;
		count++;
	}
	smart_head->refcount = count;
	return (0);
}

static int
clone_http_build_done(char **content, int content_length)
{
	*content = realloc(*content, content_length + 14);
	strncpy(*content+content_length, "00000009done\n\0", 14);
	return 13; // Always the same length
}

static int
clone_http_build_want(char **content, int content_length, char *capabilities, const char *sha)
{
	char line[3000]; // XXX Bad approach
	int len;

	/* size + want + space + SHA(40) + space + capabilities + newline */
	len = 4 + 4 + 1 + 40 + 1 + strlen(capabilities) + 1;

	sprintf(line, "%04xwant %s %s\n", len, sha, capabilities);
	*content = realloc(*content, content_length + len + 1);
	strncpy(*content+content_length, line, len+1);

	return len;
}

static int
clone_build_post_content(const char *sha, char **content)
{
	int content_length;
	char *capabilities = "multi_ack_detailed no-done side-band-64k thin-pack ofs-delta deepen-since deepen-not agent=opengit/0.0.1-pre";

	content_length = 0;

	content_length += clone_http_build_want(content, content_length,
	    capabilities, sha);
	content_length += clone_http_build_done(content, content_length);

	return content_length;
}

static void
clone_http_get_sha(int packfd, char *url, struct smart_head *smart_head)
{
	char git_upload_pack[1000];
	char *content = NULL;
	int content_length;
	struct parseread parseread;
	struct url *fetchurl;
	FILE *packptr;

	sprintf(git_upload_pack, "%s/git-upload-pack", url);

	fetchurl = fetchParseURL(git_upload_pack);
	if (fetchurl == NULL) {
		fprintf(stderr, "Unable to parse url: %s\n", url);
		exit(128);
	}
	parseread.state = STATE_NEWLINE;
	parseread.cremnant = 0;
	parseread.fd = packfd;

	content_length = clone_build_post_content(smart_head->sha, &content);

	setenv("HTTP_ACCEPT", "application/x-git-upload-pack-result", 1);
	packptr = fetchReqHTTP(fetchurl, "POST", NULL, "application/x-git-upload-pack-request", content);
	if (packptr == NULL) {
		fprintf(stderr, "Unable to contact url: %s\n", url);
		exit(128);
	}

	size_t sz;
	int ret;
	char buf[1024];
	while((sz = fread(buf, 1, 1024, packptr)) > 0) {
		ret = clone_pack_protocol_process(buf, 1, sz, &parseread);
		if (ret != sz) {
			fprintf(stderr, "Error parsing http response. Exiting.\n");
			exit(128);
		}
	}

	fclose(packptr);
	free(content);

}

int
clone_http(char *uri, char *repodir, struct smart_head *smart_head)
{
	int packfd;
	int offset;
	int idxfd;
	struct packfileinfo packfileinfo;
	struct index_entry *index_entry;
	char path[PATH_MAX];
	char srcpath[PATH_MAX];
	int pathlen, ret;
	char *fetch_uri, *suffix;
	SHA1_CTX packctx;
	SHA1_CTX idxctx;

	pathlen = strlen(repodir);
	strncpy(path, repodir, pathlen);
	strncpy(srcpath, repodir, pathlen);
	suffix = path + strlen(repodir);

	strncat(suffix, "/.git/objects/pack/_tmp.pack", PATH_MAX-pathlen);
	packfd = open(path, O_RDWR | O_CREAT, 0660);
	if (packfd == -1) {
		fprintf(stderr, "Unable to open file %s.\n", path);
		exit(-1);
	}

	fetch_uri = uri;
again:
	ret = clone_http_get_head(fetch_uri, smart_head);
	if (ret != 0) {
		switch (ret) {
		case ENOENT:
			/* Fortunately, we can assume fetch_uri length will always be > 4 */
			if (strcmp(fetch_uri + (strlen(fetch_uri) - 4), ".git") != 0) {
				/* We'll try again with .git (+ null terminator) */
				fetch_uri = malloc(strlen(uri) + 5);
				snprintf(fetch_uri, strlen(uri) + 5, "%s.git", uri);
				goto again;
			}
			fprintf(stderr, "Unable to clone repository: %s\n", uri);
			break;
		case EINVAL:
			fprintf(stderr, "Protocol mismatch.\n");
			break;
		}

		ret = 128;
		goto out;
	}
	clone_http_get_sha(packfd, fetch_uri, smart_head);

	/* Jump to the beginning of the file */
	lseek(packfd, 0, SEEK_SET);

	SHA1_Init(&packctx);
	SHA1_Init(&idxctx);

	offset = pack_parse_header(packfd, &packfileinfo, &packctx);
	index_entry = malloc(sizeof(struct index_entry) * packfileinfo.nobjects);
	offset = pack_get_object_meta(packfd, offset, &packfileinfo, index_entry,
	    &packctx, &idxctx);
	close(packfd);

	SHA1_Final(packfileinfo.sha, &packctx);

	/* Sort the index entry */
	qsort(index_entry, packfileinfo.nobjects, sizeof(struct index_entry),
	    sortindexentry);

	strncpy(suffix, "/.git/objects/pack/_tmp.idx", PATH_MAX-pathlen);
	idxfd = open(path, O_RDWR | O_CREAT, 0660);
	if (idxfd == -1) {
		fprintf(stderr, "Unable to open packout.idx for writing.\n");
		ret = -1;
		goto out;
	}

	pack_build_index(idxfd, &packfileinfo, index_entry, &idxctx);
	free(index_entry);
	close(idxfd);

	strncpy(suffix, "/.git/objects/pack/pack-", 24);
	for(int x=0;x<20;x++)
		snprintf(suffix+24+(x*2), 3, "%02x", packfileinfo.sha[x]);

	/* Rename pack and index files */
	strncat(suffix, ".pack", 6);
	strncat(srcpath, "/.git/objects/pack/_tmp.pack", strlen(path));
	rename(srcpath, path);

	strncpy(srcpath+strlen(srcpath)-4, "idx", 4);
	strncpy(path+strlen(path)-4, "idx", 5);
	rename(srcpath, path);
	ret = 0;
out:
	if (fetch_uri != uri)
		free(fetch_uri);
	return (ret);
}

