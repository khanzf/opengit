#ifndef __ZLIB_HANDLER__H
#define __ZLIB_HANDLER__H

#define CHUNK 16384

struct writer_args {
	int fd;
	long sent;
};

typedef unsigned char *inflated_handler(unsigned char *, int size, void *data);

int deflate_caller(int sourcefd, inflated_handler inflated_handler, void *arg);
unsigned char *write_callback(unsigned char *buf, int size, void *arg);

#endif
