/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Farhan Khan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "lib/pack.h"
#include "lib/ini.h"
#include "clone.h"
#include "init.h"

/* uri, destdir, smart_head */
typedef int (*clone_handle_func)(char *, char *, struct smart_head *);

static struct clone_handler {
	const char *uri_scheme;
	clone_handle_func handler;
} clone_handlers[] = {
	{
		.uri_scheme = "http://",
		.handler = clone_http,
	},
	{
		.uri_scheme = "https://",
		.handler = clone_http,
	},
};

/* XXX Assume ssh by default? */
static struct clone_handler *default_handler = NULL;

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

/*
static void
clone_usage(int type)
{
	exit(128);
}*/

// XXX This may be moved to pack.[ch]
static void
process_nak()
{
	// Currently unimplemented
}

static void
process_unknown(unsigned char *reply, struct parseread *parseread, int offset, int size)
{
	if (parseread->psize == 0)
		write(parseread->fd, reply+5, size-5);
	else
		write(parseread->fd, reply, size);
}


static void
process_remote(unsigned char *reply, struct parseread *parseread)
{
	char buf[200];
	strncpy(buf, (char *)reply+5, parseread->osize-5);
	buf[parseread->osize-5] = '\0';
}

static void
process_objects(unsigned char *reply, struct parseread *parseread, int offset,
    int size)
{
	switch(parseread->state) {
	case STATE_NAK:
		process_nak();
		break;
	case STATE_REMOTE:
		process_remote(reply, parseread);
		break;
	default:
		process_unknown(reply, parseread, offset, size);
		break;
	}
}

// XXX This may be moved to pack.[ch]
size_t
clone_pack_protocol_process(void *buffer, size_t size, size_t nmemb, void *userp)
{
	unsigned char *reply;
	struct parseread *parseread = userp;
	int offset = 0;
	char tmp[5];
	int check;
	int pool;

	/*
	 * Check if there are any left-over bits from the previous iteration
	 * If so, add them to the reply and increase the pool size
	 * Otherwise, use the given values
	*/
	if (parseread->cremnant > 0) {
		reply = malloc(nmemb + parseread->cremnant + 1);
		memcpy(reply+parseread->cremnant, buffer, size * nmemb);
		memcpy(reply, parseread->bremnant, parseread->cremnant);
		pool = (size * nmemb) + parseread->cremnant;
	}
	else {
		reply = buffer;
		pool = size * nmemb;
	}

	/*
	 * Iterate up to the pool size being 4 bytes, in the event that the last
	 * four bytes are the beginning of a new object
	 */
	while(offset + 4 < pool) {
		if (parseread->state == STATE_NEWLINE) {
			bzero(tmp, 5);
			memcpy(tmp, reply+offset, 4); tmp[4] = '\0';
			check = sscanf(tmp, "%04x", &parseread->osize);

			if (parseread->osize == 0) {
				offset += 4;
				break;
			}

			parseread->psize = 0;

			if ( strncmp((char *)reply+offset+4, "NAK\n", 4)==0)
				parseread->state = STATE_NAK;
			else if (reply[offset+4] == 0x02)
				parseread->state = STATE_REMOTE;
			else
				parseread->state = STATE_UNKNOWN;
		}

		/*
		 * If the pool minus the offset is greater or equal to the
		 * remaining necessary bytes. This means we completed the data
		 * in that object and can reset to a new state.
		 */
		if ((pool - offset) >= (parseread->osize - parseread->psize) ) {
			process_objects(reply+offset, parseread, offset,
			    (parseread->osize - parseread->psize));
			offset += (parseread->osize - parseread->psize);
			parseread->state = STATE_NEWLINE;
		}
		/* Otherwise, we need more bytes */
		else if ((pool-offset) < (parseread->osize - parseread->psize)) {
			process_objects(reply+offset, parseread, offset,
			    pool-offset);
			parseread->psize += (pool - offset);
			offset = pool;
		}

	}

	/* The while-loop could break and the offset has not caught up to
	 * the pool. In this case, we need to cache those 4 bytes, update
	 * the parseread->cremnant value and return offset + parseread->cremnant
	 * (which should be nmemb * size).
	 */
	if (pool-offset > 0)
		memcpy(parseread->bremnant, reply+offset, pool-offset);
	if (parseread->cremnant > 0)
		free(reply);
	parseread->cremnant = nmemb - offset;

	return offset + parseread->cremnant;

}

/*
 * Gets the git directory name from the path
 * Will expand if the path is a bare repo, does not have a name, et al
 */
