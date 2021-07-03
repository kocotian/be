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

	{ ModNone,      '0',    beginning,      {0} },
	{ ModNone,      '$',    ending,         {0} },
	{ ModShift,     'g',    ending,         {1} }, /* alternate g$ for vim powerusers */

	/* edit mode */
	{ ModNone,      'i',    insertmode,     {.i = 0} },
	{ ModShift,     'i',    insertmode,     {.i = 1} },
	{ ModNone,      'a',    appendmode,     {.i = 0} },
	{ ModShift,     'a',    appendmode,     {.i = 1} },

	{ ModNone,      'o',    openline,       {0} },
	{ ModNone,      'O',    openline,       {1} },
	{ ModNone,      '\r',   openline,       {2} },

	{ ModNone,      'd',    deleteline,     {0} },
	{ ModShift,     'd',    deleteline,     {.i = -1} },
	{ ModNone,      'c',    changeline,     {0} },
	{ ModShift,     'c',    changeline,     {.i = -1} },

	{ ModNone,      'm',    togglemark,     {0} },

	/* other modes */
	{ ModNone,      'g',    globalsubmode,  {0} },
	{ ModNone,      ':',    commandmode,    {0} },

	/* other */
	{ ModShift,     'z',    buffermode,     {0} },
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
},

editbindings[] = {
	/* modifier     key     function        argument */
	{ ModNone,      033,    normalmode,     {0} },
	{ ModNone,      '\r',   normalmode,     {1} },
	{ ModNone,      127,    removechar,     {0} },
	{ ModNone,      0,      insertchar,     {0} },
},

bufferbindings[] = {
	/* modifier     key     function        argument */
	{ ModShift,     'z',    bufwriteclose,  {0} },
	{ ModShift,     'w',    bufwrite,       {0} },
	{ ModShift,     'c',    bufclose,       {0} },
	{ ModShift,     'q',    bufkill,        {0} },
	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
},

commandbindings[] = {
	/* modifier     key     function        argument */
	{ ModNone,      033,    normalmode,     {1} },
	{ ModNone,      '\r',   execcmd,        {0} },
	{ ModNone,      127,    cmdremovechar,  {0} },
	{ ModNone,      0,      cmdinsertchar,  {.v = REPLACE} },
},

/* submodes */
s_globalbindings[] = {
	/* modifier     key     function        argument */
	{ ModNone,      '0',    beginning,      {1} },
	{ ModNone,      '$',    ending,         {1} },

	{ ModNone,      'g',    beginning,      {1} }, /* alternate g0 for vim powerusers */

	{ ModNone,      0,      echoe,          {.v = "Key is not bound"} },
};

#define BIND(KEYS) { (KEYS), LEN((KEYS)) }

static Binding bindings[] = {
	/* mode                    keys */
	[ModeEdit]          = BIND(editbindings),
	[ModeNormal]        = BIND(normalbindings),
	[ModeBuffer]        = BIND(bufferbindings),
	[ModeCommand]       = BIND(commandbindings),

	[SubModeGlobal]     = BIND(s_globalbindings),
};
