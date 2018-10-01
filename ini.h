#ifndef _INI_H_
#define _INI_H_

enum section_types {
	CORE = 1,
	REMOTE = 2,
	BRANCH = 3,

	OTHER = 99
};

enum boolean {
	NOT_SET = 0,
	TRUE = 1,
	FALSE = 2
};

// XXX Could this be made a union?
struct section {
	enum section_types 	type;

	/* Used by main */
	int 			repositoryformatversion;
	enum boolean		filemode;
	enum boolean		bare;
	enum boolean		logallrefupdates;

	/* Used by remote */
	char *			repo_name;
	char *			url;
	char *			fetch;

	/* Other */
	char *			other_header_name;
	char *			other_variable;
	char *			other_value;

	struct section *next;
};

extern struct section *sections;

int	config_parser();
void	ini_init_regex();

#endif
