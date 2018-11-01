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
	int idx_offset;
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
			idx_offset = 4;

			idx_version = *(idxmap + idx_offset+3);
			printf("the version is: %d\n", idx_version);

			idx_offset += 4;
			idxhdr = idxmap + idx_offset;

			// Fanout Table

			for(v=0;v<256;v++) {
				printf("Count -> %02x:%d\n", v, ntohl(idxhdr->fantable[v]));
			}

			idx_offset += sizeof(struct idxhdr);

			int last = ntohl(idxhdr->fantable[255]);
			struct entry *entries = idxmap + idx_offset;

			printf("size is: %d\n", last);

			// Hashes

			for(v=0;v<last;v++) {
				printf("SHA: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
					entries[v].sha[0], entries[v].sha[1], entries[v].sha[2], entries[v].sha[3],
					entries[v].sha[4], entries[v].sha[5], entries[v].sha[6], entries[v].sha[7],
					entries[v].sha[8], entries[v].sha[9], entries[v].sha[10], entries[v].sha[11],
					entries[v].sha[12], entries[v].sha[13], entries[v].sha[14], entries[v].sha[15],
					entries[v].sha[16], entries[v].sha[17], entries[v].sha[18], entries[v].sha[19]
				);
			}

			idx_offset += (sizeof(struct entry) * last);

			// Offsets
			struct offset *crc = idxmap + idx_offset;

			for(v=0;v<last;v++) {
				printf("crc: %u\n", crc[v].addr);
			}

		printf("Ending\n");

			idx_offset += (sizeof(struct entry) * last);
			struct offset *offsets = idxmap + idx_offset;

			for(v=0;v<last;v++) {
				printf("Offset: %u\n", offsets[v].addr);
			}

			exit(0);

			munmap(idxmap, sb.st_size);
			close(idxfilefd);
			

		}
	}

	closedir(d);
}
