/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Farhan Khan. All rights reserved.
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

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <fetch.h>
#include <errno.h>
#include <regex.h>
#include "lib/pack.h"
#include "lib/protocol.h"
#include "clone.h"

/* Match the ssh scheme */
int
match_ssh(struct clone_handler *chandler, char *uri)
{
#define MAXGROUPS 5
	regex_t gitpath;
	regmatch_t m[MAXGROUPS];
	int r;

	r = regcomp(&gitpath, "(([a-z0-9_-]{0,31})@)?(.*):([a-zA-z\\/]+)(\\.git)?", REG_EXTENDED);
	if (r != 0) {
		fprintf(stderr, "Unable to compile regex, exiting.\n");
		exit(r);
	}

	r = regexec(&gitpath, uri, MAXGROUPS, m, 0);
	if (r == 0) {
		struct conn_ssh *conn_ssh = malloc(sizeof(struct conn_ssh));
		conn_ssh->ssh_user = strndup(uri+m[2].rm_so, m[2].rm_eo - m[2].rm_so);
		conn_ssh->ssh_host = strndup(uri+m[3].rm_so, m[3].rm_eo - m[3].rm_so);
		conn_ssh->ssh_path = strndup(uri+m[4].rm_so, m[4].rm_eo - m[4].rm_so);
		conn_ssh->port = SSH_PORT;
		conn_ssh->bpos = 0;
		conn_ssh->buf = NULL;

		conn_ssh->path = &conn_ssh->ssh_path;
		chandler->conn_data = conn_ssh;
		chandler->path = &conn_ssh->ssh_path;
		return 1;
	}
	return 0;
}

void
setup_connection(struct clone_handler *chandler)
{
	struct conn_ssh *conn_ssh = chandler->conn_data;
	int filedes1[2], filedes2[2], filedes3[2];
	int pid;

	if (pipe(filedes1) == -1)
		goto out;
	if (pipe(filedes2) == -1)
		goto out;
	if (pipe(filedes3) == -1)
		goto out;

	pid = fork();

	if (pid == 0) {
		if (dup2(filedes1[0], STDIN_FILENO) == -1)
			goto out;
		if (dup2(filedes2[1], STDOUT_FILENO) == -1)
			goto out;
		if (dup2(filedes3[1], STDERR_FILENO) == -1)
			goto out;
		execl("/usr/bin/ssh", "ssh", "-l", "git", "github.com", "git-upload-pack", conn_ssh->ssh_path, NULL);
	}
	else {
		close(filedes1[0]);
		close(filedes2[1]);
		close(filedes3[0]);
		conn_ssh->out = filedes1[1];
		conn_ssh->in = filedes2[0];
		conn_ssh->err = filedes3[1];
	}

	return;
out:
	fprintf(stderr, "Unable to setup connection, exiting.\n");
	exit(-1);
}

static int
ssh_readfn(void *v, char *buf, int len)
{
	struct conn_ssh *conn_ssh = v;
	int b, rlen;

	if (conn_ssh->bsize == 0 || conn_ssh->buf == NULL || conn_ssh->bsize == conn_ssh->bpos) {
		conn_ssh->buf = realloc(conn_ssh->buf, len);
		b = read(conn_ssh->in, conn_ssh->buf, len);
		conn_ssh->bsize = b;
		conn_ssh->bpos = 0;
	}

	rlen = conn_ssh->bsize - conn_ssh->bpos;
	if (len < rlen)
		rlen = len;

	memcpy(buf, conn_ssh->buf - conn_ssh->bpos, len);
	conn_ssh->bpos += rlen;

	return (rlen);
}

static int
ssh_writefn(void *v, const char *buf, int len)
{
	fprintf(stderr, "Writing currently unimplemented, exiting\n");
	exit(0);
	return (0);
}

FILE *
ssh_run_service(struct clone_handler *chandler, char *service)
{
	FILE *stream;
	//struct conn_ssh *conn_ssh = chandler->conn_data;

	setup_connection(chandler);
	stream = funopen(chandler->conn_data, ssh_readfn, ssh_writefn, NULL, NULL);

	if (stream == NULL) {
		fprintf(stderr, "Failed to open stream, exiting.\n");
		exit(-1);
	}

	return stream;
}

FILE *
ssh_get_pack_stream(struct clone_handler *chandler, char *content)
{
	struct conn_ssh *conn_ssh = chandler->conn_data;
	write(conn_ssh->out, content, strlen(content));
	return fdopen(conn_ssh->in, "r");
}
