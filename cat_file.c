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
cat_file_get_content(char *sha, uint8_t flags)
{
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
	struct objectinfo *objectinfo = idxmap + offset;

	// Used for size
	unsigned long size;
	unsigned long *sevenbit;
	unsigned long used = 0;


        sevenbit = idxmap + offset;
        used++;
        size = *sevenbit & 0x0F;

	/*
		The next 'while' block calculates the object size
	        Git's size count is all sorts of screwy
	        In short, we are adding up the value of the lower 4 bits.
	        If the highest bit of the lower 8-bits is 1, then look at the next 8 bits
	        Those lower 4 bits are shifted over by 4 + (7 times the iteration we are on).
	        And this value is added to the size total.
	*/
        while(*sevenbit & 0x80) {
                sevenbit = idxmap + offset + used;
                size += (*sevenbit & 0x7F) << (4 + (7*(used-1)));
                used++;
        }

	switch(flags) {
		case CAT_FILE_PRINT:
			pack_uncompress_object(idxmap, size, offset + used);
			break;
		case CAT_FILE_SIZE:
			printf("%lu\n", size);
			break;
		case CAT_FILE_TYPE:
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
			break;
	}
}

int
cat_file_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	char *sha;
	uint8_t flags = 0;
	unsigned char sha_bin[20];
	int i;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "p:t:s:", long_options, NULL)) != -1)
		switch(ch) {
		case 'p':
			argc--;
			argv++;
			sha = argv[1];
			flags = CAT_FILE_PRINT;
			break;
		case 't':
			argc--;
			argv++;
			sha = argv[1];
			flags = CAT_FILE_TYPE;
			break;
		case 's':
			argc--;
			argv++;
			sha = argv[1];
			flags = CAT_FILE_SIZE;
			break;
		default:
			printf("cat-file: Currently not implemented\n");
			cat_file_usage(0);
		}

	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}

	for(i=0;i<20;i++)
		sscanf(sha+i*2, "%2hhx", &sha_bin[i]);

	switch(flags) {
		case CAT_FILE_PRINT:
		case CAT_FILE_TYPE:
		case CAT_FILE_SIZE:
			cat_file_get_content(sha_bin, flags);
	}

	return (ret);
}

