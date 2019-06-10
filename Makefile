# $FreeBSD$

.PATH: ${.CURDIR}/lib

MAN=
CFLAGS+= -Wall
LDFLAGS= -lmd -lz -lfetch

PROG=	ogit

SRCS=	ogit.c ini.c index.c common.c pack.c remote.c init.c \
	zlib-handler.c buffering.c loose.c \
	hash-object.c update-index.c cat-file.c log.c clone.c index-pack.c

CLEANFILES+=	${PROG}.core

HAS_TESTS=
# XXX TODO: MK_TESTS
SUBDIR.yes+=	tests

.include <bsd.prog.mk>
