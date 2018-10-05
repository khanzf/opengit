#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "common.h"

/*
	Gets the path of the .git directory
	If found, returns 0 and sets dotgitpath
	If not found, returns 1
	If errors, returns -1
*/

char dotgitpath[PATH_MAX + NAME_MAX];

int
git_repository_path()
{
	char *d = NULL;
	int s;
	char path[PATH_MAX + 5];
	struct stat sb;
	int ret = 1;

	d = getcwd(d, PATH_MAX);
	if (d == NULL) {
		fprintf(stderr, "Unable to get current directory: %s\n", strerror(errno));
		exit(errno);
	}

	/* XXX Is there a better way to do this? */

	while (strncmp(d, "/\0", 2) != 0) {
		sprintf(path, "%s/.git", d);
		s = stat(path, &sb);
		if (s == -1) {
			d = dirname(d);
			if (d == NULL) {
				ret = -1;
				break;
			}
			continue;
		}
		else if (s == 0) {
			ret = 0;
			strncpy(dotgitpath, path, strlen(path));
			break;
		}
	};

	free(d);
	return (ret);
}
