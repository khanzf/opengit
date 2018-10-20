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
#include "hash_object.h"
#include "common.h"
#include "ini.h"

static struct option long_options[] =
{
	{"stdin", no_argument, NULL, 0},
	{"stdin-paths", no_argument, NULL, 0},
	{"no-filters", no_argument, NULL, 0},
	{"literally", no_argument, NULL, 0},
	{"path", required_argument, NULL, 0},
	{NULL, 0, NULL, 0}
};

int
hash_object_usage(int type)
{
	fprintf(stderr, "usage: git hash-object [-t <type>] [-w] [--path=<file> | --no-filters] [--stdin] [--] <file>...\n");
	fprintf(stderr, "   or: git hash-object  --stdin-paths\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -t <type>\t\twrite the object into the object database\n");
	fprintf(stderr, "    -w\t\t\twrite the object into the object database\n");
	fprintf(stderr, "    --stdin\t\tread the object from stdin\n");
	fprintf(stderr, "    --stdin-paths\tread file names from stdin\n");
	fprintf(stderr, "    --no-filters\tstore file as is without filters\n");
	fprintf(stderr, "    --literally\t\tjust hash any random garbage to create corrupt objects for debugging Git\n");
	fprintf(stderr, "    --path <file>\tprocess file as it were from this path\n");
	fprintf(stderr, "\n");
	return 0;
}

int
hash_object_compute_checksum(FILE *source, char *checksum, char *header)
{
	SHA1_CTX context;
	char in[4096];
	int l;

	SHA1_Init(&context);
	SHA1_Update(&context, header, strlen(header)+1);

	while((l = fread(&in, 1, 4096, source)) != -1 && l != 0) {
		SHA1_Update(&context, in, l);
	}
	SHA1_End(&context, checksum);

	/* Reset the stream */
	fseek(source, 0, SEEK_SET);

	return 0;
}

int
hash_object_create_header(char *filepath, char *header)
{
	struct stat sb;
	int l;
	stat(filepath, &sb);

	l = sprintf(header, "blob %ld", sb.st_size);

	return l;
}

int
hash_object_create_zlib(FILE *source, FILE *dest, char *header, char *checksum)
{
	int ret, flush;
	z_stream strm;
	int have;
	char in[Z_CHUNK];
	char out[Z_CHUNK];
	char filepath[PATH_MAX + NAME_MAX];

	sprintf(filepath, "%s/objects/%c%c", dotgitpath, checksum[0], checksum[1]);

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = deflateInit(&strm, Z_BEST_SPEED);
	if (ret != Z_OK)
		return ret;

	/* Beginning of writing the header */
	strm.next_in = (char *) header;
	strm.avail_in = strlen(header) + 1;

	do {
		strm.avail_out = Z_CHUNK;
		strm.next_out = out;
		if (deflate (& strm, Z_NO_FLUSH) < 0) {
			fprintf(stderr, "returned a bad status of.\n");
			exit(0);
		}
		have = Z_CHUNK - strm.avail_out;
		fwrite(out, 1, have, dest);
	} while(strm.avail_out == 0);

	/* Beginning of file content */
	do {
		strm.avail_in = fread(in, 1, Z_CHUNK, source);
		if (ferror(source)) {
			(void)deflateEnd(&strm);
			return Z_ERRNO;
		}

		flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = in;

		do {
			strm.avail_out = Z_CHUNK;
			strm.next_out = out;
			ret = deflate(&strm, flush);
			have = Z_CHUNK - strm.avail_out;
			if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
				(void)deflateEnd(&strm);
				return Z_ERRNO;
			}
		} while(strm.avail_out == 0);

	} while (flush != Z_FINISH);

	return 0;
}

int
hash_object_create_file(FILE **objectfileptr, char *checksum)
{
	char objectpath[PATH_MAX];

	// First create the directory
	sprintf(objectpath, "%s/objects/%c%c",
	    dotgitpath, checksum[0], checksum[1]);

	mkdir(objectpath, 0755);

	// Reusing objectpath variable
	sprintf(objectpath, "%s/objects/%c%c/%s",
	    dotgitpath, checksum[0], checksum[1], checksum+2);

	*objectfileptr = fopen(objectpath, "w");

	return 0;
}

int
hash_object_write(char *filearg, uint8_t flags)
{
	int ret = 0;
	FILE *fileptr;
	FILE *objectfileptr = NULL;
	char checksum[HEX_DIGEST_LENGTH];
	char header[32];

	git_repository_path();

	fileptr = fopen(filearg, "r");

	hash_object_create_header(filearg, (char *)&header);
	hash_object_compute_checksum(fileptr,(char *)&checksum,(char *)&header);
	hash_object_create_file(&objectfileptr, (char *) &checksum);
	if (flags & CMD_HASH_OBJECT_WRITE)
		hash_object_create_zlib(fileptr, objectfileptr, (char *)&header, (char *)&checksum);

	printf("%s\n", checksum);
	return (ret);
}

int
hash_object_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;

	int8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "wt:", long_options, NULL)) != -1)
		switch(ch) {
		case 'w':
			flags |= CMD_HASH_OBJECT_WRITE; 
			q++;
			break;
		default:
			printf("Currently not implemented\n");
			hash_object_usage(0);
			return -1;
		}
	argc = argc - q;
	argv = argv + q;

	if (argv[1])
		ret = hash_object_write(argv[1], flags);
	else
		return hash_object_usage(0);

	return (ret);
}

