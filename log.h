#ifndef __LOG_H__
#define __LOG_H__

#define LOG_STATUS_COMMIT	0x00
#define LOG_STATUS_HEADERS	0x01
#define LOG_STATUS_PARENT	0x10
#define LOG_STATUS_AUTHOR	0x20

struct logarg {
	int type;
	int status;
	char sha[40];
	char next_commit[40];
	char *headers;
	int size;
};

int log_main(int argc, char *argv[]);

#endif
