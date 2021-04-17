include config.mk

be: be.c
	${CC} ${CFLAGS} -o $@ $^
