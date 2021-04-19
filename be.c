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
#include <lang.h>
#include <str.h>
#include <util.h>

#define CURBUF (be.buffers.data[be.windows.data[be.focusedwin].buffer])
#define CURBUFINDEX (be.windows.data[be.focusedwin].buffer)
#define CURWIN (be.windows.data[be.focusedwin])
#define CURWININDEX (be.focusedwin)
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
	ModeBuffer,
	SubModeGlobal,
	SubModeMovement,
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

typedef struct Line {
	char *data;
	size_t len;
} Line;

typedef struct Buffer {
	Array(Line) lines;
	char path[PATH_MAX], name[NAME_MAX];
	int anonymous, dirty;
	ssize_t x, y;
	Mode mode;
	Mode submodes[SUBMODES_MAX];
	size_t submodeslen;
} Buffer;

typedef struct Window {
	int buffer;
	int r, c, x, y;
} Window;

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
static inline void edit(void);
static inline void switchmode(Mode mode);
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
static void buffermode(const Arg *arg);
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
	Array(Buffer) buffers;
	Array(Window) windows;
	int focusedwin;
	int r, c;
} be;

char *argv0;

/* config */
#include "config.h"

/* terminal */
static void
rawOn(void)
{
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &(be.origtermios)) < 0)
		die("tcgetattr:");
	raw = (be.origtermios);
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
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &(be.origtermios)) < 0)
		die("tcsetattr:");
}

static void
getws(int *r, int *c)
{
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
		die("ioctl:");
	if (ws.ws_col < 20 || ws.ws_row < 3)
		die(lang_err[ErrScreenTooSmall], 20, 3);
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
			FOCUSPOINT, CURBUF.x + 1,
			CURBUF.mode == ModeEdit ? '5' : '1');

	if ((unsigned)write(STDOUT_FILENO, ab.data, ab.len) != ab.len)
		die("write:");

	abFree(&ab);
}

static void
appendRows(String *ab)
{
	size_t y;
	for (y = 0; y < (unsigned)(CURWIN.r - 1); ++y)
		abAppend(ab, "\033[K~\r\n", (y < (unsigned)(CURWIN.r - 1)) ? 6 : 4);
}

