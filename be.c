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
#define REPLACE (void *)(-1)
#define SUBMODES_MAX 32

#ifdef UNLIMITED
#define PATH_MAX 1024
#endif

/* types */
typedef enum Mod {
	ModNone = 0xff, ModControl = 0x1f, ModShift = 0xdf,
} Mod;

typedef enum Mode {
	ModeNormal, ModeEdit,
	SubModeGlobal,
	SubModeMovement,
	SubModeBuffer,
} Mode;

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
	Mode mode;
	Mode submodes[SUBMODES_MAX];
	size_t submodeslen;
} Buffer;

typedef struct Binding {
	Key *keys;
	size_t len;
} Binding;

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
static void switchmode(Mode mode);
/*********/
static void newBuffer(Buffer *buf);
static void freeBuffer(Buffer *buf);
static int writeBuffer(Buffer *buf, char *filename);
static int minibufferPrint(const char *s);
static int minibufferError(const char *s);
static inline int submodePush(Buffer *b, Mode m);
static inline int submodePop(Buffer *b);
/*********/
static void setup(char *filename);
static void finish(void);
static void usage(void);
/*********/
static void echo(const Arg *arg);
static void echoe(const Arg *arg);
static void normalmode(const Arg *arg);
static void insertmode(const Arg *arg);
static void appendmode(const Arg *arg);
static void globalsubmode(const Arg *arg);
static void buffersubmode(const Arg *arg);
static void cursormove(const Arg *arg);
static void beginning(const Arg *arg);
static void ending(const Arg *arg);
static void insertchar(const Arg *arg);
static void removechar(const Arg *arg);
static void openline(const Arg *arg);
static void bufwriteclose(const Arg *arg);
static void bufwrite(const Arg *arg);
static void bufclose(const Arg *arg);
static void bufkill(const Arg *arg);

/* global variables */
static struct {
	struct termios origtermios;
	int r, c;
} terminal;

static struct {
	Array(Buffer) bufs;
	int curbuf;
} editor;

char *argv0;

/* config */
#include "config.h"

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
		die(lang_err[1], 20, 3);
	*c = ws.ws_col;
	*r = ws.ws_row;
}

/* output */
static void
termRefresh(void)
{
	String ab = { NULL, 0 };
	char cp[24];

	abAppend(&ab, "\033[?25l\033[H", 9);
	appendRows(&ab);
	appendContents(&ab);
	appendStatus(&ab);
	abPrintf(&ab, cp, 24, "\033[%4d;%4ldH\033[?25h\033[%c q",
			FOCUSPOINT, CURBUF(editor).x + 1,
			CURBUF(editor).mode == ModeEdit ? '5' : '1');

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
		if ((unsigned)(y + CURBUF(editor).y - FOCUSPOINT) < CURBUF(editor).rows.len) {
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
		abPrintf(ab, cp, 256, "%c:%c %s L%ld of %ld C%ld of %ld (%s",
				CURBUF(editor).anonymous ? 'U' : '-',
				CURBUF(editor).dirty ? '*' : '-',
				CURBUF(editor).anonymous ?
					"*anonymous*" : CURBUF(editor).name,
				CURBUF(editor).y + 1,
				CURBUF(editor).rows.len,
				CURBUF(editor).x + 1,
				CURBUF(editor).rows.data[CURBUF(editor).y].len,
				lang_modes[CURBUF(editor).mode]
		);
		for (i = 0; i < (signed)CURBUF(editor).submodeslen; ++i) {
			abPrintf(ab, cp, 256, "/%s",
					lang_modes[CURBUF(editor).submodes[i]]
			);
		}
		abPrintf(ab, cp, 256, ") %ld buffer(s)",
				editor.bufs.len - 1
		);
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
	Binding *binds;
	size_t i;
	Arg arg;
	binds = &bindings[CURBUF(editor).submodeslen ?
		CURBUF(editor).submodes[CURBUF(editor).submodeslen - 1] : CURBUF(editor).mode];
	for (i = 0; i < binds->len; ++i)
		if (key == (binds->keys[i].key
						& binds->keys[i].mod)
		|| i == binds->len - 1) {
			(binds->keys[i].func)
				(
					binds->keys[i].arg.v == REPLACE ?
						arg.c = (char)(key), &arg :
						&(binds->keys[i].arg)
				);
			return;
		}
}

static void
edit(void)
{
	while (editor.bufs.len - 1) {
		termRefresh();
		editorParseKey(editorGetKey());
	}
}

static void
switchmode(Mode mode)
{
	CURBUF(editor).mode = mode;
}

/* buffers */
static void
newBuffer(Buffer *buf)
{
	Buffer init;
	String ln;

	if (buf == NULL)
		buf = &init;

	newVector(buf->rows);
	ln.data = malloc(ln.len = 0);
	pushVector(buf->rows, ln);
	*(buf->path) = *(buf->name) = '\0';
	buf->anonymous = 1;
	buf->dirty = 0;
	buf->x = buf->y = 0;
	buf->mode = ModeNormal;
	buf->submodeslen = 0;

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
	buf->mode = ModeNormal;
	buf->submodeslen = 0;

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
freeBuffer(Buffer *buf)
{
	size_t i;
	for (i = 0; i < buf->rows.len; ++i)
		free(buf->rows.data[i].data);
}

static int
writeBuffer(Buffer *buf, char *filename)
{
	int fd;
	size_t i;
	if (filename == NULL) {
		if (buf->anonymous)
			return minibufferError(lang_err[3]);
		else
			filename = buf->path;
	}
	if ((fd = open(filename, O_WRONLY | O_CREAT)) < 0)
		die("open:");
	for (i = 0; i < buf->rows.len; ++i)
		write(fd, buf->rows.data[i].data, buf->rows.data[i].len);
	close(fd);
	CURBUF(editor).dirty = 0;
	return 0;
}

static int
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
	return 0;
}

static int
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
	return 1;
}

