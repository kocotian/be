/* See COPYRIGHT file for copyright and license details */

static Key
/* general modes */
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
	{ ModNone,      '0',    beginning,      {0} },
	{ ModNone,      '$',    ending,         {0} },

	{ ModNone,      'o',    openline,       {0} },
	{ ModNone,      'O',    openline,       {1} },
	{ ModNone,      '\r',   openline,       {2} },

	{ ModNone,      'd',    deleteline,     {0} },
	{ ModShift,     'd',    deleteline,     {.i = -1} },
	{ ModNone,      'c',    changeline,     {0} },
	{ ModShift,     'c',    changeline,     {.i = -1} },

	{ ModNone,      'm',    togglemark,     {0} },

	/* Submodes */
	{ ModNone,      'g',    globalsubmode,  {0} },

	/* other */
	{ ModShift,     'z',    buffermode,     {0} },
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
},

editbindings[] = {
	/* modifier     key     function        argument */
	{ ModNone,      033,    normalmode,     {0} },
	{ ModNone,      '\r',   normalmode,     {1} },
	{ ModNone,      127,    removechar,     {0} },
	{ ModNone,      0,      insertchar,     {.v = REPLACE} },
},

bufferbindings[] = {
	/* modifier     key     function        argument */
	{ ModShift,     'z',    bufwriteclose,  {0} },
	{ ModShift,     'w',    bufwrite,       {0} },
	{ ModShift,     'c',    bufclose,       {0} },
	{ ModShift,     'q',    bufkill,        {0} },
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
},

/* submodes */
s_globalbindings[] = {
	/* modifier     key     function        argument */
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
},

s_movementbindings[] = {
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
};

#define BIND(KEYS) { (KEYS), LEN((KEYS)) }

static Binding bindings[] = {
	/* mode                    keys */
	[ModeEdit]          = BIND(editbindings),
	[ModeNormal]        = BIND(normalbindings),
	[ModeBuffer]        = BIND(bufferbindings),
	[SubModeGlobal]     = BIND(s_globalbindings),
	[SubModeMovement]   = BIND(s_movementbindings),
};
