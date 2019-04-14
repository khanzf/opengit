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
#include <fetch.h>
#include <sha.h>
#include "lib/zlib-handler.h"
#include "lib/common.h"
#include "lib/pack.h"
#include "lib/ini.h"
#include "clone.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

void
clone_usage(int type)
{
	fprintf(stderr, "Git clone usage statement\n");
	exit(128);
}

void
clone_http_get_head(char *url, char *sha)
{
	struct smart_head smart_head;
	FILE *web;
	char fetchurl[1000];
	char out[1024];
	char *response, *position;
	char *token, *string, *tofree;
	int r;
	long offset;

	sprintf(fetchurl, "%s/info/refs?service=git-upload-pack", url);
	if ((web = fetchGetURL(fetchurl, NULL)) == NULL) {
		fprintf(stderr, "Unable to clone repository: %s\n", url);
		exit(128);
	}
	offset = 0;
	smart_head.cap = 0;
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

	if (strncmp(position, "0000", 4)) {
		fprintf(stderr, "Protocol mismatch.\n");
		exit(128);
	}
	position += 4;

	sscanf(position, "%04lx", &offset);
	position += 4;
	strncpy(smart_head.sha, position, 41);

	tofree = string = strndup(position+41+strlen(position+41)+1,
			    offset-(47+strlen(position+41)));

	while((token = strsep(&string, " \n")) != NULL) {
		if (!strncmp(token, "multi_ack", 9))
			smart_head.cap |= CLONE_MULTI_ACK;
		else if (!strncmp(token, "multi_ack_detailed", 18))
			smart_head.cap |= CLONE_MULTI_ACK_DETAILED;
		else if (!strncmp(token, "no-done", 7))
			smart_head.cap |= CLONE_MULTI_NO_DONE;
		else if (!strncmp(token, "thin-pack", 9))
			smart_head.cap |= CLONE_THIN_PACK;
		else if (!strncmp(token, "side-band", 9))
			smart_head.cap |= CLONE_SIDE_BAND;
		else if (!strncmp(token, "side-band-64k", 13))
			smart_head.cap |= CLONE_SIDE_BAND_64K;
		else if (!strncmp(token, "ofs-delta", 9))
			smart_head.cap |= CLONE_OFS_DELTA;
		else if (!strncmp(token, "agent", 5))
			smart_head.cap |= CLONE_AGENT;
		else if (!strncmp(token, "shallow", 7))
			smart_head.cap |= CLONE_SHALLOW;
		else if (!strncmp(token, "deepen-since", 12))
			smart_head.cap |= CLONE_DEEPEN_SINCE;
		else if (!strncmp(token, "deepen-not", 10))
			smart_head.cap |= CLONE_DEEPEN_NOT;
		else if (!strncmp(token, "deepen-relative", 15))
			smart_head.cap |= CLONE_DEEPEN_RELATIVE;
		else if (!strncmp(token, "no-progress", 11))
			smart_head.cap |= CLONE_NO_PROGRESS;
		else if (!strncmp(token, "include-tag", 11))
			smart_head.cap |= CLONE_INCLUDE_TAG;
		else if (!strncmp(token, "report-status", 13))
			smart_head.cap |= CLONE_REPORT_STATUS;
		else if (!strncmp(token, "delete-refs", 11))
			smart_head.cap |= CLONE_DELETE_REFS;
		else if (!strncmp(token, "quiet", 5))
			smart_head.cap |= CLONE_QUIET;
		else if (!strncmp(token, "atomic", 6))
			smart_head.cap |= CLONE_ATOMIC;
		else if (!strncmp(token, "push-options", 12))
			smart_head.cap |= CLONE_PUSH_OPTIONS;
		else if (!strncmp(token, "allow-tip-sha1-in-want", 22))
			smart_head.cap |= CLONE_ALLOW_TIP_SHA1_IN_WANT;
		else if (!strncmp(token, "allow-reachable-sha1-in-want", 28))
			smart_head.cap |= CLONE_ALLOW_REACHABLE_SHA1_IN_WANT;
		else if (!strncmp(token, "push-cert", 9))
			smart_head.cap |= CLONE_PUSH_CERT;
		else if (!strncmp(token, "filter", 6))
			smart_head.cap |= CLONE_FILTER;
	}
	free(tofree);

	position += offset;
	memcpy(sha, position, 40);
	sha[40] = '\0';
}

int
clone_http_build_done(char **content, int content_length)
{
	*content = realloc(*content, content_length + 14);
	strncpy(*content+content_length, "00000009done\n\0", 14);
	return 13; // Always the same length
}

int
clone_http_build_want(char **content, int content_length, char *capabilities, const char *sha)
{
	char line[3000]; // XXX Bad approach
	int len;

	// size + want + space + SHA(40) + space + capabilities + newline
	len = 4 + 4 + 1 + 40 + 1 +strlen(capabilities) + 1;

	sprintf(line, "%04xwant %s %s\n", len, sha, capabilities);
	*content = realloc(*content, content_length + len + 1);
	strncpy(*content+content_length, line, len+1);

	return len;
}

int
clone_build_post_content(const char *sha, char **content)
{
	int content_length;
	char *capabilities = "multi_ack_detailed no-done side-band-64k thin-pack ofs-delta deepen-since deepen-not agent=git/2.19.2.windows.1";

	content_length = 0;

	content_length += clone_http_build_want(content, content_length, capabilities, sha);
	content_length += clone_http_build_done(content, content_length);

	return content_length;
}

