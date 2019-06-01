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


#ifndef __CLONE_H__
#define __CLONE_H__

#include "lib/common.h"

#define CLONE_MULTI_ACK				BIT(0)
#define CLONE_MULTI_ACK_DETAILED		BIT(1)
#define CLONE_MULTI_NO_DONE			BIT(2)
#define CLONE_THIN_PACK				BIT(3)
#define CLONE_SIDE_BAND				BIT(4)
#define CLONE_SIDE_BAND_64K			BIT(5)
#define CLONE_OFS_DELTA				BIT(6)
#define CLONE_AGENT				BIT(7)
#define CLONE_SHALLOW				BIT(8)
#define CLONE_DEEPEN_SINCE			BIT(9)
#define CLONE_DEEPEN_NOT			BIT(10)
#define CLONE_DEEPEN_RELATIVE			BIT(11)
#define CLONE_NO_PROGRESS			BIT(12)
#define CLONE_INCLUDE_TAG			BIT(13)
#define CLONE_REPORT_STATUS			BIT(14)
#define CLONE_DELETE_REFS			BIT(15)
#define CLONE_QUIET				BIT(16)
#define CLONE_ATOMIC				BIT(17)
#define CLONE_PUSH_OPTIONS			BIT(18)
#define CLONE_ALLOW_TIP_SHA1_IN_WANT		BIT(19)
#define CLONE_ALLOW_REACHABLE_SHA1_IN_WANT	BIT(20)
#define CLONE_PUSH_CERT				BIT(21)
#define CLONE_FILTER				BIT(22)

struct ref {
	char		sha[41];
	char		*path;
};

struct smart_head {
	char		sha[41];
	uint32_t	cap;
	int		refcount;
	struct ref	*refs;
};

struct branch {
	char 		sha[41];
	char 		*name;
};

#define STATE_NEWLINE			0
#define STATE_NAK			1
#define STATE_REMOTE			2
#define STATE_PACK			3
#define STATE_UNKNOWN			999

struct parseread {
	int		state;		// current state
	int		osize;		// object size
	int		psize;		// processed size
	int		cremnant;	// Remnant count
	char		bremnant[4];	// Remnant byes
	int		fd;
};

int	clone_main(int argc, char *argv[]);

int clone_http(char *url, char *repodir, struct smart_head *smart_head);
size_t clone_pack_protocol_process(void *buffer, size_t size, size_t nmemb,
    void *userp);

#endif
