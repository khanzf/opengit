#ifndef _OGIT_H_
#define _OGIT_H_

struct cmd {
	const char *c_arg;
	int (*c_func)();
};

extern	int cmd_count;

extern int log_main();
extern int remote_main();

#endif
