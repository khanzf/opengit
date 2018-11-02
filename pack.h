#ifndef PACK_H
#define PACK_H

#include <stdint.h>

/*
Header source Documentation/technical/multi-pack-index.txt
*/

/*
    GNU git has at least 2 things called "index" in Git.
    These are for files located in .git/objects/pack/[*].idx
*/

// idx file headers
struct offset {
	unsigned int addr;
};

struct checksum {
	unsigned int val;
};

struct entry {
	unsigned char sha[20];
};

struct fan {
	int count[256];
};

// pack file headers


// Shared by both idx and pack files
struct packhdr {
	uint8_t		sig[4];
};

int pack_find_sha_offset(char *sha, char *idxmap);

#endif
