#ifndef _OGIT_H_
#define _OGIT_H_

#include <limits.h>

struct cmd {
	const char *c_arg;
	int (*c_func)(int argc, char *argv[]);
};

static char dotgitpath[PATH_MAX + NAME_MAX];

extern	int cmd_count;

extern int log_main();
extern int remote_main();

#endif
