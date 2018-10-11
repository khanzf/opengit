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
#include <zlib.h>
#include "common.h"
#include "index.h"
#include "ini.h"

static struct option long_options[] =
{
	{"add", no_argument, NULL, 0},
	{"cacheinfo", required_argument, NULL, 1},
	{NULL, 0, NULL, 0}
};

int
update_index_usage(int type)
{
	fprintf(stderr, "usage: git update-index [<options>] [--] [<file>...]\n");
	return 0;
}

int
update_index_open_index(FILE **indexptr)
{
	char indexpath[PATH_MAX];
	sprintf(indexpath, "%s/index", dotgitpath);

	printf("File: %s\n", indexpath);

	*indexptr = fopen(indexpath, "rw");

	return 0;
}

int
update_index_parse(FILE **indexptr)
{
	int i;
	struct _cache_hdr *cache_hdr;

	struct _index_hdr *index_hdr;
	struct _extension_hdr *extension_hdr;

	unsigned char *indexmap;
	int offset;
	struct stat sb;

	unsigned int flags;

	fstat(fileno(*indexptr), &sb);

	indexmap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fileno(*indexptr), 0);
	if (indexmap == MAP_FAILED) {
		fprintf(stderr, "mmap failed on index file\n");
		exit(1);
	}

	cache_hdr = (struct _cache_hdr *)indexmap;
	offset = sizeof(*cache_hdr);

	printf("Offset is: %d\n", offset);

	printf("File size: %d\n", sb.st_size);

	if (strncmp(cache_hdr->sig, "DIRC", 4)) {
		fprintf(stderr, "Bad index signature\n");
		exit(-1);
	}

	printf("Signature:\t%s\n", cache_hdr->sig);
	printf("Version:\t%d\n", ntohl(cache_hdr->version));
	printf("Entries:\t%d\n", ntohl(cache_hdr->entries));

#define CE_EXTENDED  (0x4000)

	for(i=0;i<ntohl(cache_hdr->entries);i++) {
		index_hdr = (struct _index_hdr *)((char *)indexmap + offset);

		flags = ntohs(index_hdr->flags);

		if (flags & CE_EXTENDED) {
			extension_hdr = (struct _extension_hdr *)indexmap + offset;
			/* 72 is calculated as offsetof(struct _extension_hdr, name) + 8 bytes */
			offset += (72 + strlen(extension_hdr->name)) & ~0x7;
		}
		else {
			/* 70 is calculated as offsetof(struct _index_hdr, name) + 8 bytes */
			offset += (70 + strlen(index_hdr->name)) & ~0x7;
		}
	}
	return 0;
}

int
update_index_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;

	int8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, ":", long_options, NULL)) != -1)
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

	FILE *indexptr;

	printf("update_index\n");
	update_index_open_index(&indexptr);
	update_index_parse(&indexptr);

	return (ret);
}

