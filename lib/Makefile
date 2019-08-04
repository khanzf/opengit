
LIB=	ogit
SHLIB_MAJOR=	0
SHLIB_MINOR=	0
SRCS=	buffering.c common.c index.c ini.c loose.c pack.c protocol.c zlib-handler.c

.if defined(DEBUG)
CFLAGS+=        -Wall -Wunreachable-code -Werror -O0 -g
.endif
CFLAGS+=	-Wall

.include <bsd.lib.mk>
