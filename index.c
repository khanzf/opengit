#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "index.h"

/*
    XXX This code could be cleaned up by having a local offset value
    and updating it at the end of the function
*/
/*
    This code is very confusing and is worse on the GNU git side.
    This is the best I could do to make it readable.
*/
struct cache_tree *
parse_treeentries(char *indexmap, int *offset)
{
	struct cache_tree *cache_tree;
	char *endptr;
	int q=0;

	cache_tree = malloc(sizeof(struct cache_tree));

	while(indexmap[*offset])
		*offset = *offset + 1;
	*offset = *offset + 1;

	cache_tree->entry_count = strtol(indexmap + *offset, &endptr, 10);
	*offset = *offset + (endptr - (indexmap + *offset));

	*offset = *offset + 1; // Jump past the space

	cache_tree->subtree_count = strtol(indexmap + *offset, &endptr, 10);
	*offset = *offset + (endptr - (indexmap + *offset));

	*offset = *offset + 1; // Jump past the new line

	// XXX rename from objectname to sha?
	memcpy(cache_tree->objectname, indexmap + *offset, 20); 
	printf("Tree: %x%x%x%x%x%x%x%x%x%x\n", cache_tree->objectname[0], cache_tree->objectname[1],
	cache_tree->objectname[2], cache_tree->objectname[3], cache_tree->objectname[4],
	cache_tree->objectname[5], cache_tree->objectname[6], cache_tree->objectname[7],
	cache_tree->objectname[8], cache_tree->objectname[9]);

	if (cache_tree->subtree_count >= 0)
		*offset = *offset + 20;

	cache_tree->subtree = malloc(sizeof(struct cache_tree *) * q);

	for(q=0;q<cache_tree->subtree_count;q++)
		cache_tree->subtree[q] = parse_treeentries(indexmap, offset);

	return cache_tree;
}

void
parse_indexentries(char *indexmap, int *offset, int entries)
{
	struct indexentry *indexentry;
	struct indexextentry *indexextentry;
	char *name;
uint8_t *sha;
	int i;

	indexentry = (struct indexentry *)((char *)indexmap + *offset);

	for(i=0;i<entries;i++) {
		indexentry = (struct indexentry *)((char *)indexmap + *offset);
		if (ntohs(indexentry->flags) & CE_EXTENDED) {
			indexextentry = (struct indexextentry *)((char *)indexmap + *offset);
			*offset += (72 + strlen(indexextentry->name)) & ~0x7;
			name = indexextentry->name;
			sha = indexextentry->sha;
		}
		else {
			*offset += (70 + strlen(indexentry->name)) & ~0x7;
			name = indexentry->name;
			sha = indexentry->sha;
		}
		printf("Name: %x:%x:%x:%x:%x:%x:%x:%x:%x:%x %s\n", sha[0], sha[1], sha[2], sha[3], sha[4], sha[5], sha[6], sha[7], sha[8], sha[9], name);

	}
}

struct cache_tree *
parse_index(char *indexmap, off_t indexsize)
{
	struct cache_tree *cache_tree;
	struct indexcache *indexcache;
	struct indexhdr *indexhdr;

	int offset;

	offset = 0;

	indexcache = malloc(sizeof(struct indexcache));

	while (offset < indexsize) {
		indexhdr = (struct indexhdr *)((char *)indexmap + offset);

		if (!memcmp(indexhdr->sig, "DIRC", 4)) {
			offset += sizeof(struct indexhdr);
			parse_indexentries(indexmap, &offset, ntohl(indexhdr->entries));
		}
		else if (!memcmp(indexhdr->sig, "TREE", 4)) {
			uint32_t extsize;
			memcpy(&extsize, (struct indexhdr *)((char *)indexmap + offset + 4), 4);
			offset += 8;
			indexcache->cache_tree = parse_treeentries(indexmap, &offset);	
			offset += extsize + 8;
		}
		else {
			printf("Found something after DIRC\n");
			printf("Found this: %c%c%c%c\n", indexhdr->sig[0], indexhdr->sig[1], indexhdr->sig[2], indexhdr->sig[3]);
			printf("Exiting.\n");
			exit(0);
		}
	}

	return cache_tree;
}
