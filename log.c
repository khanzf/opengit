#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sha.h>
#include "log.h"
#include "common.h"
#include "ini.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

int
log_usage(int type)
{
	fprintf(stderr, "usage: git log [<options>] [<revision-range>] [[--] <path>...]\n");
	fprintf(stderr, "   or: git show [<options>] <object>...\n\n");
	fprintf(stderr, "    -q, --quiet           suppress diff output\n");
	fprintf(stderr, "    --source              show source\n");
	fprintf(stderr, "    --use-mailmap         Use mail map file\n");
	fprintf(stderr, "    --decorate-refs <pattern>\n");
	fprintf(stderr, "                          only decorate refs that match <pattern>\n");
	fprintf(stderr, "    --decorate-refs-exclude <pattern>\n");
	fprintf(stderr, "                          do not decorate refs that match <pattern>\n");
	fprintf(stderr, "    --decorate[=...]      decorate options\n");
	fprintf(stderr, "    -L <n,m:file>         Process line range n,m in file, counting from 1\n\n");
	exit(129);
	return 129;
}

void
log_display_commits()
{
	printf("Log commits\n");
}

int
log_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1)
		switch(ch) {
		case 0:
			printf("0\n");
			break;
		case 1:
			printf("1\n");
			break;
		default:
			printf("Currently not implemented\n");
			return -1;
		}
	argc = argc - q;
	argv = argv + q;

	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}
	config_parser();

	log_display_commits();

	return (ret);
}

