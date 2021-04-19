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

	/* other */
	{ ModShift,     'z',    buffersubmode,  {0} },
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
},

editbindings[] = {
	/* modifier     key     function        argument */
	{ ModNone,      033,    normalmode,     {0} },
	{ ModNone,      '\r',   normalmode,     {1} },
	{ ModNone,      127,    removechar,     {0} },
	{ ModNone,      0,      insertchar,     {.v = REPLACE} },
},

/* submodes */
s_bufferbindings[] = {
	/* modifier     key     function        argument */
	{ ModShift,     'z',    bufwriteclose,  {0} },
	{ ModShift,     'w',    bufwrite,       {0} },
	{ ModShift,     'c',    bufclose,       {0} },
	{ ModShift,     'q',    bufkill,        {0} },
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
};

#define BIND(KEYS) { (KEYS), LEN((KEYS)) }

static Binding bindings[] = {
	/* mode                    keys */
	[ModeEdit]          = BIND(editbindings),
	[ModeNormal]        = BIND(normalbindings),
	[SubModeBuffer]     = BIND(s_bufferbindings),
};
