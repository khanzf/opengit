#define _WITH_DPRINTF
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "init.h"
#include "ini.h"

static struct option long_options[] =
{
	{"quiet",no_argument, NULL, 'q'},
	{"bare", no_argument, NULL, 0},
	{"template", required_argument, NULL, 0},
	{"separate-git-dir", required_argument, NULL, 0},
	{"shared", required_argument, NULL, 0},
	{NULL, 0, NULL, 0}
};

int
init_usage(int type)
{
	return 0;
}

int
bare_init(char *project_directory)
{
	struct stat sb;
	struct section new_config;
	char cwd[PATH_MAX];
	int fd;
	int ret;
	int x;

	char *dirs[] = {
		    "./.git/objects",
		    "./.git/objects/pack",
		    "./.git/objects/info",
		    "./.git/refs",
		    "./.git/refs/tags",
		    "./.git/refs/heads",
		    "./.git/branches"};

	/* XXX Do this later */
	if (project_directory != NULL) {
		fprintf(stderr, "Currently not supporting separate directories\n");
		return -1;
	}

	for(x = 0; x < 7; x++) {
		fd = open(dirs[x], O_WRONLY | O_CREAT);
		if (fd != -1) {
			fprintf(stderr, "File or directory %s already exists\n", dirs[x]);
			return (-1);
		}
	}

	new_config.type = CORE;
	new_config.repositoryformatversion = 0;
	new_config.filemode = TRUE;
	new_config.bare = TRUE;
	new_config.logallrefupdates = TRUE;

	mkdir("./.git", 0755);

	fd = open("./.git/config", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dprintf(fd, "[core]\n");
	dprintf(fd, "\trepositoryformatversion = 0\n");
	dprintf(fd, "\tfilemode = true\n");
	dprintf(fd, "\tbare = false\n");
	dprintf(fd, "\tlogallrefupdates = true\n");
	close(fd);

	for(x = 0; x < 7; x++) {
		ret = mkdir(dirs[x], 0755);
		if (ret == -1) {
			fprintf(stderr, "Cannot create %s\n", dirs[x]);
			perror("Cannot create directory");
			return(ret);
		}
	}

	fd = open("./.git/description", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if (fd == -1 || fstat(fd, &sb)) {
		fprintf(stderr, "Cannot create description file\n");
		return -1;
	}
	close(fd);

	fd = open("./.git/HEAD", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd != -1 && fstat(fd, &sb) == 0) {
		write(fd, "ref: refs/heads/master\x0a", 23); 
	}
	close(fd);

	getcwd((char *)&cwd, PATH_MAX);
	printf("Initialized empty Git repository in %s/.git/\n", cwd);

	return 0;
}

int
init_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;

	char *project_directory = NULL;

	uint8_t flags = 0;

	argc--; argv++;

	if (argc >= 2)
		project_directory = argv[1];

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
		switch(ch) {
		default:
			flags = 0;
			init_usage(-1);
		}
	}

	bare_init(project_directory);

	return (ret);
}
