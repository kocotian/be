include config.mk

SRC = util.c
OBJ = ${SRC:.c=.o}

.c.o:
	${CC} ${CFLAGS} -c -o $@ $^

be: be.c ${OBJ}
	${CC} ${CFLAGS} -o $@ $^
