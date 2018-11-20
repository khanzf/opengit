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
#include <zlib.h>
#include "zlib_handler.h"
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
cat_file_get_content(char *sha_str, uint8_t flags)
{
	// Check if the loose file exists. If not, check the pack files
	if (cat_file_get_content_loose(sha_str, flags) == 0)
		cat_file_get_content_pack(sha_str, flags);
}

void
cat_file_print_type_by_id(int object_type)
{
	switch(object_type) {
	case OBJ_COMMIT:
		printf("commit\n");
		break;
	case OBJ_TREE:
		printf("tree\n");
		break;
	case OBJ_BLOB:
		printf("blob\n");
		break;
	case OBJ_TAG:
		printf("tag\n");
		break;
	case OBJ_OFS_DELTA:
		printf("obj_ofs_delta\n");
		break;
	case OBJ_REF_DELTA:
		printf("obj_ref_delta\n");
		break;
	default:
		fprintf(stderr, "Unknown type, exiting.\n");
		exit(1);
	}
}

int
cat_file_get_loose_headers(unsigned char *buf, int size, void *arg)
{
	struct loosearg *loosearg = arg;
	unsigned char *endptr;
	int hdr_offset = 0;

	// Is there a cleaner way to do this?
	if (!memcmp(buf, "commit ", 7)) {
		loosearg->type = OBJ_COMMIT;
		loosearg->size = strtol((char *)buf + 7, (char **)&endptr, 10);
		hdr_offset = 8 + (endptr - (buf+7));
	}
	else if (!memcmp(buf, "tree ", 5)) {
		loosearg->type = OBJ_TREE;
		loosearg->size = strtol((char *)buf + 5, (char **)&endptr, 10);
		hdr_offset = 6 + (endptr - (buf+5));
	}
	else if (!memcmp(buf, "blob ", 5)) {
		loosearg->type = OBJ_BLOB;
		loosearg->size = strtol((char *)buf + 5, (char **)&endptr, 10);
		hdr_offset = 6 + (endptr - buf);
	}
	else if (!memcmp(buf, "tag", 3)) {
		loosearg->type = OBJ_TAG;
		hdr_offset = 3;
	}
	else if (!memcmp(buf, "obj_ofs_delta", 13)) {
		loosearg->type = OBJ_REF_DELTA;
		hdr_offset = 15;
	}
	else if (!memcmp(buf, "obj_ref_delta", 13)) {
		loosearg->type = OBJ_REF_DELTA;
		hdr_offset = 15;
	}

	return hdr_offset;
}

unsigned char *
cat_loose_object_cb(unsigned char *buf, int size, void *arg)
{
	struct loosearg *loosearg = arg;
	int hdr_offset;

	if (loosearg->step == 0) {
		loosearg->step++;
		hdr_offset = cat_file_get_loose_headers(buf, size, arg);
		switch(loosearg->cmd) {
		case CAT_FILE_TYPE:
			cat_file_get_loose_headers(buf, size, arg);
			cat_file_print_type_by_id(loosearg->type);
			return NULL;
		case CAT_FILE_SIZE:
			printf("%lu\n", loosearg->size);
			return NULL;
		case CAT_FILE_PRINT:
			size -= hdr_offset;
			buf += hdr_offset;
			break;
		}
	}

	write(loosearg->fd, buf, size);

	return buf;
}

int
cat_file_get_content_loose(char *sha_str, uint8_t flags)
{
	char objectpath[PATH_MAX];
	int objectfd;
	struct loosearg loosearg;

	sprintf(objectpath, "%s/objects/%c%c/%s", dotgitpath, sha_str[0], sha_str[1], sha_str+2);
	objectfd = open(objectpath, O_RDONLY);
	if (objectfd == -1)
		return 0;

	loosearg.fd = STDOUT_FILENO;
	loosearg.cmd = flags;
	loosearg.step = 0;
	loosearg.sent = 0;

	deflate_caller(objectfd, cat_loose_object_cb, &loosearg);

	return 1;
}

void
cat_file_get_content_pack(char *sha_str, uint8_t flags)
{
	DIR *d;
	struct dirent *dir;
	char packdir[PATH_MAX];
	char idxfile[PATH_MAX];
	int packfd;
	char *file_ext;
	unsigned char *idxmap;
	struct stat sb;
	int offset = 0;
	uint8_t sha_bin[20];
	int i;

	for (i=0;i<20;i++)
		sscanf(sha_str+i*2, "%2hhx", &sha_bin[i]);

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

			offset = pack_find_sha_offset(sha_bin, idxmap);

			munmap(idxmap, sb.st_size);

			if (offset != -1)
				break;
		}
	}

	// Process CAT_FILE_EXIT here
	if (flags == CAT_FILE_EXIT) {
		if (offset == -1)
			exit(1);
		else
			exit(0);
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

	if (memcmp(idxmap, "PACK", 4)) {
		fprintf(stderr, "error: file %s is not a GIT packfile\n", idxfile);
		fprintf(stderr, "error: bad object HEAD\n");
		exit(128);
	}

	loc = 4;
	version = *(idxmap + loc + 3);
	if (version != 2) {
		fprintf(stderr, "error: unsupported version: %d\n", version);
		// XXX replace 128 with proper return macro value
		exit(128);
	}

	loc += 4;
	nobjects = *(idxmap + loc + 3);

	// Classify Object
	struct objectinfohdr *objectinfohdr = (struct objectinfohdr *)(idxmap + offset);

	// Used for size
	unsigned long size;
	unsigned long *sevenbit;
	unsigned long used = 0;

        sevenbit = (unsigned long *)(idxmap + offset);
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
                sevenbit = (unsigned long *)(idxmap + offset + used);
                size += (*sevenbit & 0x7F) << (4 + (7*(used-1)));
                used++;
        }

	lseek(packfd, offset + used, SEEK_CUR);

	switch(flags) {
		case CAT_FILE_PRINT:
			pack_uncompress_object(packfd);
			break;
		case CAT_FILE_SIZE:
			printf("%lu\n", size);
			break;
		case CAT_FILE_TYPE:
			cat_file_print_type_by_id(objectinfohdr->type);
			break;
	}
}

int
cat_file_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	char *sha_str;
	uint8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "p:t:s:", long_options, NULL)) != -1)
		switch(ch) {
		case 'p':
			argc--;
			argv++;
			sha_str = argv[1];
			flags = CAT_FILE_PRINT;
			break;
		case 't':
			argc--;
			argv++;
			sha_str = argv[1];
			flags = CAT_FILE_TYPE;
			break;
		case 's':
			argc--;
			argv++;
			sha_str = argv[1];
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

	switch(flags) {
		case CAT_FILE_PRINT:
		case CAT_FILE_TYPE:
		case CAT_FILE_SIZE:
			cat_file_get_content(sha_str, flags);
	}

	return (ret);
}

