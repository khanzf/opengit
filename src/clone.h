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


#ifndef __CLONE_H__
#define __CLONE_H__

#include <sys/queue.h>
#include "lib/common.h"
#include "lib/protocol.h"

struct clone_handler;

/* uri, destdir, smart_head */
typedef int (*clone_uri_scheme)(struct clone_handler *, char *);
typedef int (*clone_get_repo_state)(struct clone_handler *, char **);
typedef FILE *(*clone_get_pack_fptr)(struct clone_handler *, char *);

struct clone_handler {
	clone_uri_scheme	  matcher;
	clone_get_repo_state	  get_repo_state;
	clone_get_pack_fptr	  get_pack_fptr;

	void			 *conn_data;

	char			**path;
};

/* Holds ssh connection data */
struct conn_ssh {
	int		  fdout;
	int		  fdin;
	int		  fderr;

	char		 *ssh_user;
	char		 *ssh_host;
	int		  port;
	char		 *ssh_path;
	int		  in;
	int		  out;
	int		  err;

	char		**path;
};

struct branch {
	char 		sha[41];
	char 		*name;
};

extern struct clone_handler http_handler;

/* HTTP and HTTPS handler functions */
int	 match_http(struct clone_handler *chandler, char *uri);
int	 http_get_repo_state(struct clone_handler *chandler, char **response);
FILE	*http_get_pack_fptr(struct clone_handler *chandler, char *content);


/* ssh handler functions */
int	 match_ssh(struct clone_handler *chandler, char *uri);
int	 ssh_get_repo_state(struct clone_handler *chandler, char **response);
FILE	*ssh_get_pack_fptr(struct clone_handler *chandler, char *content);

int	 clone_main(int argc, char *argv[]);

#endif
