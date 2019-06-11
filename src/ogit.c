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
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "lib/common.h"
#include "update-index.h"
#include "hash-object.h"
#include "index-pack.h"
#include "cat-file.h"
#include "clone.h"
#include "ogit.h"
#include "init.h"

static struct cmd cmds[] = {
	{"remote",		remote_main},
	{"init",		init_main},
	{"hash-object",		hash_object_main},
	{"update-index",	update_index_main},
	{"cat-file",		cat_file_main},
	{"log",			log_main},
	{"clone",		clone_main},
	{"index-pack",		index_pack_main}
};

bool color = true;

int cmd_count = nitems(cmds);

void
parse_color_opt(const char *optarg)
{

	if (optarg == NULL || strcasecmp(optarg, "always") == 0)
		color = true;
	else if (strcasecmp(optarg, "never") == 0)
		color = false;
	else if (strcasecmp(optarg, "auto") == 0)
		color = isatty(STDOUT_FILENO);
}

void
usage()
{
	printf("Usage statement goes here\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	int ch;

	if (argc == 1)
		usage();

	for (ch = 0; ch < cmd_count; ch++) {
		if (strncmp(cmds[ch].c_arg, argv[1], strlen(cmds[ch].c_arg)) == 0 &&
		    strncmp(cmds[ch].c_arg, argv[1], strlen(argv[1])) == 0) {
			break;
		}
	}

	if (ch == cmd_count)
		usage();

	cmds[ch].c_func(argc, argv);
}
