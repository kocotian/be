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

#include <ctype.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <str.h>
#include <util.h>

/* types */
typedef enum Mod {
	ModNone = 0, ModControl = 0x1f, ModShift = 0xdf,
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
	char path[PATH_MAX], name[PATH_MAX];
	int anonymous, dirty;
	int x, y;
} Buffer;

/* prototypes */
static void rawOn(void);
static void rawRestore(void);
static void getws(int *r, int *c);
/*********/
static void termRefresh(void);
static void appendRows(String *ab);
/*********/
static void abAppend(String *ab, const char *str, size_t len);
static void abFree(String *ab);
/*********/
static unsigned char editorGetKey(void);
static void editorParseKey(unsigned char key);
static void edit(void);
/*********/
static void newBuffer(Buffer *buf);
/*********/
static void setup(void);
static void finish(void);
/*********/
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
	abAppend(&ab, cp, (unsigned)snprintf(cp, 19, "\033[%4d;%4dH\033[?25h",
				((terminal.r - 1) / 2), editor.bufs.data[editor.curbuf].x + 1));

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
	/* TODO: information that key is not bound */
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
	buf->anonymous = buf->dirty = 1;
	buf->x = buf->y = 0;

	if (buf == &init)
		pushVector(editor.bufs, init);
}

/* other */
static void
setup(void)
{
	rawOn();
	getws(&(terminal.r), &(terminal.c));
	newVector(editor.bufs);
	newBuffer(NULL);
	editor.curbuf = 0;
}

static void
finish(void)
{
	rawRestore();
	termRefresh();
	exit(0);
}

/* editor functions */
static void
quit(const Arg *arg)
{
	(void)arg;
	finish();
}

int
main(int argc, char *argv[])
{
	setup();
	edit();
	finish();

	return 0;
}
