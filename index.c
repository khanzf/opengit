#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "index.h"

/*
    XXX This code could be cleaned up by having a local offset value
    and updating it at the end of the function
*/
void
parse_treeentries(unsigned char *indexmap, int *offset)
{
	unsigned char *treemap = indexmap;
	int entry_count;
	int subtrees;
	unsigned char *endptr;
	uint8_t objectid[20];
	int q;

//	printf("Offset starts at: %d\n", *offset);
	while(indexmap[*offset])
		*offset = *offset + 1;
	*offset = *offset + 1;

	entry_count = strtol(indexmap + *offset, &endptr, 10);
//	printf("entry_count: %d\n", entry_count);
	*offset = *offset + (endptr - (indexmap + *offset));
//	printf("That size was: %d\n", (endptr - (indexmap + *offset)));

	*offset = *offset + 1; // Jump past the space

	subtrees = strtol(indexmap + *offset, &endptr, 10);
	printf("Subtrees: %d\n", subtrees);
	*offset = *offset + (endptr - (indexmap + *offset));

//	printf("THe next character: 0x%x\n", indexmap[*offset]);

	*offset = *offset + 1; // Jump past the new line

	memcpy(objectid, indexmap + *offset, 20); 
//	printf("object id: %x%x%x%x%x\n", objectid[0], objectid[1], objectid[2], objectid[3], objectid[4]);

	if (subtrees >= 0)
		*offset = *offset + 20;

	for(q=0;q<subtrees;q++) {
		parse_treeentries(indexmap, offset);
//		printf("Going through a subtree\n");
//		printf("%c %c %c %c %c\n",	indexmap[*offset],
//						indexmap[*offset+1],
//						indexmap[*offset+2],
//						indexmap[*offset+3]);
		
	}

//	printf("The End\n");
}

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

//		printf("Name -> %s\n", name);
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
		printf("Signature:\t%s\n", indexhdr->sig);

		if (!memcmp(indexhdr->sig, "DIRC", 4)) {
			offset += sizeof(struct _indexhdr);
			parse_indexentries(indexmap, &offset, ntohl(indexhdr->entries));
		}
		else if (!memcmp(indexhdr->sig, "TREE", 4)) {
			uint32_t extsize;
			memcpy(&extsize, (struct _indexhdr *)((unsigned char *)indexmap + offset + 4), 4);
			printf("Length: %d\n", ntohl(extsize));
			offset += 8;
			parse_treeentries(indexmap, &offset);	
		}
		else {
			printf("Found something after DIRC\n");
			printf("Found this: %c%c%c%c\n", indexhdr->sig[0], indexhdr->sig[1], indexhdr->sig[2], indexhdr->sig[3]);
			exit(0);
		}
	}
}
