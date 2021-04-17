#include <termios.h>
#include <unistd.h>

static void rawRestore(void);
static void rawOn(void);

static struct termios origtermios;

static void
rawOn(void)
{
	struct termios raw;

	tcgetattr(STDIN_FILENO, &origtermios);
	raw = origtermios;
	raw.c_cflag |= (CS8);
	raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_oflag &= (tcflag_t)~(OPOST);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void
rawRestore(void)
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &origtermios);
}

int
main(int argc, char *argv[])
{
	rawOn();
	/* code */
	rawRestore();
	return 0;
}
