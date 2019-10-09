
MAN=
.if defined(NDEBUG)
# Debugging compiles in gcc because it works better with valgrind
CC=		gcc
CFLAGS+=	-DNDEBUG -g -Wall -Wunreachable-code -Werror -O0
.endif
CFLAGS+=	-Wall -I${.CURDIR}/..
# Currently statically linking against libogit.a library
LDADD+=		${.OBJDIR}/../lib/libogit.a
LDFLAGS+=	-lmd -lz -lfetch

PROG=		ogit

SRCS=		ogit.c remote.c init.c hash-object.c update-index.c log.c \
		cat-file.c clone.c clone_http.c clone_ssh.c index-pack.c

CLEANFILES+=	${PROG}.core

# These tests rely on Kyua/atf bits
.if .FreeBSD
HAS_TESTS=
# XXX TODO: MK_TESTS
SUBDIR.yes+=	tests
.endif

.include <bsd.prog.mk>
