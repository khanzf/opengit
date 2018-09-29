#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "ogit.h"
#include "ini.h"

static struct cmd cmds[] = {
	{"remote",	remote_main}
};

int cmd_count = 1;

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
