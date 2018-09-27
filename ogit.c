#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "ogit.h"

static struct cmd cmds[] = {
};

int cmd_count = 0;

void
usage()
{
	printf("Usage goes here\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	int ch;
	int option_index = 0;

	if (argc == 1)
		usage();

	for (ch = 0; ch < cmd_count; ch++) {
		if (strncmp(cmds[ch].c_arg, argv[1], strlen(cmds[ch].c_arg)) == 0) {
			break;
		}
	}

	if (ch == cmd_count)
		usage();

	cmds[ch].c_func();

}
