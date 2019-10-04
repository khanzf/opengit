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
#include "lib/common.h"
#include "lib/pack.h"
#include "lib/index.h"
#include "lib/ini.h"
#include "lib/loose.h"
#include "lib/zlib-handler.h"
#include "clone.h"
#include "init.h"

static struct clone_handler clone_handlers[] = {
	{
		.matcher = match_http,
		.run_service = http_run_service,
		.get_pack_stream = http_get_pack_stream,
	},
	{
		.matcher = match_ssh,
		.run_service = ssh_run_service,
		.get_pack_stream = ssh_get_pack_stream,
	},
};

/* XXX Assume ssh by default? */
static struct clone_handler *default_handler = NULL;

static struct option long_options[] =
{
	{NULL, 0, NULL, 0}
};

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
	if (!strncmp(path+x-PKTSIZELEN, ".git", 4))
		end = x - 4;

	/* Find the first '/' */
	for(;path[x-1]!='/' && x !=0;x--);
	if (x == 0) {
		fprintf(stderr, "There must be a pathname, failure.\n");
		exit(-1);
	}

	reponame = strndup(path+x, end-x);
	return (reponame);
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

	for(int x=0;x<smart_head->refcount;x++) {
		fwrite(smart_head->refs[x].sha, HASH_SIZE, 1, refs);
		fprintf(refs, " %s\n", smart_head->refs[x].path);
	}

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
clone_initial_config(char *uri, char *repodir, struct section *sections)
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
	remote.url = uri;
	remote.fetch = "+refs/heads/*:refs/remotes/origin/*";

	branch.type = BRANCH;
	branch.repo_name = "master";
	branch.remote = "origin";
	branch.merge = "refs/heads/master";

	core.next = &remote;
	remote.next = &branch;
	branch.next = NULL;

	strlcpy(path, repodir, PATH_MAX);
	strlcat(path, "/.git/config", PATH_MAX);
	fd = open(path, O_WRONLY | O_CREAT, 0660);
	if (fd == -1) {
		fprintf(stderr, "1 Unable to open file %s: %s\n", path, strerror(errno));
		exit(errno);
	}

	ini_write_config(fd, &core);
}

/*
 * Description: Used to loop through each tree object and iteratively
 * through each sub-tree object.
 * Handler for iterate_tree
 */
void
generate_tree_item(char *mode, uint8_t type, char *sha, char *filename, void *arg)
{
	char *prefix = arg;
	char *buildpath = prefix;
	char *fn = prefix + strlen(prefix);
	int imode;

	imode = strtol(mode+2, 0, 8);
	snprintf(fn, PATH_MAX, "/%s", filename);
	if (type == OBJ_TREE) {
		mkdir(buildpath, 0777);
		ITERATE_TREE(sha, generate_tree_item, prefix);
	}
	else {
		struct writer_args writer_args;
		int buildfd;

		buildfd = open(buildpath, O_CREAT|O_WRONLY, imode);
		writer_args.fd = buildfd;
		writer_args.sent = 0;

		CONTENT_HANDLER(sha, write_cb, write_pack_cb, &writer_args);
	}
	*fn = '\0';
}

static int
clone_generic_build_done(char **content, int content_length)
{
	*content = realloc(*content, content_length + 14);
	strlcpy(*content+content_length, "00000009done\n\0", 14);
	return (13); // Always the same length
}

static int
clone_generic_build_want(char **content, int content_length, char *capabilities, const char *sha)
{
	char line[3000]; // XXX Bad approach
	int len;

	/* size + want + space + SHA(40) + space + capabilities + newline */
	len = PKTSIZELEN + 4 + 1 + HASH_SIZE + 1 + strlen(capabilities) + 1;

	sprintf(line, "%04xwant %s %s\n", len, sha, capabilities);
	*content = realloc(*content, content_length + len + 1);
	strlcpy(*content+content_length, line, len+1);

	return (len);
}

static int
clone_build_post_content(const char *sha, char **content)
{
	int content_length;
	char *capabilities = "multi_ack_detailed no-done side-band-64k thin-pack ofs-delta deepen-since deepen-not agent=opengit/0.0.1-pre";

	content_length = 0;

	content_length += clone_generic_build_want(content, content_length,
	    capabilities, sha);
	content_length += clone_generic_build_done(content, content_length);

	return (content_length);
}

