/*
   be - based editor - simple editor inspired by ed, vi, emacs and acme.
   Copyright (C) 2021  Kacper Kocot <kocotian@kocotian.pl>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/limits.h>
#elif __FreeBSD__
#include <sys/syslimits.h>
#elif __OpenBSD__
#include <limits.h>
#else
#define UNLIMITED
#endif

#include <arg.h>
#include <str.h>
#include <util.h>

#define CURBUF(EDITOR) ((EDITOR).bufs.data[(EDITOR).curbuf])
#ifdef UNLIMITED
#define PATH_MAX 1024
#endif

/* types */
typedef enum Mod {
	ModNone = 0xff, ModControl = 0x1f, ModShift = 0xdf,
} Mod;

typedef union Arg {
	int i;
	unsigned int ui;
	char c;
	float f;
	const void *v;
} Arg;

typedef struct Key {
	Mod mod;
	unsigned char key;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct Buffer {
	Array(String) rows;
	char path[PATH_MAX], name[NAME_MAX];
	int anonymous, dirty;
	ssize_t x, y;
} Buffer;

/* prototypes */
static void rawOn(void);
static void rawRestore(void);
static void getws(int *r, int *c);
/*********/
static void termRefresh(void);
static void appendRows(String *ab);
static void appendContents(String *ab);
static void appendStatus(String *ab);
/*********/
static void abAppend(String *ab, const char *str, size_t len);
#define abPrintf(AB, CP, CPLEN, ...) \
	(abAppend((AB), (CP), (unsigned)snprintf((CP), (CPLEN), __VA_ARGS__)))
static void abFree(String *ab);
/*********/
static unsigned char editorGetKey(void);
static void editorParseKey(unsigned char key);
static void edit(void);
/*********/
static void newBuffer(Buffer *buf);
static void minibufferPrint(const char *s);
static void minibufferError(const char *s);
/*********/
static void setup(char *filename);
static void finish(void);
static void usage(void);
/*********/
static void echo(const Arg *arg);
static void cursormove(const Arg *arg);
static void quit(const Arg *arg);

/* global variables */
#include "config.h"
static struct {
	struct termios origtermios;
	int r, c;
} terminal;

static struct {
	Array(Buffer) bufs;
	int curbuf;
} editor;

char *argv0;

/* terminal */
static void
rawOn(void)
{
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &(terminal.origtermios)) < 0)
		die("tcgetattr:");
	raw = (terminal.origtermios);
	raw.c_cflag |= (CS8);
	raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_oflag &= (tcflag_t)~(OPOST);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
		die("tcsetattr:");
}

static void
rawRestore(void)
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &(terminal.origtermios)) < 0)
		die("tcsetattr:");
}

static void
getws(int *r, int *c)
{
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
		die("ioctl:");
	if (ws.ws_col < 20 || ws.ws_row < 3)
		die("screen is too small (min. 20x3)");
	*c = ws.ws_col;
	*r = ws.ws_row;
}

/* output */
static void
termRefresh(void)
{
	String ab = { NULL, 0 };
	char cp[19];

	abAppend(&ab, "\033[?25l\033[H", 9);
	appendRows(&ab);
	appendContents(&ab);
	appendStatus(&ab);
	abPrintf(&ab, cp, 19, "\033[%4d;%4ldH\033[?25h",
			FOCUSPOINT, CURBUF(editor).x + 1);

	if ((unsigned)write(STDOUT_FILENO, ab.data, ab.len) != ab.len)
		die("write:");

	abFree(&ab);
}

static void
appendRows(String *ab)
{
	size_t y;
	for (y = 0; y < (unsigned)(terminal.r - 2); ++y)
		abAppend(ab, "\033[K~\r\n", (y < (unsigned)(terminal.r - 3)) ? 6 : 4);
}

static void
appendContents(String *ab)
{
	ssize_t y;
	char cp[19];
	for (y = 0; y < terminal.r - 1; ++y) {
		if (y + CURBUF(editor).y - FOCUSPOINT < CURBUF(editor).rows.len) {
			abPrintf(ab, cp, 19, "\033[%4ld;0H\033[K",
					y);
			abAppend(ab,
					CURBUF(editor).rows.data[y + CURBUF(editor).y - FOCUSPOINT].data,
					CURBUF(editor).rows.data[y + CURBUF(editor).y - FOCUSPOINT].len);
		}
	}
}

static void
appendStatus(String *ab)
{
	char cp[256];
	ssize_t i;
	i = -1;
	abPrintf(ab, cp, 256, "\033[%4d;0H\033[K\033[0;97;100m",
			(unsigned)(terminal.r - 1));
	while (++i < terminal.c) {
		abAppend(ab, " ", 1);
	}
	abAppend(ab, "\r", 1);
	/* status drawing */
	{
		abPrintf(ab, cp, 256, "%s L%ld",
				CURBUF(editor).anonymous ?
					"*anonymous*" : CURBUF(editor).name,
				CURBUF(editor).y + 1);
	}
	abAppend(ab, "\r\033[0m", 6);
}

/* append buffer */
static void
abAppend(String *ab, const char *str, size_t len)
{
	char *nb;
	if ((nb = realloc(ab->data, ab->len + len)) == NULL)
		return;
	memcpy((ab->data = nb) + ab->len, str, len);
	ab->len += len;
}

static void
abFree(String *ab)
{
	free(ab->data);
}

