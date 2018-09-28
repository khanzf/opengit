#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "ini.h"
#include "lib.h"

int remote_main()
{
	int ret = 0;

	if(git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}

	config_parser();

	return (ret);
}
