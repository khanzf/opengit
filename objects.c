#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "objects.h"
#include "common.h"
#include "ini.h"

#include <netinet/in.h>

void
test_test() {
	DIR *d;
	struct dirent *dir;
	struct idxhdr *idxhdr;
	char packdir[PATH_MAX];
	char idxfile[PATH_MAX];
//	char packfile[PATH_MAX];
	char *file_ext;
	int idxfilefd;
	char *idxmap;
//	void *packmap;
	struct stat sb;
	int offset;
	int idx_version;
	

int v;

	sprintf(packdir, "%s/objects/pack", dotgitpath);

	d = opendir(packdir);

	if (d) {
		while((dir = readdir(d)) != NULL) {
			file_ext = strrchr(dir->d_name, '.');
			if (!file_ext || strncmp(file_ext, ".idx", 4))
				continue;
			sprintf(idxfile, "%s/objects/pack/%s", dotgitpath, dir->d_name);
			//printf("Working with: -> %s\n", idxfile);

			idxfilefd = open(idxfile, O_RDONLY);
			fstat(idxfilefd, &sb);
			idxmap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, idxfilefd, 0);

			if (idxmap == NULL) {
				fprintf(stderr, "mmap(2) error, exiting.\n");
				exit(0);
			}

			if (strncmp(idxmap, "\xff\x74\x4f\x63", 4)) {
				fprintf(stderr, "Header signature does not match index version 2.\n");
				exit(0);
			}
			offset = 4;

			idx_version = *(idxmap + offset+3);
			printf("the version is: %d\n", idx_version);

			offset += 4;
			idxhdr = idxmap + offset;


			for(v=0;v<256;v++) {
				printf("Count -> %02x:%d\n", v, ntohl(idxhdr->fantable[v]));
			}

			offset += sizeof(struct idxhdr);

			int last = idxhdr->fantable[255];
			struct entry *entries = malloc(sizeof(struct entry) * last);

			printf("size is: %d\n", ntohl(last));

			entries = idxmap + offset;
			for(v=0;v<last;v++) {
				printf("Offset: %d, %d SHA\n", v, entries[v].offset);
			}
			printf("After\n");

			munmap(idxmap, sb.st_size);
			close(idxfilefd);
			

		}
	}

	closedir(d);
}
