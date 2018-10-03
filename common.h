#ifndef __COMMON_H__
#define __COMMON_H__

#define nitems(x)	(sizeof((x)) / sizeof((x)[0]))

extern char dotgitpath[PATH_MAX + NAME_MAX];
int git_repository_path();

#endif
