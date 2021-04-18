include config.mk

SRC = util.c str.c
OBJ = ${SRC:.c=.o}

.c.o:
	${CC} ${FLAGS} -c -o $@ $^

be: be.c ${OBJ}
	${CC} ${FLAGS} -o $@ $^
