# $FreeBSD$

.PATH: ${.CURDIR}/lib

MAN=
CFLAGS+= -Wall
LDFLAGS= -lmd -lz -lfetch

PROG=	ogit

SRCS=	cat-file.c clone.c hash-object.c index-pack.c init.c log.c \
	ogit.c remote.c update-index.c

SRCS+=	buffering.c common.c index.c ini.c loose.c pack.c zlib-handler.c

CLEANFILES+=	${PROG}.core

HAS_TESTS=
# XXX TODO: MK_TESTS
SUBDIR.yes+=	tests

.include <bsd.prog.mk>
