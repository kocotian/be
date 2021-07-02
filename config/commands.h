/* See COPYRIGHT file for copyright and license details */

static Command
commands[] = {
	/* command              alias       function        argument */
	{ "writeclose",         "z",        bufwriteclose,  {0} },
	{ "write",              "w",        bufwrite,       {0} },
	{ "close",              "c",        bufclose,       {0} },
	{ "quit",               "q",        bufkill,        {0} },
	{ "shell",              "sh",       shell,          {0} },
};
