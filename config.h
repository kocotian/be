/* See COPYRIGHT file for copyright and license details */

/* focus point is focused place of terminal, for example
   if you set this to 1, focus will be on top,
   if you set this to (terminal.r - 2), focus will be on bottom */
#define FOCUSPOINT ((terminal.r - 1) / 2)

/* language */
#include <lang/en_US.h>

/* key bindings */
static Key
normalbindings[] = {
	/* modifier     key     function        argument */
	/* movement */
	{ ModNone,      'h',    cursormove,     {.i = 0} },
	{ ModNone,      'j',    cursormove,     {.i = 1} },
	{ ModNone,      'k',    cursormove,     {.i = 2} },
	{ ModNone,      'l',    cursormove,     {.i = 3} },

	/* edit mode */
	{ ModNone,      'i',    insertmode,     {.i = 0} },
	{ ModShift,     'i',    insertmode,     {.i = 1} },
	{ ModNone,      'a',    appendmode,     {.i = 0} },
	{ ModShift,     'a',    appendmode,     {.i = 1} },

	/* advanced movement */
	{ ModNone,      '^',    beginning,      {0} },
	{ ModNone,      '0',    beginning,      {0} },
	{ ModNone,      '$',    ending,         {0} },
	{ ModNone,      'E',    ending,         {0} },

	{ ModNone,      'o',    openline,       {0} },
	{ ModNone,      'O',    openline,       {1} },
	{ ModNone,      '\r',   openline,       {2} },

	/* other */
	{ ModShift,     'q',    quit,           {0} },
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
},

editbindings[] = {
	/* modifier     key     function        argument */
	{ ModNone,      033,    normalmode,     {0} },
	{ ModNone,      '\r',   normalmode,     {1} },
	{ ModNone,      127,    removechar,     {0} },
	{ ModNone,      0,      insertchar,     {.v = REPLACE} },
};

#define BIND(KEYS) { (KEYS), LEN((KEYS)) }

static Binding bindings[] = {
	/* mode                    keys */
	[ModeEdit]          = BIND(editbindings),
	[ModeNormal]        = BIND(normalbindings),
};
