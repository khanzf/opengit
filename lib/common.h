/*-
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


#ifndef __COMMON_H__
#define __COMMON_H__

#include <limits.h>
#include <stdint.h>

/* Portability */
#if defined(__FreeBSD__)
#include <sha.h>
#elif defined(__OpenBSD__)
#include <sha1.h>

#define SHA1_End(x, y)	SHA1End(x, y)
#define SHA1_Final(x, y) SHA1Final(x, y)
#define SHA1_Init(x)	SHA1Init(x)
#define SHA1_Update(x, y, z) SHA1Update(x, y, z)
#else
#error "Unsure what to do for inclusion of sha1 bits in this environment"
#endif

/* From Documentation/technical/pack-format.txt */
#define OBJ_NONE		0
#define OBJ_COMMIT		1
#define OBJ_TREE		2
#define OBJ_BLOB		3
#define OBJ_TAG			4
// Reserved			5
#define OBJ_OFS_DELTA		6
#define OBJ_REF_DELTA		7

#define HASH_SIZE	40

#define CONTENT_HANDLER(sha, loose_handler, pack_handler, args) {	\
	if (loose_content_handler(sha, loose_handler, args))		\
		pack_content_handler(sha, pack_handler, args);		\
} while(0)


#ifndef nitems
#define nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

#define BIT(nr)		(1 << (nr))

typedef void		tree_handler(char *, uint8_t, char *, char *, void *);

extern char		dotgitpath[PATH_MAX];
int			git_repository_path();

void			iterate_tree(char *treesha, tree_handler tree_handler, void *args);
void			sha_bin_to_str(uint8_t *bin, char *str);
void			sha_str_to_bin(char *str, uint8_t *bin);

#endif
