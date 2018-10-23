#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include "cat_file.h"
#include "ini.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

int
cat_file_usage(int type)
{
	fprintf(stderr, "usage: git cat-file (-t [--allow-unknown-type] | -s [--allow-unknown-type] | -e | -p | <type> | --textconv | --filters) [--path=<path>] <object>\n");
	fprintf(stderr, "\n");
	exit(type);
}

int
cat_file_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;
	char *sha;

	int8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "t:", long_options, NULL)) != -1)
		switch(ch) {
		case 't':
			argc--;
			argv++;
			sha = argv[1];
			printf("The target is: %s\n", sha);
			break;
		default:
			printf("cat-file: Currently not implemented\n");
			cat_file_usage(0);
		}

	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}

	printf("The argument for cat-file: %s\n", argv[1]);

	argc = argc - q;
	argv = argv + q;

	test_test();

	return (ret);
}

