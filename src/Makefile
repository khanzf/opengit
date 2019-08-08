
MAN=
.if defined(DEBUG)
CFLAGS+=	-Wall -Wunreachable-code -Werror -O0 -g
.endif
CFLAGS+=	-Wall -I${.CURDIR}/..
LDFLAGS+=	-lmd -lz -lfetch
LDFLAGS+=	${.OBJDIR}/../lib/libogit.a

PROG=	ogit

SRCS=	ogit.c remote.c init.c hash-object.c update-index.c cat-file.c log.c \
    clone.c clone_http.c clone_ssh.c index-pack.c

CLEANFILES+=	${PROG}.core

# These tests rely on Kyua/atf bits
.if .FreeBSD
HAS_TESTS=
# XXX TODO: MK_TESTS
SUBDIR.yes+=	tests
.endif

.include <bsd.prog.mk>
