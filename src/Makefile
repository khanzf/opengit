# $FreeBSD$

.PATH: ${.CURDIR}/lib

MAN=
CFLAGS+= -Wall
LDFLAGS= -lmd -lz -lfetch

PROG=	ogit

SRCS=	ogit.c remote.c init.c hash-object.c update-index.c cat-file.c log.c \
    clone.c clone_http.c index-pack.c

CLEANFILES+=	${PROG}.core

HAS_TESTS=
# XXX TODO: MK_TESTS
SUBDIR.yes+=	tests

.include <bsd.prog.mk>
