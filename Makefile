MAN=

CFLAGS=	-Wall -lmd -lz

PROG=	ogit	

SRCS=	ogit.c ini.c index.c common.c remote.c init.c hash_object.c update_index.c cat_file.c pack.c log.c zlib_handler.c clone.c

CLEANFILES+=	${PROG}.core

.include <bsd.prog.mk>
