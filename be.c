#include <termios.h>
#include <unistd.h>

#include <util.h>

static void rawRestore(void);
static void rawOn(void);

static struct termios origtermios;

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

int
main(int argc, char *argv[])
{
	rawOn();
	/* code */
	rawRestore();
	return 0;
}