static inline int
submodePush(Buffer *b, Mode m)
{
	if (b->submodeslen >= SUBMODES_MAX)
		return 1;
	b->submodes[(b->submodeslen)++] = m;
	return 0;
}

static inline int
submodePop(Buffer *b)
{
	if (!b->submodeslen)
		return 1;
	--(b->submodeslen);
	return 0;
}

/* other */
static void
setup(char *filename)
{
	Buffer b;
	rawOn();
	getws(&(terminal.r), &(terminal.c));
	newVector(editor.bufs);
	/* pushing fallback buffer used when no buffers left */
	memset(&b, 0, sizeof b);
	pushVector(editor.bufs, b);
	if (filename == NULL)
		newBuffer(NULL);
	else
		editBuffer(NULL, filename);
	editor.curbuf = 1;
	minibufferPrint(lang_base[2]);
}

static void
finish(void)
{
	rawRestore();
	write(STDOUT_FILENO, "\033[2J\033[H", 7);
	exit(0);
}

static void
usage(void)
{
	die("%s: %s [-hLv] [FILE]", lang_err[0], argv0);
}

/* editor functions */
static void
echo(const Arg *arg)
{
	minibufferPrint(arg->v);
}

static void
echoe(const Arg *arg)
{
	minibufferError(arg->v);
}

static void
normalmode(const Arg *arg)
{
	(void)arg;
	switchmode(ModeNormal);
}

static void
insertmode(const Arg *arg)
{
	if (arg->i)
		beginning(NULL);
	switchmode(ModeEdit);
}

static void
appendmode(const Arg *arg)
{
	Arg a;
	a.i = 3;
	cursormove(&a);
	if (arg->i)
		ending(NULL);
	switchmode(ModeEdit);
}

static void
globalsubmode(const Arg *arg)
{
	(void)arg;
	submodePush(&CURBUF(editor), SubModeGlobal);
	submodePush(&CURBUF(editor), SubModeMovement);
	editorParseKey(editorGetKey());
	submodePop(&CURBUF(editor)); /* SubModeMovement */
	submodePop(&CURBUF(editor)); /* SubModeGlobal */
}

static void
buffersubmode(const Arg *arg)
{
	(void)arg;
	switchmode(SubModeBuffer);
	editorParseKey(editorGetKey());
	switchmode(ModeNormal);
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
		if (CURBUF(editor).y < (signed)CURBUF(editor).rows.len - 1) {
			if (CURBUF(editor).x >= (unsigned)CURBUF(editor).rows.data[++(CURBUF(editor).y)].len)
				CURBUF(editor).x = (unsigned)CURBUF(editor).rows.data[CURBUF(editor).y].len;
		} else
			minibufferPrint("Already on bottom");
		break;
	case 2: /* up */
		if (CURBUF(editor).y > 0) {
			if (CURBUF(editor).x >= (unsigned)CURBUF(editor).rows.data[--(CURBUF(editor).y)].len)
				CURBUF(editor).x = (unsigned)CURBUF(editor).rows.data[CURBUF(editor).y].len;
		} else
			minibufferPrint("Already on top");
		break;
	case 3: /* right */
		if (CURBUF(editor).x < (signed)CURBUF(editor).rows.data[CURBUF(editor).y].len - 1)
			++(CURBUF(editor).x);
		else
			minibufferPrint("Already on end of line");
		break;
	}
}

static void
beginning(const Arg *arg)
{
	(void)arg;
	CURBUF(editor).x = 0;
}

