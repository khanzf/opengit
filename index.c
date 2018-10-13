#include <stdint.h>
#include <stdio.h>
#include "index.h"

void
parse_indexentries(unsigned char *indexmap, int *offset, int entries)
{
	struct _indexentry *indexentry;
	struct _indexextentry *indexextentry;
	char *name;
	int i;

	indexentry = (struct _indexentry *)((char *)indexmap + *offset);

	for(i=0;i<entries;i++) {
		indexentry = (struct _indexentry *)((char *)indexmap + *offset);
		if (ntohs(indexentry->flags) & CE_EXTENDED) {
			indexextentry = (struct _indexextentry *)((char *)indexmap + *offset);
			*offset += (72 + strlen(indexextentry->name)) & ~0x7;
name = indexextentry->name;
		}
		else {
			*offset += (70 + strlen(indexentry->name)) & ~0x7;
name = indexentry->name;
		}

		printf("Name -> %s\n", name);
	}
}

void
parse_index(unsigned char *indexmap, off_t indexsize)
{
	struct _indexhdr *indexhdr;
	int offset;

	offset = 0;

	while (offset < indexsize) {
		indexhdr = (struct _indexhdr *)((unsigned char *)indexmap + offset);
		offset += sizeof(struct _indexhdr);
		printf("Signature:\t%s\n", indexhdr->sig);

		if (!memcmp(indexhdr->sig, "DIRC", 4)) {
			parse_indexentries(indexmap, &offset, ntohl(indexhdr->entries));
		}
		else {
			printf("Found something after DIRC\n"); exit(0);
		}
	}
}
