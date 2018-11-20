#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "zlib_handler.h"
#include "pack.h"
#include "common.h"
#include "ini.h"

#include <netinet/in.h>

void
pack_uncompress_object(int packfd)
{
	struct writer_args writer_args;
	writer_args.fd = STDOUT_FILENO;
	writer_args.sent = 0;
	deflate_caller(packfd, write_cb, &writer_args);
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
