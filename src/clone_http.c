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
match_http(char *uri)
{
	if (strncmp(uri, "http://", 7) == 0)
		return 1;
	else if (strncmp(uri, "https://", 8) == 0)
		return 1;
	return 0;
}

FILE *
http_get_pack_fptr(char *url, char *content)
{
	char git_upload_pack[1000];
	struct url *fetchurl;
	FILE *packptr;

	snprintf(git_upload_pack, 1000, "%s/git-upload-pack", url);

	setenv("HTTP_ACCEPT", "application/x-git-upload-pack-result", 1);
	fetchurl = fetchParseURL(git_upload_pack);
	if (fetchurl == NULL) {
		fprintf(stderr, "Unable to parse url: %s\n", url);
		exit(128);
	}

	packptr = fetchReqHTTP(fetchurl, "POST", NULL, "application/x-git-upload-pack-request", content);
	if (packptr == NULL) {
		fprintf(stderr, "Unable to contact url: %s\n", url);
		exit(128);
	}

	return packptr;
}

int
http_get_repo_state(char *uri, char **response)
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