static char *
get_repo_dir(char *path)
{
	char *reponame;
	int x, end;

	x = strlen(path);
	if (path[x-1] == '/')
		x--;
	end = x;
	/* Check if the URL ends in .git or .git/ */
	if (!strncmp(path+x-4, ".git", 4))
		end = x - 4;

	/* Find the first '/' */
	for(;path[x-1]!='/' && x !=0;x--);
	if (x == 0) {
		fprintf(stderr, "There must be a pathname, failure.\n");
		exit(-1);
	}

	reponame = strndup(path+x, end-x);
	return reponame;
}

static void
populate_packed_refs(char *repodir, struct smart_head *smart_head)
{
	char path[PATH_MAX];
	FILE *refs;

	snprintf(path, PATH_MAX, "%s/%s", repodir, ".git/packed-refs");
	refs = fopen(path, "w");
	if (refs == NULL) {
		fprintf(stderr, "Unable to open file for writing. %s.\n",
		    path);
	}

	for(int x=0;x<smart_head->refcount;x++)
		fprintf(refs, "%s %s\n",
		    smart_head->refs[x].sha,
		    smart_head->refs[x].path);

	fclose(refs);
}

static void
populate_symrefs(char *repodir, struct smart_head *smart_head)
{
	char path[PATH_MAX];
	struct symref *ref;
	FILE *symfile;

	/* HEAD -> refs/heads/master */
	STAILQ_FOREACH(ref, &smart_head->symrefs, link) {
		snprintf(path, sizeof(path), "%s/.git/%s", repodir, ref->symbol);
		symfile = fopen(path, "w");
		if (symfile == NULL) {
			fprintf(stderr, "Unable to open file for writing. %s.\n",
			    path);
			continue;
		}

		fprintf(symfile, "ref: %s\n", ref->path);
		fclose(symfile);
	}
}

static void
clone_initial_config(char *repopath, char *repodir, struct section *sections)
{
	struct section core;
	struct section remote;
	struct section branch;
	char path[PATH_MAX];
	int fd;

	/* Adds core, remote and branch */
	sections = malloc(sizeof(struct section) * 3);

	core.type = CORE;
	core.repositoryformatversion = 0;
	core.filemode = TRUE;
	core.bare = FALSE;
	core.logallrefupdates = TRUE;

	remote.type = REMOTE;
	remote.repo_name = "origin";
	remote.url = repopath;
	remote.fetch = "+refs/heads/*:refs/remotes/origin/*";

	branch.type = BRANCH;
	branch.remote = "origin";
	branch.merge = "refs/heads/master";

	core.next = &remote;
	remote.next = &branch;
	branch.next = NULL;

	strncpy(path, repodir, strlen(repodir)+1);
	strncat(path, "/.git/config", 12);
	fd = open(path, O_WRONLY | O_CREAT, 0660);
	if (fd == -1) {
		fprintf(stderr, "Unable to open file %s: %s\n", path, strerror(errno));
		exit(errno);
	}

	ini_write_config(fd, &core);
}

int
clone_main(int argc, char *argv[])
{
	struct smart_head smart_head;
	char *repodir;
	char *repopath;
	struct clone_handler *chandler;
	int nch, ret = 0;
	int ch;
	int q = 0;
	bool found;

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
	repopath = argv[1];
	repodir = get_repo_dir(argv[1]);

	found = false;
	chandler = NULL;
	for (nch = 0; nch < nitems(clone_handlers); ++nch) {
		chandler = &clone_handlers[nch];
		assert(chandler->uri_scheme != NULL);
		assert(chandler->handler != NULL);

		if (strncmp(repopath, chandler->uri_scheme,
		    strlen(chandler->uri_scheme)) == 0) {
			found = true;
			break;
		}
	}

	/* This will become stale when we setup the default handler... (ssh?) */
	if (!found && default_handler != NULL) {
		fprintf(stderr, "URI Scheme not recognized for '%s'\n", repopath);
		return (128);
	} else if (!found) {
		chandler = default_handler;
	}

	init_dirinit(repodir);

	smart_head.refs = NULL;
	STAILQ_INIT(&smart_head.symrefs);
	ret = chandler->handler(repopath, repodir, &smart_head);
	if (ret != 0)
		goto out;

	/* Populate .git/pack-refs */
	populate_packed_refs(repodir, &smart_head);

	if (!STAILQ_EMPTY(&smart_head.symrefs))
		populate_symrefs(repodir, &smart_head);
	/* Write the initial config file */
	clone_initial_config(repopath, repodir, NULL);
	/* Checkout Latest commit */

out:
	free(repodir);

	return (ret);
}
