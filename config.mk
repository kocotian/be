# version
VERSION = 0.1

# includes
INC = -I.

# flags
CFLAGS = ${INC} -Wall -Wextra -Wconversion -std=c99 -pedantic
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -DVERSION=\"${VERSION}\"
FLAGS = ${CFLAGS} ${CPPFLAGS}

# compiler
CC = gcc
