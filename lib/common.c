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


#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "common.h"
#include "pack.h"
#include "loose.h"
#include "zlib-handler.h"

/*
	Gets the path of the .git directory
	If found, returns 0 and sets dotgitpath
	If not found, returns 1
	If errors, returns -1
*/

char dotgitpath[PATH_MAX];

/*
 * Description: Recovers a tree object and iterates through it. It will also
 * identify the object type.
 * Arguments: 1) treesha is a char[HASH_SIZE]
 * 2) tree_handler is what the function should do with the resultant data
 */
void
iterate_tree(char *treesha, tree_handler tree_handler, void *args)
{
	struct decompressed_object decompressed_object;
	if (loose_content_handler(treesha, buffer_cb, &decompressed_object))
		pack_content_handler(treesha, pack_buffer_cb, &decompressed_object);

	long offset = 0;
	long space;
	uint8_t *shabin;
	char shastr[HASH_SIZE+1], mode[7];
	char *filename;
	uint8_t type;

	while(offset<decompressed_object.size) {
		/* Get the file mode */
		strlcpy(mode, (char *)decompressed_object.data+offset, 7);
		if (mode[5] != ' ')
			space = 6;
		else
			space = 5;
		offset = offset + space;
		mode[space] = '\0';

		filename = (char *)decompressed_object.data+offset+1;
		offset = offset + strlen((char *)decompressed_object.data+offset) + 1;

		/* Get the SHA, convert it to a string */
		shabin = decompressed_object.data + offset;
		sha_bin_to_str(shabin, shastr);
		shastr[HASH_SIZE] = '\0';

		/* Determine the type */
		if (loose_content_handler(shastr, get_type_loose_cb, &type))
			pack_content_handler(shastr, get_type_pack_cb, &type);

		if (tree_handler)
			tree_handler(mode, type, shastr, filename, args);

		/* Skip over the binary sha */
		offset = offset + 20;
	}

	free(decompressed_object.data);
}

int
git_repository_path()
{
	char *d = NULL;
	int s;
	char path[PATH_MAX];
	struct stat sb;
	int ret = 1;

	d = getcwd(d, PATH_MAX);
	if (d == NULL) {
		fprintf(stderr, "Unable to get current directory: %s\n", strerror(errno));
		exit(errno);
	}

	/* XXX Is there a better way to do this? */

	while (strncmp(d, "/\0", 2) != 0) {
		snprintf(path, sizeof(path), "%s/.git", d);
		s = stat(path, &sb);
		if (s == -1) {
			d = dirname(d);
			if (d == NULL) {
				ret = -1;
				break;
			}
			continue;
		}
		else if (s == 0) {
			ret = 0;
			strlcpy(dotgitpath, path, sizeof(dotgitpath));
			break;
		}
	};

	free(d);
	return (ret);
}

void
update_branch_pointer(char repodir, char *ref, char *sha)
{
	FILE *reffile;

	reffile = fopen(ref, "w");
	if (reffile == NULL) {
		exit(-1);
	}
}

/*
 * Description: Converts a SHA in binary format to a string
 * Arguments: 1) pointer to the SHA, 2) str is a char[HASH_SIZE]
 * which stores the output string
 */
void
sha_bin_to_str(uint8_t *bin, char *str)
{
	int x;
	for(x=0;x<HASH_SIZE/2;x++) {
		str[(x*2)] = bin[x] >> 4;
		str[(x*2)+1] = bin[x] & 0x0f;
	}

	for(x=0;x<HASH_SIZE;x++)
		if (str[x] <= 9)
			str[x] = str[x] + '0';
		else
			str[x] = str[x] + 87;
}
