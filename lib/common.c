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
#include <regex.h>

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

char repodir[PATH_MAX];
char dotgitpath[PATH_MAX];

const char *object_name[] = {
	NULL,
	"commit",
	"tree",
	"blob",
	"tag",
	NULL,
	"obj_ofs_delta",
	"obj_ref_delta"
};

/*
 * Description: Recovers a tree object and iterates through it. It will also
 * identify the object type.
 * Arguments: 1) treesha is a char[HASH_SIZE]
 * 2) tree_handler is what the function should do with the resultant data
 */
void
iterate_tree(struct decompressed_object *decompressed_object, tree_handler tree_handler, void *args)
{
	long offset = 0;
	long space;
	uint8_t *shabin;
	char shastr[HASH_SIZE+1], mode[7];
	char *filename;
	uint8_t type;

	while(offset<decompressed_object->size) {
		/* Get the file mode */
		strlcpy(mode, (char *)decompressed_object->data+offset, 7);
		if (mode[5] != ' ')
			space = 6;
		else
			space = 5;
		offset = offset + space;
		mode[space] = '\0';

		filename = (char *)decompressed_object->data+offset+1;
		offset = offset + strlen((char *)decompressed_object->data+offset) + 1;

		/* Get the SHA, convert it to a string */
		shabin = decompressed_object->data + offset;
		sha_bin_to_str(shabin, shastr);
		shastr[HASH_SIZE] = '\0';

		/* Determine the type */
		CONTENT_HANDLER(shastr, get_type_loose_cb, get_type_pack_cb, &type);

		if (tree_handler)
			tree_handler(mode, type, shastr, filename, args);

		/* Skip over the binary sha */
		offset = offset + 20;
	}

	free(decompressed_object->data);
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

/*
 * Description: Convert a binary format to a string
 * Arguments: 1) char[HASH_SIZE] source, 2) Pointer to SHA output
 * which stores the output binary hash
 */
void
sha_str_to_bin(char *str, uint8_t *bin)
{
	for(int x=0;x<HASH_SIZE;x=x+2) {
		if (str[x] >= '0' && str[x] <= '9')
			bin[x/2] = (str[x] - '0') & 0x0f;
		else if (str[x] >= 'a' && str[x] <= 'f')
			bin[x/2] = (str[x] - 'a' + 10) & 0x0f;

		if (str[x+1] >= '0' && str[x+1] <= '9')
			bin[x/2] |= (str[x+1] - '0') << 4;
		else if (str[x+1] >= 'a' && str[x+1] <= 'f')
			bin[x/2] |= (str[x+1] - 'a' + 10) << 4;
	}
}

void
sha_str_to_bin_network(char *str, uint8_t *bin)
{
	for(int x=0;x<HASH_SIZE;x=x+2) {
		if (str[x] >= '0' && str[x] <= '9')
			bin[x/2] = ((str[x] - '0') << 4) & 0xf0;
		else if (str[x] >= 'a' && str[x] <= 'f')
			bin[x/2] = ((str[x] - 'a' + 10) << 4) & 0xf0;

		if (str[x+1] >= '0' && str[x+1] <= '9')
			bin[x/2] |= (str[x+1] - '0') & 0x0f;
		else if (str[x+1] >= 'a' && str[x+1] <= 'f')
			bin[x/2] |= (str[x+1] - 'a' + 10) & 0x0f;
	}
}

/* Description: Returns number of digits in integer */
int
count_digits(int check)
{
	int digits = 1;
	while(check /= 10)
		digits++;
	return (digits);
}

/*
 * Parse a commit message header
 * ToFree: Use free_commithead()
 */
void
populate_commitheader(struct commitheader *commitheader, char *header, long len)
{
	char *token, *tofree, *string;
	int type = 0;
	int size;
	regex_t re;
	regmatch_t m[5];

	regcomp(&re, "[author|committer] (.*) <(.*)> ([0-9]*) (-?[0-9]*)", REG_EXTENDED);

	commitheader->numparent = 0;
	commitheader->parent = NULL;

	tofree = string = strndup(header, len);

	while((token = strsep(&string, "\n")) != NULL) {
		if (type == 0 || token[0] != ' ') {
			if (!strncmp(token, "tree ", 5)) {
				printf("TREE\n");
				strlcpy(commitheader->treesha, token + 5, HASH_SIZE+1);
			}
			else if (!strncmp(token, "parent ", 7)) {
				printf("PARENT\n");
				commitheader->parent = realloc(commitheader->parent,
				    sizeof(char *) * (commitheader->numparent+1));
				commitheader->parent[commitheader->numparent] = malloc(HASH_SIZE+1);
				strlcpy(commitheader->parent[commitheader->numparent], token+7, HASH_SIZE+1);
				commitheader->numparent++;
			}
			else if (!strncmp(token, "author ", 7)) {
				printf("AUTHOR\n");
				if (regexec(&re, token, 5, m, 0) == REG_NOMATCH) {
					fprintf(stderr, "Author not matched\n");
					exit(0);
				}
				commitheader->author_name = strndup(token + m[1].rm_so, m[1].rm_eo - m[1].rm_so);
				printf("Author Name: %s\n", commitheader->author_name);
				commitheader->author_email = strndup(token + m[2].rm_so, m[2].rm_eo - m[2].rm_so);
				printf("Author Email: %s\n", commitheader->author_email);
				commitheader->author_time = atol(token + m[3].rm_so);
				printf("Author Time: %lu\n", commitheader->author_time);
				commitheader->author_tz = strndup(token + m[4].rm_so, m[4].rm_eo - m[4].rm_so);
				printf("Author Tz: %s\n", commitheader->author_tz);
			}
			else if (!strncmp(token, "committer ", 10)) {
				printf("COMMITTER\n");
				if (regexec(&re, token, 5, m, 0) == REG_NOMATCH) {
					fprintf(stderr, "Committer not matched\n");
					exit(0);
				}
				commitheader->committer_name = strndup(token + m[1].rm_so, m[1].rm_eo - m[1].rm_so);
				commitheader->committer_email = strndup(token + m[2].rm_so, m[2].rm_eo - m[2].rm_so);
				commitheader->committer_time = atol(token + m[3].rm_so);
				commitheader->committer_tz = strndup(token + m[4].rm_so, m[4].rm_eo - m[4].rm_so);
			}
			else if (!strncmp(token, "gpgsig ", 7)) {
				printf("GPGSIG\n");
				type = HEADER_GPGSIG;
				commitheader->gpgsig = strdup(token + 7);
				size = strlen(token + 7);
			}
		}
		else if (type == HEADER_GPGSIG) {
			commitheader->gpgsig = realloc(commitheader->gpgsig, strlen(token) + size);
			commitheader->gpgsig[size] = '\n';
			strncpy(commitheader->gpgsig + size + 1, token + 1, strlen(token));
			size += strlen(token) + 1;
		}
	}

	regfree(&re);
	free(tofree);
}

/* Frees a commitheader structure */
void
free_commitheader(struct commitheader *commitheader)
{
	free(commitheader->author_name);
	free(commitheader->author_email);
	free(commitheader->author_tz);

	free(commitheader->committer_name);
	free(commitheader->committer_email);
	free(commitheader->committer_tz);

	for(int x=0;x>commitheader->numparent;x++)
		free(commitheader->parent[x]);
	free(commitheader->parent);

	free(commitheader->gpgsig);
}
