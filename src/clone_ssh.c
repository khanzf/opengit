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
#include "lib/pack.h"
#include "lib/protocol.h"
#include "clone.h"

/* Match the http(s)? scheme */
int
match_ssh(struct clone_handler *chandler, char *uri)
{
	/* XXX This is a temporary matching mechanism, long-term is a regex */
	if (strncmp(uri, "git@", 4) == 0) {
		struct conn_ssh *conn_ssh = malloc(sizeof(struct conn_ssh));
		conn_ssh->ssh_user = "git";
		conn_ssh->ssh_host = "github.com";
		conn_ssh->port = 22;
		conn_ssh->ssh_path = "khanzf/opengit";

		conn_ssh->path = &conn_ssh->ssh_path;
		chandler->conn_data = conn_ssh;
		return 1;
	}
	return 0;
}

void
setup_connection(struct clone_handler *chandler)
{
	int filedes1[2], filedes2[2], filedes3[2];
	int pid;

	pipe(filedes1);
	pipe(filedes2);

	pid = fork();

	if (pid == 0) {
		dup2(filedes1[0], STDIN_FILENO);
		dup2(filedes2[1], STDOUT_FILENO);
		dup2(filedes2[1], STDERR_FILENO);
		execl("/usr/bin/ssh", "ssh", "git@github.com", "git-upload-pack", "khanzf/opengit", NULL);
	}
	else {
		struct conn_ssh *conn_ssh = chandler->conn_data;
		close(filedes1[0]);
		close(filedes2[1]);
		close(filedes3[0]);
		conn_ssh->out = filedes1[1];
		conn_ssh->in = filedes2[0];
		conn_ssh->err = filedes3[1];

	}
}

int
ssh_get_repo_state(struct clone_handler *chandler, char **response)
{
	long offset = 0;
	long r;
	char out[1024];
	struct conn_ssh *conn_ssh = chandler->conn_data;

	setup_connection(chandler);
	do {
		r = read(conn_ssh->in, out, 1024);
		*response = realloc(*response, offset+r);
		memcpy(*response+offset, out, r);
		offset += r;
	} while(r >= 1024);

	return (0);
}

FILE *
ssh_get_pack_stream(struct clone_handler *chandler, char *content)
{
	struct conn_ssh *conn_ssh = chandler->conn_data;
	write(conn_ssh->out, content, strlen(content));
	return fdopen(conn_ssh->in, "r");
}
