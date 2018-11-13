#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include "pack.h"
#include "common.h"
#include "ini.h"

#include <netinet/in.h>

#define CHUNK 16384

unsigned char *
pack_uncompress_object(unsigned char *idxmap, unsigned long size, int offset)
{
	z_stream strm;
	int chunk_size = 0, remaining_size, consumed = 0;
	unsigned char *content;

	unsigned char out[CHUNK];
	unsigned have;
	int ret;

	remaining_size = size;
	content = malloc(size);

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
		exit(ret);

	/* decompress until deflate stream ends or end of file */
	do {
		if (remaining_size >= CHUNK) {
			strm.avail_in = CHUNK;
			remaining_size -= CHUNK;
		}
		else {
			strm.avail_in = remaining_size;
			remaining_size = 0;
		}
		if (strm.avail_in == 0)
			break;

		strm.next_in = idxmap + offset + consumed;
		consumed = consumed + chunk_size;

		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
//			assert(ret != Z_STREAM_ERROR);
			switch(ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				exit(ret);
			}
			have = CHUNK - strm.avail_out;
			fwrite(out, 1, have, stdout);

		} while(strm.avail_out == 0);

	} while (ret != Z_STREAM_END);

	return content;
}

int
pack_get_pack_get_content_type(unsigned char *idxmap, int offset)
{
	return (idxmap[offset] >> 4) & 7;

}

int
pack_find_sha_offset(unsigned char *sha, unsigned char *idxmap)
{
	struct fan *fans;
	struct entry *entries;
	struct checksum *checksums;
	struct offset *offsets;
	int idx_offset;
	char idx_version;
	int nelements;
	int n;

	if (memcmp(idxmap, "\xff\x74\x4f\x63", 4)) {
		fprintf(stderr, "Header signature does not match index version 2.\n");
		exit(0);
	}
	idx_offset = 4;

	idx_version = *(idxmap + idx_offset+3);
	if (idx_version != 2) {
		fprintf(stderr, "opengit currently only supports version 2. Exiting.\n");
		exit(0);
	}
	idx_offset += 4;

	// Get the fan table and capture last element
	fans = (struct fan *)(idxmap + idx_offset);
	nelements = ntohl(fans->count[255]);
	// Move to SHA entries
	idx_offset += sizeof(struct fan);
	// Point to SHA entries
	entries = (struct entry *)(idxmap + idx_offset);

	for(n=0;n<nelements;n++)
		if (!memcmp(entries[n].sha, sha, 20))
			break;

	if (n==nelements)
		return -1;

	// Move to Checksums
	idx_offset += (sizeof(struct entry) * nelements);
	// Point to checksums
	checksums = (struct checksum *)(idxmap + idx_offset);
	// Move to Offsets
	idx_offset += (nelements * 4);
	// Capture Offsets
	offsets = (struct offset *)(idxmap + idx_offset);

	return ntohl(offsets[n].addr);
}