static void
clone_generic_get_pack(struct clone_handler *chandler, int packfd, struct smart_head *smart_head)
{
	char *content = NULL;
	int content_length;
	struct parseread parseread;
	FILE *packptr;

	content_length = clone_build_post_content(smart_head->sha, &content);
	packptr = chandler->get_pack_stream(chandler, content);

	size_t sz;
	int ret;
	char buf[1024];

	parseread.state = STATE_NEWLINE;
	parseread.cremnant = 0;
	parseread.fd = packfd;

	while((sz = fread(buf, 1, 1024, packptr)) > 0) {
		ret = proto_process_pack(buf, 1, sz, &parseread);
		if (ret != sz) {
			fprintf(stderr, "Error parsing http response. Exiting.\n");
			exit(128);
		}
	}

	fclose(packptr);
	free(content);
}

int
clone_generic(struct clone_handler *chandler, char *repodir, struct smart_head *smart_head)
{
	int packfd;
	int offset;
	int idxfd;
	struct packfileinfo packfileinfo;
	struct index_entry *index_entry;
	char path[PATH_MAX];
	char srcpath[PATH_MAX];
	int ret;
	FILE *stream;
	char *response;
	char *newpath;
	SHA1_CTX packctx;
	SHA1_CTX idxctx;

	strlcpy(path, dotgitpath, PATH_MAX);

	strlcat(path, "objects/pack/_tmp.pack", PATH_MAX);
	packfd = open(path, O_RDWR | O_CREAT, 0660);
	if (packfd == -1) {
		fprintf(stderr, "1 Unable to open file %s.\n", path);
		exit(-1);
	}
again:
	response = NULL;
	stream = chandler->run_service(chandler, "git-upload-pack");
	if (stream == NULL) {
		/* Fortunately, we can assume fetch_uri length will always be > 4 */
		if (strncmp(*chandler->path + strlen(*chandler->path) - 4, ".git", 4)) {
			/* We'll try again with .git (+ null terminator) */
			newpath  = malloc(strlen(*chandler->path) + 5);
			strncpy(newpath, *chandler->path, strlen(*chandler->path));
			strncat(newpath, ".git", strlen(*chandler->path) + 5);
			free(*chandler->path);
			*chandler->path = newpath;
			goto again;
		}

		fprintf(stderr, "Unable to clone repository.\n");
		ret = 128;
		goto out;
	}

	int pktsize;
	char out[8000];
	int r;
	bool inservice = false;
	int added = 0;

	do {
		/* Read the packet size */
		r = fread(out, 1, PKTSIZELEN, stream);
		if (r != PKTSIZELEN) {
			perror("Read a 4 size, probably an error.");
			exit(0);
		}
		out[PKTSIZELEN] = '\0';
		pktsize = (int)strtol(out, NULL, 16);

		if (inservice == false) {
			/* Break if the packet was 0. */
			if (pktsize == 0)
				break;
			r += fread(out + PKTSIZELEN, 1, pktsize - PKTSIZELEN, stream);
			if (r != pktsize) {
				printf("Failed to read the size, expected %d, read %d, exiting.\n", pktsize, r);
				exit(0);
			}
			if (strncmp(out+PKTSIZELEN, "# service=", 10) == 0)
				inservice = true;
		} else /* Reset the inservice flag */
			inservice = false;

		/* Append read bytes to the end */
		response = realloc(response, added + r);
		memcpy(response + added, out, r);
		added += r;
	} while (r >= PKTSIZELEN);

	response = realloc(response, added + r);
	memcpy(response + added, PKTFLUSH, PKTSIZELEN);

	fclose(stream);

	ret = proto_parse_response(response, smart_head);
	clone_generic_get_pack(chandler, packfd, smart_head);

	/* Jump to the beginning of the file */
	lseek(packfd, 0, SEEK_SET);

	SHA1_Init(&packctx);
	SHA1_Init(&idxctx);

	offset = pack_parse_header(packfd, &packfileinfo, &packctx);
	index_entry = malloc(sizeof(struct index_entry) * packfileinfo.nobjects);
	offset = pack_get_object_meta(packfd, offset, &packfileinfo, index_entry,
	    &packctx, &idxctx);
	close(packfd);

	SHA1_Final(packfileinfo.sha, &packctx);

	/* Sort the index entry */
	qsort(index_entry, packfileinfo.nobjects, sizeof(struct index_entry),
	    sortindexentry);

	strncpy(path, dotgitpath, PATH_MAX);
	strncat(path, "objects/pack/_tmp.idx", PATH_MAX);
	idxfd = open(path, O_RDWR | O_CREAT, 0660);
	if (idxfd == -1) {
		fprintf(stderr, "Unable to open packout.idx for writing.\n");
		ret = -1;
		goto out;
	}

	pack_build_index(idxfd, &packfileinfo, index_entry, &idxctx);
	free(index_entry);
	close(idxfd);

	char *suffix = path;
	strncpy(path, dotgitpath, PATH_MAX);
	suffix += strlcat(path, "objects/pack/pack-", PATH_MAX);
	for(int x=0;x<20;x++)
		snprintf(suffix+(x*2), 3, "%02x", packfileinfo.sha[x]);

	/* Rename pack and index files */
	strlcat(suffix, ".pack", PATH_MAX);

	strncpy(srcpath, dotgitpath, PATH_MAX);
	strlcat(srcpath, "objects/pack/_tmp.pack", PATH_MAX);
	rename(srcpath, path);

	strlcpy(srcpath+strlen(srcpath)-4, "idx", 4);
	strlcpy(path+strlen(path)-4, "idx", 4);
	rename(srcpath, path);
	ret = 0;
out:
	return (ret);
}


