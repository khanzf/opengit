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
#include <stdlib.h>
#include <zlib.h>
#include <unistd.h>
#include <string.h>
#include "zlib-handler.h"

unsigned char *
buffer_cb(unsigned char *buf, int size, int deflated_size, void *arg)
{
	struct decompressed_object *decompressed_object = arg;

	decompressed_object->data = realloc(decompressed_object->data, decompressed_object->size + size);
	memcpy(decompressed_object->data + decompressed_object->size, buf, size);
	decompressed_object->size += size;
	decompressed_object->deflated_size += deflated_size;

	return buf;
}

unsigned char *
write_cb(unsigned char *buf, int size, int __unused deflate_bytes, void *arg)
{
	struct writer_args *writer_args = arg;

	writer_args->sent += write(writer_args->fd, buf, size);

	return buf;
}

int
deflate_caller(int sourcefd, inflated_handler inflated_handler, uint32_t *crcv, void *arg) {
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];
	unsigned have;
	z_stream strm;
	int ret;
	int input_len;
	int use;
	int burn = 0;
	
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	do {
		strm.avail_in = input_len = read(sourcefd, in, CHUNK);
		if (strm.avail_in == -1) {
			(void)inflateEnd(&strm);
			perror("read from source file");
			return Z_ERRNO;
		}
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);

			switch(ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			have = CHUNK - strm.avail_out;

			// Return value of 0 code means exit
			use = input_len - strm.avail_in;
			if (crcv != NULL)
				*crcv = crc32(*crcv, in+burn, use);
			burn+=use;
			input_len -= use;
			if (inflated_handler(out, have, use, arg) == NULL)
				goto end_inflation;

		} while(strm.avail_out == 0);
	} while(ret != Z_STREAM_END);

end_inflation:
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

int
zlib_deliver_loose_object_content(unsigned char *buf, int size, void *data)
{
	return 0;
}
