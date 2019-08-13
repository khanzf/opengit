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

/* Match the http(s)? scheme */
int
match_http(struct clone_handler *chandler, char *uri)
{
	struct url *fetchurl = fetchParseURL(uri);

	if (fetchurl != NULL) {
		struct conn_http *conn_http = malloc(sizeof(struct conn_http));
		conn_http->fetchurl = fetchurl;
		chandler->path = &fetchurl->doc;
		chandler->conn_data = conn_http;
		return 1;
	}

	fetchFreeURL(fetchurl);

	return 0;
}

/* Gets the current repository state */
int
http_get_repo_state(struct clone_handler *chandler, char **response)
{
	char *savedoc;
	FILE *web;
	long offset = 0;
	long r;
	struct conn_http *conn_http = chandler->conn_data;
	struct url *fetchurl = conn_http->fetchurl;
	char out[1024];
	char git_upload_pack[1000];
	int ret;


	snprintf(git_upload_pack, 1000, "%s/info/refs?service=git-upload-pack", fetchurl->doc);
	savedoc = fetchurl->doc;
	fetchurl->doc = git_upload_pack;
	web = fetchReqHTTP(fetchurl, "POST", "NULL", "*/*", NULL);
	if (web == NULL)
		ret = ENOENT;
	else {
		ret = 0;
		do {
			r = fread(out, 1, 1024, web);
			*response = realloc(*response, offset+r);
			memcpy(*response+offset, out, r);
			offset += r;
		} while(r >= 1024);

		fclose(web);
	}
	fetchurl->doc = savedoc;

	return (ret);
}

/* Returns a file pointer to the connection */
FILE *
http_get_pack_stream(struct clone_handler *chandler, char *content)
{
	char *savedoc;
	char git_upload_pack[1000];
	struct conn_http *conn_http = chandler->conn_data;
	struct url *fetchurl = conn_http->fetchurl;
	FILE *packptr;

	snprintf(git_upload_pack, 1000, "%s/git-upload-pack", fetchurl->doc);
	savedoc = fetchurl->doc;
	fetchurl->doc = git_upload_pack;

	setenv("HTTP_ACCEPT", "application/x-git-upload-pack-result", 1);

	packptr = fetchReqHTTP(fetchurl, "POST", NULL, "application/x-git-upload-pack-request", content);
	if (packptr == NULL) {
		fprintf(stderr, "Unable to contact url: %s\n", fetchurl->doc);
		exit(128);
	}
	fetchurl->doc = savedoc;

	return packptr;
}
