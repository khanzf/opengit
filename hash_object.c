#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "ini.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

int
hash_object_usage(int type)
{
	printf("usage: ogit hash-object\n");
	return 0;
}

int
hash_object_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;

	//uint8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1)
		switch(ch) {
		default:
			break;
		}

	return (ret);
}
