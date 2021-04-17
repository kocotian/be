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
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <util.h>

/* macros */
#define CTRL_KEY(KEY) ((KEY) & 0x1f)

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

/* prototypes */
static void rawRestore(void);
static void rawOn(void);
/*********/
static unsigned char editorGetKey(int fd);
static void edit(void);
static void editorParseKey(unsigned char key);
/*********/
static void finish(void);
/*********/
static void quit(Arg *arg);

/* global variables */
#include "config.h"
static struct termios origtermios;

/* terminal */
static void
rawOn(void)
{
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &origtermios) < 0)
		die("tcgetattr:");
	raw = origtermios;
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
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &origtermios) < 0)
		die("tcsetattr:");
}

/* editor */
static unsigned char
editorGetKey(int fd)
{
	ssize_t rb;
	unsigned char c;

	while ((rb = read(fd, &c, 1)) != 1)
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
		editorParseKey(editorGetKey(STDIN_FILENO));
	}
}

/* other */
static void
finish(void)
{
	rawRestore();
	exit(0);
}

/* editor functions */
static void
quit(Arg *arg)
{
	(void)arg;
	finish();
}

int
main(int argc, char *argv[])
{
	rawOn();
	edit();
	finish();

	return 0;
}
