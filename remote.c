#define _WITH_DPRINTF
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "remote.h"
#include "ini.h"
#include "lib.h"

static struct option long_options[] =
{
	{"verbose", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

int
remote_usage(int type)
{
	if (type == REMOTE_USAGE_DEFAULT) {
		printf("Remote usage statement\n");
		return 0;
	}
	else if (type == REMOTE_USAGE_REMOVE) {
		printf("usage: ogit remote remove <name>\n");
		return 129;
	}

	return 0;
}

int
remote_remove(int argc, char *argv[], uint8_t flags)
{
	char *repolocation;
	char tmpconfig[PATH_MAX + NAME_MAX];
	struct section *cur_section = sections;
	int fd;
	int match = 0;
	//char *newconfigpath;

	if (argc != 2)
		return remote_usage(REMOTE_USAGE_REMOVE);

	repolocation = argv[1];

	while(cur_section) {
		if (cur_section->type == REMOTE && !strncmp(cur_section->repo_name, repolocation, strlen(repolocation))) {
			match = 1;
			break;
		}
		cur_section = cur_section->next;
	}

	if (match == 0) {
		fprintf(stderr, "fatal: No such remote: %s\n", repolocation);
		return (128);
	}

	cur_section = sections;

	sprintf(tmpconfig, "%s/.config.XXXXXX", dotgitpath);
	fd = mkstemp(tmpconfig);
	if (fd == -1) {
		fprintf(stderr, "Unable to open temporary file: %s\n", tmpconfig);
		return -1;
	}

	/* Rewrite config file */
	while(cur_section) {
		if (cur_section->type == CORE) {
			dprintf(fd, "[core]\n");
			if (cur_section->repositoryformatversion != 0xFF) {
				dprintf(fd, "\trepositoryformatversion = %d\n", cur_section->repositoryformatversion);
			}
			if (cur_section->filemode)
				dprintf(fd, "\tfilemode = %s\n", (cur_section->filemode == TRUE ? "true" : "false"));
			if (cur_section->bare)
				dprintf(fd, "\tbare = %s\n", (cur_section->bare == TRUE ? "true" : "false"));
			if (cur_section->logallrefupdates != 0xFF)
				dprintf(fd, "\tlogallrefupdates = %s\n", (cur_section->logallrefupdates == TRUE ? "true" : "false"));
		}
		else if (cur_section->type == REMOTE) {
			if (strncmp(cur_section->repo_name, repolocation, strlen(repolocation))) {
				dprintf(fd, "[remote \"%s\"]\n", cur_section->repo_name);
				if (cur_section->url)
					dprintf(fd, "\turl = %s\n", cur_section->url);
				if (cur_section->fetch)
					dprintf(fd, "\tfetch = %s\n", cur_section->fetch);
			}
		}

		cur_section = cur_section->next;
	}

	close(fd);

	return 0;
}

int
remote_list(int argc, char *argv[], uint8_t flags)
{
	struct section *cur_section = sections;

	/* Usage if the default list command syntax is broken */
	if (argc > 1 && flags != OPT_VERBOSE)
	        remote_usage(REMOTE_USAGE_DEFAULT);

	while(cur_section) {
		if (cur_section->type == REMOTE) {
			if (!(flags & OPT_VERBOSE))
				printf("%s\n", cur_section->repo_name);
			else {
				printf("%s\t%s (fetch)\n",
				    cur_section->repo_name,
				    cur_section->url);
				printf("%s\t%s (push)\n",
				    cur_section->repo_name,
				    cur_section->url);
			}
		}
		cur_section = cur_section->next;
	}

	return 0;
}

int
remote_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;

	uint8_t cmd = 0;
	uint8_t flags = 0;

	argc--; argv++;

	if (argc > 1) {
		if (strncmp(argv[1], "add", 3) == 0) {
			argc--;
			argv++;
			cmd = CMD_ADD;
		}
		else if (strncmp(argv[1], "remove", 6) == 0) {
			argc--;
			argv++;
			cmd = CMD_REMOVE;
		}
	}

	while((ch = getopt_long(argc, argv, "v", long_options, NULL)) != -1)
		switch(ch) {
		case 'v':
			flags |= OPT_VERBOSE;
			continue;
		case '?':
		default:
			remote_usage(REMOTE_USAGE_DEFAULT);
		}


	if(git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}

	config_parser();

	switch(cmd) {
		case CMD_REMOVE:
			remote_remove(argc, argv, flags);
			break;
		case CMD_DEFAULT:
		default:
			remote_list(argc, argv, flags);
			break;
	}
	

	return (ret);
}