int
clone_main(int argc, char *argv[])
{
	struct smart_head smart_head;
	char *repodir;
	char *repopath;
	char *uri;
	char inodepath[PATH_MAX];
	struct clone_handler *chandler;
	struct indextree indextree;
	struct indexpath indexpath;
	struct treeleaf treeleaf;
	struct decompressed_object decompressed_object;
	struct commitcontent commitcontent;
	int nch, ret = 0;
	int packfd;
	int ch;
	int e;
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
			return (-1);
		}
	argc = argc - q;
	argv = argv + q;

	uri = repopath = argv[1];
	repodir = get_repo_dir(argv[1]);

	/* Set dotgitpath */
	strlcpy(dotgitpath, repodir, PATH_MAX);
	strlcat(dotgitpath, "/.git/", PATH_MAX);

	found = false;
	chandler = NULL;
	for (nch = 0; nch < nitems(clone_handlers); ++nch) {
		chandler = &clone_handlers[nch];
		assert(chandler->matcher != NULL);

		if (chandler->matcher(chandler, uri)) {
			found = true;
			break;
		}
	}

	/* This will become stale when we setup the default handler... (ssh?) */
	if (!found && default_handler != NULL) {
		fprintf(stderr, "URI Scheme not recognized for '%s'\n", uri);
		exit(128);
	} else if (!found) {
		chandler = default_handler;
	}
	/* Make directories */
	if (mkdir(repodir, 0755)) {
		fprintf(stderr, "Unable to create repodir: %s\n", repodir);
		exit(0);
	}
	if (mkdir(dotgitpath, 0755)) {
		fprintf(stderr, "Unable to make dotgitpath: %s\n", dotgitpath);
		exit(0);
	}

	/* Make initial directories */
	init_dirinit(0);

	smart_head.refs = NULL;
	STAILQ_INIT(&smart_head.symrefs);
	ret = clone_generic(chandler, repodir, &smart_head);
	if (ret != 0)
		goto out;

	/* Populate .git/pack-refs */
	populate_packed_refs(repodir, &smart_head);

	if (!STAILQ_EMPTY(&smart_head.symrefs))
		populate_symrefs(repodir, &smart_head);
	/* Write the initial config file */
	clone_initial_config(uri, repodir, NULL);

	/* Retrieve the commit header and parse it out */
	CONTENT_HANDLER(smart_head.sha, buffer_cb, pack_buffer_cb, &decompressed_object);
	parse_commitcontent(&commitcontent, (char *)decompressed_object.data, decompressed_object.size);

	strlcpy(inodepath, repodir, PATH_MAX);
	ITERATE_TREE(commitcontent.treesha, generate_tree_item, inodepath);

	indextree.version = INDEX_VERSION_2;
	indextree.entries = 0;
	indexpath.indextree = &indextree;
	e = snprintf(inodepath, PATH_MAX, "%s/", repodir);
	indexpath.fullpath = inodepath;
	indexpath.path = (char *)inodepath + e;

	indextree.dircleaf = NULL;
	indextree.treeleaf = &treeleaf;

	/* Terminate the string */
	indexpath.path[0] = '\0';

	ITERATE_TREE(commitcontent.treesha, index_generate_indextree, &indexpath);

	treeleaf.entry_count = 0;
	treeleaf.local_tree_count = 0;
	treeleaf.total_tree_count = 0;
	treeleaf.subtree = NULL;
	sha_str_to_bin(commitcontent.treesha, treeleaf.sha);

	indexpath.current_position = 0;

	treeleaf.ext_size = 0;
	ITERATE_TREE(commitcontent.treesha, index_generate_treedata, &indexpath);

	index_calculate_tree_ext_size(&treeleaf);

	strlcpy(inodepath, dotgitpath, PATH_MAX);
	strlcat(inodepath, "/index", PATH_MAX);
	packfd = open(inodepath, O_CREAT|O_RDWR, 0666);
	if (packfd == -1) {
		printf("Error with /tmp/mypack");
		exit(0);
	}
	index_write(&indextree, packfd);

out:
	free(repodir);
	free(treeleaf.subtree); /* Allocated by index_generate_treedata */

	return (ret);
}
