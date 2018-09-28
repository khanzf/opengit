#ifndef _INI_H_
#define _INI_H_

enum section_types {
	CORE,
	REMOTE,
	BRANCH
};

enum boolean {
	TRUE,
	FALSE
};

struct section {
	enum section_types 	section_type;

	int 			repositoryformatversion;
	enum boolean		filemode;
	enum boolean		bare;
	enum boolean		logallrefupdates;

	char *			url;
	char *			fetch;

	struct section *next;
};

int	config_parser();
void	ini_init_regex();

#endif
