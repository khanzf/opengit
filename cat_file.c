#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <fcntl.h>
#include "cat_file.h"
#include "common.h"
#include "pack.h"
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

void
cat_file_get_content_type(char *sha) {
	// Find the sha or fail
	DIR *d;
	struct dirent *dir;
	char packdir[PATH_MAX];
	char idxfile[PATH_MAX];
	int packfd;
	char *file_ext;
	char *idxmap;
	struct stat sb;
	int offset = 0;

	sprintf(packdir, "%s/objects/pack", dotgitpath);
	d = opendir(packdir);

	/* Find hash in idx file or die */
	if (d) {
		while((dir = readdir(d)) != NULL) {
			file_ext = strrchr(dir->d_name, '.');
			if (!file_ext || strncmp(file_ext, ".idx", 4))
				continue;
			sprintf(idxfile, "%s/objects/pack/%s", dotgitpath, dir->d_name);

			packfd = open(idxfile, O_RDONLY);
			fstat(packfd, &sb);
			idxmap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, packfd, 0);

			if (idxmap == NULL) {
				fprintf(stderr, "mmap(2) error, exiting.\n");
				exit(0);
			}
			close(packfd);

			offset = pack_find_sha_offset(sha, idxmap);

			munmap(idxmap, sb.st_size);

			if (offset != -1)
				break;
		}
	}

	if (offset == -1) {
		fprintf(stderr, "fatal: git cat-file: could not get object info\n");
		exit(128);
	}

	/* Pack file part */
	int version;
	int nobjects;
	int loc;

	strncpy(idxfile+strlen(idxfile)-4, ".pack", 6);

	packfd = open(idxfile, O_RDONLY);
	if (packfd == -1) {
		fprintf(stderr, "fatal: git cat-file: could not get object info\n");
		exit(128); // XXX replace 128 with proper return macro value
	}
	fstat(packfd, &sb);

	idxmap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, packfd, 0);
	if (idxmap == MAP_FAILED) { // XXX Should this be MAP_FAILED?
		fprintf(stderr, "mmap(2) error, exiting.\n");
		exit(0);
	}
	close(packfd);

	if (memcmp(idxmap, "PACK", 4)) {
		fprintf(stderr, "error: file %s is not a GIT packfile\n");
		fprintf(stderr, "error: bad object HEAD\n");
		exit(128);
	}

	loc = 4;
	version = *(idxmap + loc + 3);
	if (version != 2) {
		fprintf(stderr, "error: unsupported version: %d\n", version);
		exit(128); // XXX replace 128 with proper return macro value
	}

	loc += 4;
	nobjects = *(idxmap + loc + 3);

	// Classify Object

	struct objectinfo *objectinfo = idxmap+offset;

	switch (objectinfo->type) {
	case OBJ_COMMIT:
		printf("commit\n"); break;
	case OBJ_TREE:
		printf("tree\n"); break;
	case OBJ_BLOB:
		printf("blob\n"); break;
	case OBJ_TAG:
		printf("tag\n"); break;
	case OBJ_OFS_DELTA:
		printf("OFS Delta\n"); break;
	case OBJ_REF_DELTA:
		printf("REF Delta\n"); break;
	default:
		printf("Default case\n"); break;
	}

	char *v;
	int q;
	v = idxmap + loc + 4;
}

int
cat_file_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;
	char *sha;

	uint8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "p:t:", long_options, NULL)) != -1)
		switch(ch) {
		case 'p':
			argc--;
			argv++;
			sha = argv[1];
			flags |= CAT_FILE_PRINT;
			break;
		case 't':
			argc--;
			argv++;
			sha = argv[1];
			flags |= CAT_FILE_TYPE;
			break;
		default:
			printf("cat-file: Currently not implemented\n");
			cat_file_usage(0);
		}

	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}

	if (flags & CAT_FILE_TYPE) {
		unsigned char sha_hex[20];
		int i;
		for(i=0;i<20;i++)
			sscanf(sha+i*2, "%2hhx", &sha_hex[i]);

		cat_file_get_content_type(sha_hex);
	}
	else if (flags & CAT_FILE_PRINT) {
		printf("Unimplemented -p option\n");
		exit(0);
	}

	argc = argc - q;
	argv = argv + q;

/*
struct fan *fans;
struct entry *entries;
struct checksum *checksums;
struct offset *offsets;
*/

//	pack_pointer(fans, entries, checksums, offsets);

	return (ret);
}

