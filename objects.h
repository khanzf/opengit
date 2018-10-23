#ifndef OBJECTS_H
#define OBJECTS_H

#include <stdint.h>

/*
Header source Documentation/technical/multi-pack-index.txt
*/

/*
    GNU git has at least 2 things called "index" in Git.
    These are for files located in .git/objects/pack/[*].idx
*/

struct entry {
	int offset;
	char sha[20];
};

struct idxhdr {
	int fantable[256];
};

struct packhdr {
	uint8_t		sig[4];
};

void test_test();

#endif
