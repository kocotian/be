#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include <util.h>

/* macros */
#define CTRL_KEY(KEY) ((KEY) & 0x1f)

/* prototypes */
static void rawRestore(void);
static void rawOn(void);
/*********/
static unsigned char editorGetKey(int fd);
static void edit(void);
static void editorParseKey(unsigned char key);
/*********/
static void finish(void);

/* global variables */
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
	switch (key) {
	case 'Q':
		finish(); break;
	}
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

int
main(int argc, char *argv[])
{
	rawOn();
	edit();
	finish();

	return 0;
}
