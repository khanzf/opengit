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


#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "zlib-handler.h"
#include "common.h"
#include "loose.h"
#include "pack.h"

/*
 * Provides a generic way to parse loose content
 * This is used to parse data in multiple ways.
 * Similar to pack_content_handler
 */
int loose_content_handler(char *sha, inflated_handler inflated_handler, void *iarg)
{
	char objectpath[PATH_MAX];
	int objectfd;

	snprintf(objectpath, sizeof(objectpath), "%s/objects/%c%c/%s", dotgitpath, sha[0], sha[1], sha+2);
	objectfd = open(objectpath, O_RDONLY);
	if (objectfd == -1)
		return (1);

	deflate_caller(objectfd, NULL, NULL, inflated_handler, iarg);

	return (0);
}

/*
 * Description: Recover lose object headers
 */
int
loose_get_headers(unsigned char *buf, int size, void *arg)
{
	struct loosearg *loosearg = arg;
	unsigned char *endptr;
	int hdr_offset = 0;

	// Is there a cleaner way to do this?
	if (!memcmp(buf, "commit ", 7)) {
		loosearg->type = OBJ_COMMIT;
		loosearg->size = strtol((char *)buf + 7, (char **)&endptr, 10);
		hdr_offset = 8 + (endptr - (buf+7));
	}
	else if (!memcmp(buf, "tree ", 5)) {
		loosearg->type = OBJ_TREE;
		loosearg->size = strtol((char *)buf + 5, (char **)&endptr, 10);
		hdr_offset = 6 + (endptr - (buf+5));
	}
	else if (!memcmp(buf, "blob ", 5)) {
		loosearg->type = OBJ_BLOB;
		loosearg->size = strtol((char *)buf + 5, (char **)&endptr, 10);
		hdr_offset = 6 + (endptr - buf);
	}
	else if (!memcmp(buf, "tag", 3)) {
		loosearg->type = OBJ_TAG;
		hdr_offset = 3;
	}
	else if (!memcmp(buf, "obj_ofs_delta", 13)) {
		loosearg->type = OBJ_REF_DELTA;
		hdr_offset = 15;
	}
	else if (!memcmp(buf, "obj_ref_delta", 13)) {
		loosearg->type = OBJ_REF_DELTA;
		hdr_offset = 15;
	}

	return (hdr_offset);
}




/*
 * Description: Just gets the type of a loose object
 * Handler for loose_content_handler
 */
unsigned char *
get_type_loose_cb(unsigned char *buf, int size, int __unused deflated_bytes, void *arg)
{
	uint8_t *type = arg;
	struct loosearg loosearg;

	loose_get_headers(buf, size, &loosearg);
	*type = loosearg.type;
	return (buf);
}
