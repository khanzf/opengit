#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "pack.h"
#include "common.h"
#include "ini.h"

#include <netinet/in.h>

int
pack_find_sha(char *sha, char *idxmap) {
	struct fan *fans;
	struct entry *entries;
	struct checksum *checksums;
	struct offset *offsets;
	int idx_offset;
	char idx_version;
	int nelements;
	int n;

	if (strncmp(idxmap, "\xff\x74\x4f\x63", 4)) {
		fprintf(stderr, "Header signature does not match index version 2.\n");
		exit(0);
	}
	idx_offset = 4;

	idx_version = *(idxmap + idx_offset+3);
	printf("the version is: %d\n", idx_version);
	if (idx_version != 2) {
		fprintf(stderr, "opengit currently only supports version 2. Exiting.\n");
		exit(0);
	}
	idx_offset += 4;

	// Get the fan table and capture last element
	fans = idxmap + idx_offset;
	nelements = ntohl(fans->count[255]);
	// Move to SHA entries
	idx_offset += sizeof(struct fan);
	// Point to SHA entries
	entries = idxmap + idx_offset;

	printf("%02x%02x%02x%02x AAAAAAAA\n",   sha[0], sha[1], sha[2], sha[3]);

	for(n=0;n<nelements;n++) {

		printf("%02x%02x%02x%02x",   entries[n].sha[00], entries[n].sha[01], entries[n].sha[02], entries[n].sha[03]);
		printf("%02x%02x%02x%02x",   entries[n].sha[04], entries[n].sha[05], entries[n].sha[06], entries[n].sha[07]);
		printf("%02x%02x%02x%02x",   entries[n].sha[ 8], entries[n].sha[ 9], entries[n].sha[10], entries[n].sha[11]);
		printf("%02x%02x%02x%02x",   entries[n].sha[12], entries[n].sha[13], entries[n].sha[14], entries[n].sha[15]);
		printf("%02x%02x%02x%02x\n", entries[n].sha[16], entries[n].sha[17], entries[n].sha[18], entries[n].sha[19]);

		if (!memcmp(entries[n].sha, sha, 20)) {
			printf("Match!\n");
			break;
		}
	}

	if (n==nelements) {
		printf("Same value\n");
	}

	// Move to Checksums
	idx_offset += (sizeof(struct entry) * nelements);
	// Point to checksums
	checksums = idxmap + idx_offset;
	// Move to Offsets
	idx_offset += (nelements * 4);
	// Capture Offsets
	offsets = idxmap + idx_offset;
	return 0;
	
}

/*
int
pack_pointer(struct fan *fans, struct entry *entries, struct checksum *checksums, struct offset *offsets) {
//	DIR *d;
//	struct dirent *dir;
//	struct fan *fans;
//	struct entry *entries;
//	struct checksum *checksums;
//	struct offset *offsets;
	char packdir[PATH_MAX];
	char idxfile[PATH_MAX];
	char *file_ext;
	int idxfilefd;
	char *idxmap;
	struct stat sb;
	int idx_offset;
	int idx_version;
	int nelements;
	int c;

	if (strncmp(idxmap, "\xff\x74\x4f\x63", 4)) {
		fprintf(stderr, "Header signature does not match index version 2.\n");
		exit(0);
	}
	idx_offset = 4;

	idx_version = *(idxmap + idx_offset+3);
	printf("the version is: %d\n", idx_version);

	idx_offset += 4;

	// Fanout Table
	fans = idxmap + idx_offset;
	nelements = ntohl(fans->count[255]);

	idx_offset += sizeof(struct fan);

	entries = idxmap + idx_offset;

	// Hashes

/
	for(c=0;c<nelements;c++) {
		printf("SHA: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
			entries[v].sha[0], entries[v].sha[1], entries[v].sha[2], entries[v].sha[3],
			entries[v].sha[4], entries[v].sha[5], entries[v].sha[6], entries[v].sha[7],
			entries[v].sha[8], entries[v].sha[9], entries[v].sha[10], entries[v].sha[11],
			entries[v].sha[12], entries[v].sha[13], entries[v].sha[14], entries[v].sha[15],
			entries[v].sha[16], entries[v].sha[17], entries[v].sha[18], entries[v].sha[19]
		);
	}
/

	idx_offset += (sizeof(struct entry) * nelements);

	checksums = idxmap + idx_offset;
/
	for(c=0;c<nelements;c++) {
		printf("Checksum: %u\n", ntohl(checksums[c].val));
	}
/
	idx_offset += (nelements * 4);

	// Offsets
	offsets = idxmap + idx_offset;

/	for(c=0;c<nelements;c++) {
		printf("Offset: %u\n", ntohl(offsets[c].addr));
	}/

}
*/