// XXX This may be moved to pack.[ch]
void
process_nak()
{
	// Currently unimplemented
}

void
process_unknown(unsigned char *reply, struct parseread *parseread, int offset, int size)
{
	if (parseread->psize == 0)
		write(parseread->fd, reply+5, size-5);
	else
		write(parseread->fd, reply, size);
}


void
process_remote(unsigned char *reply, struct parseread *parseread)
{
	char buf[200];
	strncpy(buf, (char *)reply+5, parseread->osize-5);
	buf[parseread->osize-5] = '\0';

	printf("Remote: %s", buf);
}

void
process_objects(unsigned char *reply, struct parseread *parseread, int offset, int size)
{
	switch(parseread->state) {
	case STATE_NAK:
		process_nak();
		break;
	case STATE_REMOTE:
		process_remote(reply, parseread);
		break;
	default:
		process_unknown(reply, parseread, offset, size);
		break;
	}
}

// XXX This may be moved to pack.[ch]
size_t
clone_pack_protocol_process(void *buffer, size_t size, size_t nmemb, void *userp)
{
	unsigned char *reply;
	struct parseread *parseread = userp;
	int offset = 0;
	char tmp[5];
	int check;
	int pool;

	/*
	 * Check if there are any left-over bits from the previous iteration
	 * If so, add them to the reply and increase the pool size
	 * Otherwise, use the given values
	*/
	if (parseread->cremnant > 0) {
		reply = malloc(nmemb + parseread->cremnant + 1);
		memcpy(reply+parseread->cremnant, buffer, size * nmemb);
		memcpy(reply, parseread->bremnant, parseread->cremnant);
		pool = (size * nmemb) + parseread->cremnant;
	}
	else {
		reply = buffer;
		pool = size * nmemb;
	}

	/*
	 * Iterate up to the pool size being 4 bytes, in the event that the last
	 * four bytes are the beginning of a new object
	 */
	while(offset + 4 < pool) {
		if (parseread->state == STATE_NEWLINE) {
			bzero(tmp, 5);
			memcpy(tmp, reply+offset, 4); tmp[4] = '\0';
			check = sscanf(tmp, "%04x", &parseread->osize);

			if (parseread->osize == 0) {
				offset += 4;
				break;
			}

			parseread->psize = 0;

			if ( strncmp((char *)reply+offset+4, "NAK\n", 4)==0)
				parseread->state = STATE_NAK;
			else if (reply[offset+4] == 0x02)
				parseread->state = STATE_REMOTE;
			else
				parseread->state = STATE_UNKNOWN;
		}

		/*
		 * If the pool minus the offset is greater or equal to the
		 * remaining necessary bytes. This means we completed the data
		 * in that object and can reset to a new state.
		 */
		if ((pool - offset) >= (parseread->osize - parseread->psize) ) {
			process_objects(reply+offset, parseread, offset,
			    (parseread->osize - parseread->psize));
			offset += (parseread->osize - parseread->psize);
			parseread->state = STATE_NEWLINE;
		}
		/* Otherwise, we need more bytes */
		else if ((pool-offset) < (parseread->osize - parseread->psize)) {
			process_objects(reply+offset, parseread, offset,
			    pool-offset);
			parseread->psize += (pool - offset);
			offset = pool;
		}

	}

	/* The while-loop could break and the offset has not caught up to
	 * the pool. In this case, we need to cache those 4 bytes, update
	 * the parseread->cremnant value and return offset + parseread->cremnant
	 * (which should be nmemb * size).
	 */
	if (pool-offset > 0)
		memcpy(parseread->bremnant, reply+offset, pool-offset);
	if (parseread->cremnant > 0)
		free(reply);
	parseread->cremnant = nmemb - offset;

	return offset + parseread->cremnant;

}

void
clone_http_get_sha(char *url, char *sha, int packfd)
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

	content_length = clone_build_post_content(sha, &content);

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

void
clone_http(char *url)
{
	char headsha[41];
	int packfd;
	int offset;
	int idxfd;
	struct packfileinfo packfileinfo;
	struct index_entry *index_entry;
	char filename[70];
	SHA1_CTX packctx;
	SHA1_CTX idxctx;

	packfd = open(".git/objects/pack/_tmp.pack", O_RDWR | O_CREAT, 0660);
	if (packfd == -1) {
		fprintf(stderr, "Unable to open file .git/objects/pack/_tmp.pack.");
		exit(-1);
	}

	clone_http_get_head(url, headsha);
	clone_http_get_sha(url, headsha, packfd);

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

	idxfd = open(".git/objects/pack/_tmp.idx", O_RDWR | O_CREAT, 0660);
	if (idxfd == -1) {
		fprintf(stderr, "Unable to open packout.idx for writing\n");
		exit(idxfd);
	}

	pack_build_index(idxfd, &packfileinfo, index_entry, &idxctx);
	free(index_entry);
	close(idxfd);

	strncpy(filename, ".git/objects/pack/pack-", 23);
	for(int x=0;x<20;x++)
		snprintf(filename+23+(x*2), 3, "%02x", packfileinfo.sha[x]);

	/* Rename pack and index files */
	strncpy(filename+63, ".pack", 6);
	rename(".git/objects/pack/_tmp.pack", filename);
	strncpy(filename+63, ".idx", 5);
	rename(".git/objects/pack/_tmp.idx", filename);
}

int
clone_main(int argc, char *argv[])
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

	git_repository_path();
	clone_http(argv[1]);

	return (ret);
}

