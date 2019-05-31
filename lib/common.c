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

/*
	Gets the path of the .git directory
	If found, returns 0 and sets dotgitpath
	If not found, returns 1
	If errors, returns -1
*/

char dotgitpath[PATH_MAX];

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
		sprintf(path, "%s/.git", d);
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
			strncpy(dotgitpath, path, strlen(path));
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
