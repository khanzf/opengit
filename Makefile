MAN=

CFLAGS=	-Wall -lmd -lz -lfetch

PROG=	ogit	

SRCS=	ogit.c lib/ini.c lib/index.c lib/common.c lib/pack.c remote.c init.c \
	lib/zlib_handler.c \
	hash_object.c update_index.c cat_file.c log.c clone.c index_pack.c

CLEANFILES+=	${PROG}.core

.include <bsd.prog.mk>
