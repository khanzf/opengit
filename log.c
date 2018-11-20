#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "zlib_handler.h"
#include "log.h"
#include "common.h"
#include "ini.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

int
log_usage(int type)
{
	fprintf(stderr, "usage: git log [<options>] [<revision-range>] [[--] <path>...]\n");
	fprintf(stderr, "   or: git show [<options>] <object>...\n\n");
	fprintf(stderr, "    -q, --quiet           suppress diff output\n");
	fprintf(stderr, "    --source              show source\n");
	fprintf(stderr, "    --use-mailmap         Use mail map file\n");
	fprintf(stderr, "    --decorate-refs <pattern>\n");
	fprintf(stderr, "                          only decorate refs that match <pattern>\n");
	fprintf(stderr, "    --decorate-refs-exclude <pattern>\n");
	fprintf(stderr, "                          do not decorate refs that match <pattern>\n");
	fprintf(stderr, "    --decorate[=...]      decorate options\n");
	fprintf(stderr, "    -L <n,m:file>         Process line range n,m in file, counting from 1\n\n");
	exit(129);
	return 129;
}

void
log_print_commit_headers(struct logarg *logarg)
{
	char *token, *tmp;
	char *tofree;
	char *datestr;
	char author[255];
	long t;

	tofree = tmp = strdup(logarg->headers);
	printf("\e[0;33mcommit %s\e[0m\n", logarg->sha);

	while((token = strsep(&tmp, "\n")) != NULL) {
		if (strncmp(token, "parent ", 7) == 0) {
			strncpy(logarg->sha, token+7, 40);
			logarg->status |= LOG_STATUS_PARENT;
		}
		else if (strncmp(token, "author ", 7) == 0) {
			t = strtol(token + 38, NULL, 10);
			datestr = ctime(&t);
			datestr[24] = '\0';
			strncpy(author, token+7, strlen(token)-23);
			printf("Author:\t%s\n", author);
			printf("Date:\t%s %s\n", datestr, token + strlen(token)-5);
			logarg->status |= LOG_STATUS_AUTHOR;
		}
	}

	free(tmp);
	free(tofree);

}

unsigned char *
log_display_cb(unsigned char *buf, int size, void *arg)
{
	char *content;
	struct logarg *logarg = arg;
	int offset = 0;

	if (logarg->status == LOG_STATUS_COMMIT) {
		logarg->status = 1;
		offset = 11;
	}

	if (logarg->status == LOG_STATUS_HEADERS) {
		// Added content to headers
		logarg->headers = realloc(logarg->headers, logarg->size + size - offset);
		memset(logarg->headers + logarg->size, 0, size);
		strncpy(logarg->headers + logarg->size, (char *)buf + offset, size - offset);

		content = strstr(logarg->headers, "\n\n");
		if (content != NULL) {
			logarg->status = 2; // Found
			content = content + 2;

			log_print_commit_headers(logarg);
			putchar('\n');
			printf("%s", content);
		}
	}
	else {
		printf("%s", buf);
	}

	return NULL;
}

void
log_display_commits()
{
	int headfd;
	char headfile[PATH_MAX];
	char refpath[PATH_MAX];
	char ref[PATH_MAX];
	int l;
	struct logarg logarg;

	bzero(&logarg, sizeof(struct logarg));

	sprintf(headfile, "%s/HEAD", dotgitpath);
	headfd = open(headfile, O_RDONLY);
	if (headfd == -1) {
		fprintf(stderr, "Error, no HEAD file found. We prob aren't in a git directory\n");
		exit(128);
	}

	read(headfd, ref, 5);
	if (strncmp(ref, "ref: ", 5) == 0) {
		ref[6] = '\0';
		l = read(headfd, ref, PATH_MAX) - 1;
		if (ref[l] == '\n')
			ref[l] = '\0';
		sprintf(refpath, "%s/%s", dotgitpath, ref);
	}
	else {
		fprintf(stderr, "Currently not supporting the raw hash in HEAD\n");
		exit(128);
	}
	close(headfd);

	headfd = open(refpath, O_RDONLY);
	if (headfd == -1) {
		fprintf(stderr, "failed to open: %s\n", refpath);
		exit(128);
	}

	read(headfd, logarg.sha, 40);

	char objectpath[PATH_MAX];
	int objectfd;

	logarg.status = LOG_STATUS_PARENT;
	while(logarg.status & LOG_STATUS_PARENT) {
		logarg.status = 0;
		sprintf(objectpath, "%s/objects/%c%c/%s",
		    dotgitpath, logarg.sha[0], logarg.sha[1],
		    logarg.sha+2);
		objectfd = open(objectpath, O_RDONLY);
		deflate_caller(objectfd, log_display_cb, &logarg);
		close(objectfd);

		if (logarg.status & LOG_STATUS_PARENT)
			putchar('\n');
	}

}

int
log_main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int q = 0;

	argc--; argv++;

	while((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1)
		switch(ch) {
		case 0:
			break;
		case 1:
			break;
		default:
			printf("Currently not implemented\n");
			return -1;
		}
	argc = argc - q;
	argv = argv + q;

	if (git_repository_path() == -1) {
		fprintf(stderr, "fatal: not a git repository (or any of the parent directories): .git");
		exit(0);
	}
	config_parser();

	log_display_commits();

	return (ret);
}

