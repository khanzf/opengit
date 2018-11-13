#ifndef INDEX_H
#define INDEX_H 

#include <stdint.h>

/*
Header source Documentation/technical/index-format.txt
*/

struct cache_tree {
	int entry_count;
	int subtree_count;
	uint8_t objectname[20];
	struct cache_tree **subtree;
};

struct indexhdr {
	char		sig[4];		/* Always "DIRC" */
	uint32_t	version;	/* Version Number */
	uint32_t	entries;	/* Number of extensions */
};

struct indexentry {
	uint32_t	ctime_sec;
	uint32_t	ctime_nsec;
	uint32_t	mtime_sec;
	uint32_t	mtime_nsec;
	uint32_t	dev;
	uint32_t	ino;
	uint32_t	mode;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	size;
	uint8_t		sha[20];
	uint16_t	flags;
	char		name[1];
} __packed;

#define CE_EXTENDED	0x4000

struct indexextentry {
	uint32_t	ctime_sec;
	uint32_t	ctime_nsec;
	uint32_t	mtime_sec;
	uint32_t	mtime_nsec;
	uint32_t	dev;
	uint32_t	ino;
	uint32_t	mode;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	size;
	uint8_t		sha[20];
	uint16_t	flags;
	uint16_t	flags2;
	char		name[1];
};

struct indexcache {
	struct indexhdr *indexhdrs;
	struct indexextentry *indexextentry;
	struct cache_tree *cache_tree;
};

struct cache_tree *parse_index(unsigned char *indexmap, off_t indexsize);

#endif
