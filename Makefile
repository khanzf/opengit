MAN=

CFLAGS=	-Wall -lmd -lz -lfetch

PROG=	ogit	

SRCS=	ogit.c lib/ini.c lib/index.c lib/common.c lib/pack.c remote.c init.c \
	lib/zlib-handler.c lib/buffering.c \
	hash-object.c update-index.c cat-file.c log.c clone.c index-pack.c

CLEANFILES+=	${PROG}.core

.include <bsd.prog.mk>
