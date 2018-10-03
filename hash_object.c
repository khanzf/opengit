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
	{NULL, 0, NULL, 0}
};

int
hash_object_usage(int type)
{
	printf("usage: ogit hash-object\n");
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
hash_object_create_zlib(FILE *source, char *header, char *zlib)
{
	int ret, flush;
	z_stream strm;
	unsigned int have;
	unsigned char in[Z_CHUNK];
	unsigned char out[Z_CHUNK];

	FILE *dest = stdout; // This is a temporary test

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
//	ret = deflateInit(&strm, Z_BEST_SPEED);
	ret = deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK)
		return ret;

	printf("Header: %s\n", header);

	/* Beginning of writing the header */

	strm.next_in = (unsigned char *) header;
	strm.avail_in = strlen(header) + 1;

	do {
		strm.avail_out = Z_CHUNK;
		strm.next_out = out;
		if (deflate (& strm, Z_FINISH) < 0) {
			fprintf(stderr, "returned a bad status of.\n");
			exit(0);
		}
		have = Z_CHUNK - strm.avail_out;
		fwrite(out, 1, have, stdout);
	} while(strm.avail_out == 0);
	/* End of writing the header */





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
}

int
hash_object_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	FILE *fileptr;
	char *filepath;
	char checksum[HEX_DIGEST_LENGTH];
	char header[32];
	char *zlib;

	//uint8_t flags = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1)
		switch(ch) {
		default:
			break;
		}

	if (argc != 2) {
		printf("Future functionality not yet implemented\n");
		return -1;
	}

	filepath = argv[1];
	fileptr = fopen(filepath, "r");

	hash_object_create_header(filepath, (char *)&header);
	hash_object_compute_checksum(fileptr,(char *)&checksum,(char *)&header);
	hash_object_create_zlib(fileptr, (char *)&header, (char *)&header);

//	printf("%s\n", checksum);
//	printf("Filepath: %s\n", filepath);

	return (ret);
}
