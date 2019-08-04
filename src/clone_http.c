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
#include "lib/protocol.h"
#include "clone.h"

static int
clone_http_build_done(char **content, int content_length)
{
	*content = realloc(*content, content_length + 14);
	strlcpy(*content+content_length, "00000009done\n\0", 14);
	return (13); // Always the same length
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
	strlcpy(*content+content_length, line, len+1);

	return (len);
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

	return (content_length);
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

static int
http_get_head(char *uri, char **response)
{
	FILE *web;
	long offset = 0;
	long r;
	char fetchurl[1024];
	char out[1024];

	snprintf(fetchurl, sizeof(fetchurl), "%s/info/refs?service=git-upload-pack", uri);
	if ((web = fetchGetURL(fetchurl, NULL)) == NULL)
		return (ENOENT);

	do {
		r = fread(out, 1, 1024, web);
		*response = realloc(*response, offset+r);
		memcpy(*response+offset, out, r);
		offset += r;
	} while(r >= 1024);

	fclose(web);

	return (0);
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
	int ret;
	char *fetch_uri;
	char *response;
	SHA1_CTX packctx;
	SHA1_CTX idxctx;

	strlcpy(path, dotgitpath, PATH_MAX);

	strlcat(path, "objects/pack/_tmp.pack", PATH_MAX);
	packfd = open(path, O_RDWR | O_CREAT, 0660);
	if (packfd == -1) {
		fprintf(stderr, "1 Unable to open file %s.\n", path);
		exit(-1);
	}

	fetch_uri = uri;
again:
	response = NULL;
	ret = http_get_head(fetch_uri, &response);
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
	ret = proto_parse_response(response, smart_head);
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

	strncpy(path, dotgitpath, PATH_MAX);
	strncat(path, "objects/pack/_tmp.idx", PATH_MAX);
	idxfd = open(path, O_RDWR | O_CREAT, 0660);
	if (idxfd == -1) {
		fprintf(stderr, "Unable to open packout.idx for writing.\n");
		ret = -1;
		goto out;
	}

	pack_build_index(idxfd, &packfileinfo, index_entry, &idxctx);
	free(index_entry);
	close(idxfd);

	char *suffix = path;
	strncpy(path, dotgitpath, PATH_MAX);
	suffix += strlcat(path, "objects/pack/pack-", PATH_MAX);
	for(int x=0;x<20;x++)
		snprintf(suffix+(x*2), 3, "%02x", packfileinfo.sha[x]);

	/* Rename pack and index files */
	strlcat(suffix, ".pack", PATH_MAX);

	strncpy(srcpath, dotgitpath, PATH_MAX);
	strlcat(srcpath, "objects/pack/_tmp.pack", PATH_MAX);
	rename(srcpath, path);

	strlcpy(srcpath+strlen(srcpath)-4, "idx", 4);
	strlcpy(path+strlen(path)-4, "idx", 4);
	rename(srcpath, path);
	ret = 0;
out:
	if (fetch_uri != uri)
		free(fetch_uri);
	return (ret);
}

