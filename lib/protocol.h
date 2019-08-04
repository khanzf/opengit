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

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <sys/queue.h>

/* Specifies the pack protocol capabilities */
#define PACKPROTO_MULTI_ACK				BIT(0)
#define PACKPROTO_MULTI_ACK_DETAILED			BIT(1)
#define PACKPROTO_MULTI_NO_DONE				BIT(2)
#define PACKPROTO_THIN_PACK				BIT(3)
#define PACKPROTO_SIDE_BAND				BIT(4)
#define PACKPROTO_SIDE_BAND_64K				BIT(5)
#define PACKPROTO_OFS_DELTA				BIT(6)
#define PACKPROTO_AGENT					BIT(7)
#define PACKPROTO_SHALLOW				BIT(8)
#define PACKPROTO_DEEPEN_SINCE				BIT(9)
#define PACKPROTO_DEEPEN_NOT				BIT(10)
#define PACKPROTO_DEEPEN_RELATIVE			BIT(11)
#define PACKPROTO_NO_PROGRESS				BIT(12)
#define PACKPROTO_INCLUDE_TAG				BIT(13)
#define PACKPROTO_REPORT_STATUS				BIT(14)
#define PACKPROTO_DELETE_REFS				BIT(15)
#define PACKPROTO_QUIET					BIT(16)
#define PACKPROTO_ATOMIC				BIT(17)
#define PACKPROTO_PUSH_OPTIONS				BIT(18)
#define PACKPROTO_ALLOW_TIP_SHA1_IN_WANT		BIT(19)
#define PACKPROTO_ALLOW_REACHABLE_SHA1_IN_WANT		BIT(20)
#define PACKPROTO_PUSH_CERT				BIT(21)
#define PACKPROTO_FILTER				BIT(22)

/* Protocol state */
#define STATE_NEWLINE					0
#define STATE_NAK					1
#define STATE_REMOTE					2
#define STATE_PACK					3
#define STATE_UNKNOWN					999

struct parseread {
	int		state;		// current state
	int		osize;		// object size
	int		psize;		// processed size
	int		cremnant;	// Remnant count
	char		bremnant[4];	// Remnant bytes
	int		fd;
};

struct symref {
	char			*symbol;
	char			*path;

	STAILQ_ENTRY(symref)	 link;
};

struct ref {
	char			 sha[HASH_SIZE];
	char			*path;

	STAILQ_ENTRY(symref)     link;
};

struct smart_head {
	char			 sha[HASH_SIZE+1];
	char		 	 headname[1024];
	uint32_t	 	 cap;
	int		 	 refcount;
	struct ref		*refs;

	STAILQ_HEAD(, symref)	 symrefs;
};

int	proto_parse_response(char *response, struct smart_head *smart_head);
size_t	proto_process_pack(void *buffer, size_t size, size_t nmemb, void *userp);


#endif