static void
appendContents(String *ab)
{
	ssize_t y;
	char cp[19];
	for (y = 0; y < CURWIN.r; ++y) {
		if ((unsigned)(y + CURBUF.y - FOCUSPOINT) < CURBUF.lines.len) {
			abPrintf(ab, cp, 19, "\033[%4ld;0H\033[K",
					y);
			abAppend(ab,
					CURBUF.lines.data[y + CURBUF.y - FOCUSPOINT].data,
					CURBUF.lines.data[y + CURBUF.y - FOCUSPOINT].len);
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
			(unsigned)(CURWIN.r));
	while (++i < CURWIN.c) {
		abAppend(ab, " ", 1);
	}
	abAppend(ab, "\r", 1);
	/* status drawing */
	{
		abPrintf(ab, cp, 256, "%c:%c %s L%ld of %ld C%ld of %ld (%s",
				CURBUF.anonymous ? 'U' : '-',
				CURBUF.dirty ? '*' : '-',
				CURBUF.anonymous ?
					"*anonymous*" : CURBUF.name,
				CURBUF.y + 1,
				CURBUF.lines.len,
				CURBUF.x + 1,
				CURBUF.lines.data[CURBUF.y].len,
				lang_modes[CURBUF.mode]
		);
		for (i = 0; i < (signed)CURBUF.submodeslen; ++i) {
			abPrintf(ab, cp, 256, "/%s",
					lang_modes[CURBUF.submodes[i]]
			);
		}
		abPrintf(ab, cp, 256, ") %ld buffer(s)",
				be.buffers.len - 1
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

	termRefresh();
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
	binds = &bindings[CURBUF.submodeslen ?
		CURBUF.submodes[CURBUF.submodeslen - 1] : CURBUF.mode];
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

static inline void
edit(void)
{
	while (be.buffers.len - 1) editorParseKey(editorGetKey());
}

static inline void
switchmode(Mode mode)
{
	CURBUF.mode = mode;
}

/* buffers */
static void
newBuffer(Buffer *buf)
{
	Buffer init;
	Line ln;

	if (buf == NULL)
		buf = &init;

	newVector(buf->lines);
	ln.data = malloc(ln.len = 0);
	pushVector(buf->lines, ln);
	*(buf->path) = *(buf->name) = '\0';
	buf->anonymous = 1;
	buf->dirty = 0;
	buf->x = buf->y = 0;
	buf->mode = ModeNormal;
	buf->submodeslen = 0;

	if (buf == &init)
		pushVector(be.buffers, init);
}

static void
editBuffer(Buffer *buf, char *filename)
{
	Buffer init;
	int fd;
	struct stat sb;
	char *data;
	String fstr, fline;
	Line fpush;

	if (buf == NULL)
		buf = &init;

	newVector(buf->lines);
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
		pushVector(buf->lines, fpush);
		fstr.data += fline.len;
		fstr.len -= fline.len;
	}

	free(data);

	if (buf == &init)
		pushVector(be.buffers, init);
}

static void
freeBuffer(Buffer *buf)
{
	size_t i;
	for (i = 0; i < buf->lines.len; ++i)
		free(buf->lines.data[i].data);
}

static int
writeBuffer(Buffer *buf, char *filename)
{
	int fd;
	size_t i;
	if (filename == NULL) {
		if (buf->anonymous)
			return minibufferError(lang_err[ErrWriteAnon]);
		else
			filename = buf->path;
	}
	if ((fd = open(filename, O_WRONLY | O_CREAT)) < 0)
		die("open:");
	for (i = 0; i < buf->lines.len; ++i)
		write(fd, buf->lines.data[i].data, buf->lines.data[i].len);
	close(fd);
	CURBUF.dirty = 0;
	return 0;
}

static int
minibufferPrint(const char *s)
{
	String ab = { NULL, 0 };
	char cp[20];

	abPrintf(&ab, cp, 20, "\033[%4d;%4dH\033[0m\033[K",
			be.r, 1);

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
			be.r, 1);

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
	Window w;
	rawOn();
	getws(&(be.r), &(be.c));

	newVector(be.buffers);
	/* pushing fallback buffer used when no buffers left */
	memset(&b, 0, sizeof b);
	pushVector(be.buffers, b);

	newVector(be.windows);
	w.r = be.r - 1; w.c = be.c;
	w.x = w.y = 0;
	w.buffer = 1;
	pushVector(be.windows, w);
	be.focusedwin = 0;

	if (filename == NULL)
		newBuffer(NULL);
	else
		editBuffer(NULL, filename);
	minibufferPrint(lang_base[ErrDirty]);
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
	die("%s: %s [-hLv] [FILE]", lang_err[ErrUsage], argv0);
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
	++CURBUF.x;
	if (arg->i)
		ending(NULL);
	if (CURBUF.x > (signed)CURBUF.lines.data[CURBUF.y].len)
		CURBUF.x = (signed)CURBUF.lines.data[CURBUF.y].len;
	switchmode(ModeEdit);
}

static void
globalsubmode(const Arg *arg)
{
	(void)arg;
	submodePush(&CURBUF, SubModeGlobal);
	submodePush(&CURBUF, SubModeMovement);
	editorParseKey(editorGetKey());
	submodePop(&CURBUF); /* SubModeMovement */
	submodePop(&CURBUF); /* SubModeGlobal */
}

static void
buffermode(const Arg *arg)
{
	(void)arg;
	switchmode(ModeBuffer);
	editorParseKey(editorGetKey());
	switchmode(ModeNormal);
}

static void
cursormove(const Arg *arg)
{
	switch (arg->i) {
	case 0: /* left */
		if (CURBUF.x > 0) --(CURBUF.x);
		else minibufferPrint(lang_info[InfoAlreadyBeg]); break;
	case 1: /* down */
		if (CURBUF.y < (signed)CURBUF.lines.len - 1) ++(CURBUF.y);
		else minibufferPrint(lang_info[InfoAlreadyBot]); break;
	case 2: /* up */
		if (CURBUF.y > 0) --(CURBUF.y);
		else minibufferPrint(lang_info[InfoAlreadyTop]); break;
	case 3: /* right */
		if (CURBUF.x < (signed)CURBUF.lines.data[CURBUF.y].len) ++(CURBUF.x);
		else minibufferPrint(lang_info[InfoAlreadyEnd]); break;
	}
	if (CURBUF.x >= (signed)CURBUF.lines.data[CURBUF.y].len)
		CURBUF.x = (signed)CURBUF.lines.data[CURBUF.y].len;
}

static void
beginning(const Arg *arg)
{
	(void)arg;
	CURBUF.x = 0;
}

static void
ending(const Arg *arg)
{
	(void)arg;
	CURBUF.x = MAX(0, (signed)CURBUF.lines.data[CURBUF.y].len);
}

static void
insertchar(const Arg *arg)
{
	CURBUF.lines.data[CURBUF.y].data =
		realloc(CURBUF.lines.data[CURBUF.y].data,
				++CURBUF.lines.data[CURBUF.y].len + 1);
	memmove(CURBUF.lines.data[CURBUF.y].data + CURBUF.x + 1,
			CURBUF.lines.data[CURBUF.y].data + CURBUF.x,
			CURBUF.lines.data[CURBUF.y].len - (unsigned)CURBUF.x);
	CURBUF.dirty = 1;

	CURBUF.lines.data[CURBUF.y].data[CURBUF.x++] = arg->c;
}

static void
removechar(const Arg *arg)
{
	(void)arg;
	if (CURBUF.x <= 0) return;
	memmove(CURBUF.lines.data[CURBUF.y].data + CURBUF.x - 1,
			CURBUF.lines.data[CURBUF.y].data + CURBUF.x,
			CURBUF.lines.data[CURBUF.y].len - (unsigned)CURBUF.x);
	--(CURBUF.lines.data[CURBUF.y].len);
	--CURBUF.x;
}

static void
openline(const Arg *arg)
{
	CURBUF.lines.data = realloc(CURBUF.lines.data,
				++(CURBUF.lines.len) * sizeof *(CURBUF.lines.data));
	if (arg->i != 1) ++CURBUF.y;
	memmove(CURBUF.lines.data + CURBUF.y,
			CURBUF.lines.data + CURBUF.y - 1,
			(CURBUF.lines.len - (unsigned)(CURBUF.y)) *
				sizeof *(CURBUF.lines.data));
	CURBUF.lines.data[CURBUF.y].data =
		malloc(CURBUF.lines.data[CURBUF.y].len =
				arg->i == 2 ? CURBUF.lines.data[CURBUF.y - 1].len -
					(unsigned)CURBUF.x : 0);
	if (arg->i == 2) {
		memmove(CURBUF.lines.data[CURBUF.y].data,
				CURBUF.lines.data[CURBUF.y - 1].data + CURBUF.x,
				CURBUF.lines.data[CURBUF.y - 1].len - (unsigned)CURBUF.x);
		CURBUF.lines.data[CURBUF.y - 1].len = (unsigned)CURBUF.x;
	}
	CURBUF.x = 0;
	switchmode(ModeEdit);
}

static void
bufwriteclose(const Arg *arg)
{
	(void)arg;
	if (CURBUF.dirty)
		writeBuffer(&CURBUF, NULL);
	freeBuffer(be.buffers.data + CURBUFINDEX);
	memmove(be.buffers.data + CURBUFINDEX,
			be.buffers.data + CURBUFINDEX + 1,
			be.buffers.len-- - (size_t)(CURBUFINDEX + 1));
}

static void
bufwrite(const Arg *arg)
{
	(void)arg;
	writeBuffer(&CURBUF, NULL);
}

static void
bufclose(const Arg *arg)
{
	(void)arg;
	if (CURBUF.dirty) {
		minibufferError(lang_err[ErrDirty]);
		return;
	}
	freeBuffer(be.buffers.data + CURBUFINDEX);
	memmove(be.buffers.data + CURBUFINDEX,
			be.buffers.data + CURBUFINDEX + 1,
			be.buffers.len-- - (size_t)(CURBUFINDEX + 1));
}

static void
bufkill(const Arg *arg)
{
	(void)arg;
	freeBuffer(be.buffers.data + CURBUFINDEX);
	memmove(be.buffers.data + CURBUFINDEX,
			be.buffers.data + CURBUFINDEX + 1,
			be.buffers.len-- - (size_t)(CURBUFINDEX + 1));
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
		die("%s, %s", lang_base[LangCode], lang_base[LangName]);
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