/* editor */
static unsigned char
editorGetKey(void)
{
	ssize_t rb;
	unsigned char c;

	while ((rb = read(STDIN_FILENO, &c, 1)) != 1)
		if (rb < 0 && errno != EAGAIN)
			die("read:");
	return c;
}

static void
editorParseKey(unsigned char key)
{
	size_t i;
	for (i = 0; i < LEN(bindings); ++i)
		if (key == (bindings[i].key & bindings[i].mod)) {
			(bindings[i].func)(&(bindings[i].arg));
			return;
		}
	minibufferError("Key not bound");
}

static void
edit(void)
{
	while (1) {
		termRefresh();
		editorParseKey(editorGetKey());
	}
}

/* buffers */
static void
newBuffer(Buffer *buf)
{
	Buffer init;
	String ln = {
		"", 0
	};

	if (buf == NULL)
		buf = &init;

	newVector(buf->rows);
	pushVector(buf->rows, ln);
	*(buf->path) = *(buf->name) = '\0';
	buf->anonymous = 1;
	buf->dirty = 0;
	buf->x = buf->y = 0;

	if (buf == &init)
		pushVector(editor.bufs, init);
}

static void
editBuffer(Buffer *buf, char *filename)
{
	Buffer init;
	int fd;
	struct stat sb;
	char *data;
	String fstr, fline, fpush;

	if (buf == NULL)
		buf = &init;

	newVector(buf->rows);
	*(buf->path) = *(buf->name) = '\0';
	strncpy(buf->path, filename, PATH_MAX);
	strncpy(buf->name,
			(strrchr(filename, '/')) == NULL ?
				filename : (strrchr(filename, '/') + 1),
			NAME_MAX);
	buf->anonymous = buf->dirty = 0;
	buf->x = buf->y = 0;

	if ((fd = open(filename, O_RDONLY)) < 0)
		die("open:");

	if (fstat(fd, &sb) < 0)
		die("stat:");

	/* maybe mmap will be better here? */
	data = malloc(((unsigned)sb.st_size * sizeof *data));
	if (read(fd, data, (unsigned)sb.st_size) != (unsigned)sb.st_size)
		die("read:");

	fstr.data = data;
	fstr.len = (unsigned)sb.st_size;

	while (Strtok(fstr, &fline, '\n') > 0) {
		fpush.data = strndup(fline.data, (fpush.len = fline.len));
		pushVector(buf->rows, fpush);
		fstr.data += fline.len;
		fstr.len -= fline.len;
	}

	free(data);

	if (buf == &init)
		pushVector(editor.bufs, init);
}

static void
minibufferPrint(const char *s)
{
	String ab = { NULL, 0 };
	char cp[20];

	abPrintf(&ab, cp, 20, "\033[%4d;%4dH\033[0m\033[K",
			terminal.r, 1);

	abAppend(&ab, s, strlen(s));

	abAppend(&ab, "\033[0m", 4);

	if ((unsigned)write(STDOUT_FILENO, ab.data, ab.len) != ab.len)
		die("write:");

	abFree(&ab);
}

static void
minibufferError(const char *s)
{
	String ab = { NULL, 0 };
	char cp[23];

	abPrintf(&ab, cp, 23, "\033[%4d;%4dH\033[0;31m\033[K",
			terminal.r, 1);

	abAppend(&ab, s, strlen(s));

	abAppend(&ab, "\033[0m", 4);

	if ((unsigned)write(STDOUT_FILENO, ab.data, ab.len) != ab.len)
		die("write:");

	abFree(&ab);
}

/* other */
static void
setup(char *filename)
{
	rawOn();
	getws(&(terminal.r), &(terminal.c));
	newVector(editor.bufs);
	if (filename == NULL)
		newBuffer(NULL);
	else
		editBuffer(NULL, filename);
	editor.curbuf = 0;
	minibufferPrint("Welcome to be");
}

static void
finish(void)
{
	rawRestore();
	termRefresh();
	exit(0);
}

static void
usage(void)
{
	die("usage: %s [-h] [FILE]", argv0);
}

/* editor functions */
static void
echo(const Arg *arg)
{
	minibufferPrint(arg->v);
}

static void
cursormove(const Arg *arg)
{
	switch (arg->i) {
	case 0: /* left */
		if (CURBUF(editor).x > 0)
			--(CURBUF(editor).x);
		else
			minibufferPrint("Already on beginning of line");
		break;
	case 1: /* down */
		if (CURBUF(editor).y < (signed)CURBUF(editor).rows.len - 1)
			++(CURBUF(editor).y);
		else
			minibufferPrint("Already on bottom");
		break;
	case 2: /* up */
		if (CURBUF(editor).y > 0)
			--(CURBUF(editor).y);
		else
			minibufferPrint("Already on top");
		break;
	case 3: /* up */
		if (CURBUF(editor).x < (signed)CURBUF(editor).rows.data[CURBUF(editor).y].len - 1)
			++(CURBUF(editor).x);
		else
			minibufferPrint("Already on end of line");
		break;
	}
}

static void
quit(const Arg *arg)
{
	(void)arg;
	finish();
}

int
main(int argc, char *argv[])
{
	char *filename;
	ARGBEGIN {
	case 'h': default: /* fallthrough */
		usage();
		break;
	} ARGEND

	filename = NULL;

	if (argc > 1)
		usage();
	else if (argc == 1)
		filename = argv[0];

	setup(filename);
	edit();
	finish();

	return 0;
}
