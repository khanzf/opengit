#ifndef __ZLIB_HANDLER__H
#define __ZLIB_HANDLER__H

#define CHUNK 16384

struct writer_args {
	int fd;
	long sent;
};

typedef unsigned char *inflated_handler(unsigned char *, int size, void *data);

int deflate_caller(int sourcefd, inflated_handler inflated_handler, void *arg);
int zlib_get_loose_object_type(unsigned char *buf, int size, void *data);
int zlib_deliver_loose_object_content(unsigned char *buf, int size, void *data);
unsigned char *write_callback(unsigned char *buf, int size, void *arg);

#endif