static void
ending(const Arg *arg)
{
	(void)arg;
	CURBUF(editor).x = (signed)CURBUF(editor).rows.data[CURBUF(editor).y].len - 1;
}

static void
insertchar(const Arg *arg)
{
	CURBUF(editor).rows.data[CURBUF(editor).y].data =
		realloc(CURBUF(editor).rows.data[CURBUF(editor).y].data,
				++CURBUF(editor).rows.data[CURBUF(editor).y].len + 1);
	memmove(CURBUF(editor).rows.data[CURBUF(editor).y].data + CURBUF(editor).x + 1,
			CURBUF(editor).rows.data[CURBUF(editor).y].data + CURBUF(editor).x,
			CURBUF(editor).rows.data[CURBUF(editor).y].len - (unsigned)CURBUF(editor).x);
	CURBUF(editor).dirty = 1;

	CURBUF(editor).rows.data[CURBUF(editor).y].data[CURBUF(editor).x++] = arg->c;
}

static void
removechar(const Arg *arg)
{
	(void)arg;
	if (CURBUF(editor).x <= 0) return;
	memmove(CURBUF(editor).rows.data[CURBUF(editor).y].data + CURBUF(editor).x - 1,
			CURBUF(editor).rows.data[CURBUF(editor).y].data + CURBUF(editor).x,
			CURBUF(editor).rows.data[CURBUF(editor).y].len - (unsigned)CURBUF(editor).x);
	--(CURBUF(editor).rows.data[CURBUF(editor).y].len);
	--CURBUF(editor).x;
}

static void
openline(const Arg *arg)
{
	CURBUF(editor).rows.data = realloc(CURBUF(editor).rows.data,
				++(CURBUF(editor).rows.len) * sizeof *(CURBUF(editor).rows.data));
	if (arg->i != 1) ++CURBUF(editor).y;
	memmove(CURBUF(editor).rows.data + CURBUF(editor).y,
			CURBUF(editor).rows.data + CURBUF(editor).y - 1,
			(CURBUF(editor).rows.len - (unsigned)(CURBUF(editor).y)) *
				sizeof *(CURBUF(editor).rows.data));
	CURBUF(editor).rows.data[CURBUF(editor).y].data =
		malloc(CURBUF(editor).rows.data[CURBUF(editor).y].len =
				arg->i == 2 ? CURBUF(editor).rows.data[CURBUF(editor).y - 1].len -
					(unsigned)CURBUF(editor).x : 0);
	if (arg->i == 2) {
		memmove(CURBUF(editor).rows.data[CURBUF(editor).y].data,
				CURBUF(editor).rows.data[CURBUF(editor).y - 1].data + CURBUF(editor).x,
				CURBUF(editor).rows.data[CURBUF(editor).y - 1].len - (unsigned)CURBUF(editor).x);
		CURBUF(editor).rows.data[CURBUF(editor).y - 1].len = (unsigned)CURBUF(editor).x;
	}
	CURBUF(editor).x = 0;
	switchmode(ModeEdit);
}

static void
bufwriteclose(const Arg *arg)
{
	(void)arg;
	if (CURBUF(editor).dirty)
		writeBuffer(&CURBUF(editor), NULL);
	freeBuffer(editor.bufs.data + editor.curbuf);
	memmove(editor.bufs.data + editor.curbuf,
			editor.bufs.data + editor.curbuf + 1,
			editor.bufs.len-- - (size_t)(editor.curbuf + 1));
}

static void
bufwrite(const Arg *arg)
{
	(void)arg;
	writeBuffer(&CURBUF(editor), NULL);
}

static void
bufclose(const Arg *arg)
{
	(void)arg;
	if (CURBUF(editor).dirty) {
		minibufferError(lang_err[2]);
		return;
	}
	freeBuffer(editor.bufs.data + editor.curbuf);
	memmove(editor.bufs.data + editor.curbuf,
			editor.bufs.data + editor.curbuf + 1,
			editor.bufs.len-- - (size_t)(editor.curbuf + 1));
}

static void
bufkill(const Arg *arg)
{
	(void)arg;
	freeBuffer(editor.bufs.data + editor.curbuf);
	memmove(editor.bufs.data + editor.curbuf,
			editor.bufs.data + editor.curbuf + 1,
			editor.bufs.len-- - (size_t)(editor.curbuf + 1));
}

int
main(int argc, char *argv[])
{
	char *filename;
	ARGBEGIN {
	case 'h': default: /* fallthrough */
		usage();
		break;
	case 'L':
		die("%s, %s", lang_base[0], lang_base[1]);
		break;
	case 'v':
		die("be-" VERSION);
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
