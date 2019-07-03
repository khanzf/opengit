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

#ifndef __ZLIB_HANDLER__H
#define __ZLIB_HANDLER__H

/* Used to process both the crc and ctx data */
struct two_darg {
	void		*crc;
	void		*sha;
};

struct writer_args {
	int		fd;
	long		sent;
};

#define CHUNK 16384

typedef int		 deflated_handler(unsigned char *, int, void *);
typedef unsigned char	*inflated_handler(unsigned char *, int, int, void *);

int			 deflate_caller(int sourcefd, deflated_handler deflated_handler, void *darg,
			     inflated_handler inflated_handler, void *arg);
unsigned char		*write_cb(unsigned char *buf, int size, int __unused deflate_bytes, void *arg);
unsigned char		*buffer_cb(unsigned char *buf, int size, int deflate_size, void *arg);
int			 zlib_update_sha(unsigned char *data, int use, void *darg);
int			zlib_update_crc(unsigned char *data, int use, void *darg);
int			zlib_update_crc_sha(unsigned char *data, int use, void *darg);

#endif
