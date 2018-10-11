#ifndef INDEX_H
#define INDEX_H 

#include <stdint.h>

/*
Header source
https://github.com/git/git/blob/master/Documentation/technical/index-format.txt
*/

struct _cache_hdr {
	unsigned char	sig[4];		/* Always "DIRC" */
	uint32_t	version;	/* Version Number */
	uint32_t	entries;	/* Number of extensions */
};

struct _index_hdr {
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

struct _extension_hdr {
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

struct _cache_hdr cache_hdr;

#endif
