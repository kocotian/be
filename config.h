/* See COPYRIGHT file for copyright and license details */

#define FOCUSPOINT ((terminal.r - 1) / 2)

static Key bindings[] = {
	/* modifier     key     function        argument */
	{ ModNone,      'h',    cursormove,     {.i = 0} },
	{ ModNone,      'j',    cursormove,     {.i = 1} },
	{ ModNone,      'k',    cursormove,     {.i = 2} },
	{ ModNone,      'l',    cursormove,     {.i = 3} },
	{ ModNone,      '^',    beginning,      {0} },
	{ ModNone,      '0',    beginning,      {0} },
	{ ModNone,      '$',    ending,         {0} },
	{ ModNone,      'E',    ending,         {0} },
	{ ModShift,     'q',    quit,           {0} },
};
