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

#include <sys/queue.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "common.h"
#include "protocol.h"


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

int
proto_parse_response(char *response, struct smart_head *smart_head)
{
	char *position;
	char *token, *string, *tofree;
	long offset;
	int count;

	position = response;
	sscanf(position, "%04lx", &offset);
	position += offset;

	/* The first four bytes are 0000, check and skip ahead */
	if (strncmp(position, "0000", 4))
		return (EINVAL);

	position += 4;

	sscanf(position, "%04lx", &offset);
	position += 4;
	memcpy(smart_head->sha, position, HASH_SIZE);

	tofree = string = strndup(position+41+strlen(position+41)+1,
	    offset-(47+strlen(position+41)));

	while((token = strsep(&string, " \n")) != NULL) {    
		if (!strncmp(token, "multi_ack", 9))    
			smart_head->cap |= PACKPROTO_MULTI_ACK;    
		else if (!strncmp(token, "multi_ack_detailed", 18))     
			smart_head->cap |= PACKPROTO_MULTI_ACK_DETAILED;    
		else if (!strncmp(token, "no-done", 7))    
			smart_head->cap |= PACKPROTO_MULTI_NO_DONE;    
		else if (!strncmp(token, "thin-pack", 9))      
			smart_head->cap |= PACKPROTO_THIN_PACK;    
		else if (!strncmp(token, "side-band", 9))      
			smart_head->cap |= PACKPROTO_SIDE_BAND;       
		else if (!strncmp(token, "side-band-64k", 13))     
			smart_head->cap |= PACKPROTO_SIDE_BAND_64K;    
		else if (!strncmp(token, "ofs-delta", 9))      
			smart_head->cap |= PACKPROTO_OFS_DELTA;    
		else if (!strncmp(token, "agent", 5))      
			smart_head->cap |= PACKPROTO_AGENT;    
		else if (!strncmp(token, "shallow", 7))      
			smart_head->cap |= PACKPROTO_SHALLOW;	
		else if (!strncmp(token, "deepen-since", 12))     
			smart_head->cap |= PACKPROTO_DEEPEN_SINCE;    
		else if (!strncmp(token, "deepen-not", 10))     
			smart_head->cap |= PACKPROTO_DEEPEN_NOT;	
		else if (!strncmp(token, "deepen-relative", 15))     
			smart_head->cap |= PACKPROTO_DEEPEN_RELATIVE;    
		else if (!strncmp(token, "no-progress", 11))     
			smart_head->cap |= PACKPROTO_NO_PROGRESS;    
		else if (!strncmp(token, "include-tag", 11))     
			smart_head->cap |= PACKPROTO_INCLUDE_TAG;     
		else if (!strncmp(token, "report-status", 13))     
			smart_head->cap |= PACKPROTO_REPORT_STATUS;    
		else if (!strncmp(token, "delete-refs", 11))     
			smart_head->cap |= PACKPROTO_DELETE_REFS;    
		else if (!strncmp(token, "quiet", 5))      
			smart_head->cap |= PACKPROTO_QUIET;    
		else if (!strncmp(token, "atomic", 6))    
			smart_head->cap |= PACKPROTO_ATOMIC;    
		else if (!strncmp(token, "push-options", 12))    
			smart_head->cap |= PACKPROTO_PUSH_OPTIONS;    
		else if (!strncmp(token, "allow-tip-sha1-in-want", 22))    
			smart_head->cap |= PACKPROTO_ALLOW_TIP_SHA1_IN_WANT;    
		else if (!strncmp(token, "allow-reachable-sha1-in-want", 28))    
			smart_head->cap |= PACKPROTO_ALLOW_REACHABLE_SHA1_IN_WANT;    
		else if (!strncmp(token, "push-cert", 9))    
			smart_head->cap |= PACKPROTO_PUSH_CERT;    
		else if (!strncmp(token, "filter", 6))    
			smart_head->cap |= PACKPROTO_FILTER;    
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
		memcpy(smart_head->refs[count].sha, position+4, HASH_SIZE);

		smart_head->refs[count].path = strndup(position+4+41,
		    offset-(4+42));

		position += offset;
		count++;
	}
	smart_head->refcount = count;
	return (0);
}
