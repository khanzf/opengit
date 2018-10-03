MAN=

CFLAGS=	-Wall -lmd -lz

PROG=	ogit	

SRCS=	ogit.c ini.c common.c remote.c init.c hash_object.c

.include <bsd.prog.mk>
