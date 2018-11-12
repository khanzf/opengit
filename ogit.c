#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "ogit.h"
#include "init.h"
#include "common.h"
#include "hash_object.h"
#include "update_index.h"
#include "cat_file.h"

static struct cmd cmds[] = {
	{"remote",		remote_main},
	{"init",		init_main},
	{"hash-object",		hash_object_main},
	{"update-index",	update_index_main},
	{"cat-file",		cat_file_main},
	{"log",			log_main}
};

int cmd_count = nitems(cmds);

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
		if (strncmp(cmds[ch].c_arg, argv[1], strlen(cmds[ch].c_arg)) == 0) {
			break;
		}
	}

	if (ch == cmd_count)
		usage();

	cmds[ch].c_func(argc, argv);
}
