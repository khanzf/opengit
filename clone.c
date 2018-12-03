#include <sys/param.h>
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
#include <fetch.h>
#include "clone.h"
#include "zlib_handler.h"
#include "common.h"
#include "ini.h"

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

void
clone_usage(int type)
{
	fprintf(stderr, "Git clone usage statement\n");
	exit(128);
}

void
clone_http(char *url)
{
	struct smart_head smart_head;
	FILE *web;
	char fetchurl[1000];
	char out[1024];
	char *response, *position;
	struct branch *branch;
	char *token, *string, *tofree;
	int r;
	long offset;
	char sha[41]; // HEAD sha

	sprintf(fetchurl, "%s/info/refs?service=git-upload-pack", url);
	web = fetchGetURL(fetchurl, NULL);
	offset = 0;
	smart_head.cap = 0;
	response = NULL;
	do {
		r = fread(out, 1, 1024, web);
		response = realloc(response, offset+r);
		memcpy(response+offset-1, out, r);
		offset += r;
	} while(r >= 1024);

	position = (char *)response;
	sscanf(position, "%03lx", &offset);
	position += offset;

	if (strncmp(position, "0000", 4)) {
		fprintf(stderr, "Protocol mismatch.\n");
		exit(128);
	}
	position += 3;

	sscanf(position, "%04lx", &offset);
	position += 4;
	strncpy(smart_head.sha, position, 41);

	tofree = string = strndup(position+41+strlen(position+41)+1,
			    offset-(47+strlen(position+41)));

	while((token = strsep(&string, " \n")) != NULL) {
		if (!strncmp(token, "multi_ack", 9))
			smart_head.cap |= CLONE_MULTI_ACK;
		else if (!strncmp(token, "multi_ack_detailed", 18))
			smart_head.cap |= CLONE_MULTI_ACK_DETAILED;
		else if (!strncmp(token, "no-done", 7))
			smart_head.cap |= CLONE_MULTI_NO_DONE;
		else if (!strncmp(token, "thin-pack", 9))
			smart_head.cap |= CLONE_THIN_PACK;
		else if (!strncmp(token, "side-band", 9))
			smart_head.cap |= CLONE_SIDE_BAND;
		else if (!strncmp(token, "side-band-64k", 13))
			smart_head.cap |= CLONE_SIDE_BAND_64K;
		else if (!strncmp(token, "ofs-delta", 9))
			smart_head.cap |= CLONE_OFS_DELTA;
		else if (!strncmp(token, "agent", 5))
			smart_head.cap |= CLONE_AGENT;
		else if (!strncmp(token, "shallow", 7))
			smart_head.cap |= CLONE_SHALLOW;
		else if (!strncmp(token, "deepen-since", 12))
			smart_head.cap |= CLONE_DEEPEN_SINCE;
		else if (!strncmp(token, "deepen-not", 10))
			smart_head.cap |= CLONE_DEEPEN_NOT;
		else if (!strncmp(token, "deepen-relative", 15))
			smart_head.cap |= CLONE_DEEPEN_RELATIVE;
		else if (!strncmp(token, "no-progress", 11))
			smart_head.cap |= CLONE_NO_PROGRESS;
		else if (!strncmp(token, "include-tag", 11))
			smart_head.cap |= CLONE_INCLUDE_TAG;
		else if (!strncmp(token, "report-status", 13))
			smart_head.cap |= CLONE_REPORT_STATUS;
		else if (!strncmp(token, "delete-refs", 11))
			smart_head.cap |= CLONE_DELETE_REFS;
		else if (!strncmp(token, "quiet", 5))
			smart_head.cap |= CLONE_QUIET;
		else if (!strncmp(token, "atomic", 6))
			smart_head.cap |= CLONE_ATOMIC;
		else if (!strncmp(token, "push-options", 12))
			smart_head.cap |= CLONE_PUSH_OPTIONS;
		else if (!strncmp(token, "allow-tip-sha1-in-want", 22))
			smart_head.cap |= CLONE_ALLOW_TIP_SHA1_IN_WANT;
		else if (!strncmp(token, "allow-reachable-sha1-in-want", 28))
			smart_head.cap |= CLONE_ALLOW_REACHABLE_SHA1_IN_WANT;
		else if (!strncmp(token, "push-cert", 9))
			smart_head.cap |= CLONE_PUSH_CERT;
		else if (!strncmp(token, "filter", 6))
			smart_head.cap |= CLONE_FILTER;
	}
	free(tofree);

	position += offset;
	tofree = string = strdup(position);
	r = 0;

	while((token = strsep(&string, "\n")) != NULL) {
		if (!strncmp(token, "0000", 4))
			break;
		branch = realloc(branch, sizeof(struct branch) * (r+1));
		sscanf(token, "%04lx", &offset);
		strncpy(branch[r].sha, token+4, 40);
		branch[r].sha[40] = '\0';
		branch[r].name = strdup(token + 45);
	}
	free(tofree);
	free(response);

	memcpy(sha, position, 40);
	sha[40] = '\0';

	printf("HEAD: %s\n", sha);


	exit(0);
}

int
clone_main(int argc, char *argv[])
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

	clone_http(argv[1]);

	return (ret);
}

