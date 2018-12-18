MAN=

CFLAGS=	-Wall -lmd -lz -lfetch -lcurl -I /usr/local/include -L /usr/local/lib

PROG=	ogit	

SRCS=	ogit.c ini.c index.c common.c remote.c init.c hash_object.c \
	update_index.c cat_file.c pack.c log.c zlib_handler.c clone.c \
	index_pack.c

CLEANFILES+=	${PROG}.core

.include <bsd.prog.mk>
