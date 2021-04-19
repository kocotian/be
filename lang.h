#ifndef _LANG_H
#define _LANG_H

typedef enum {
	LangCode = 0, LangName, LangWelcome,
} Lang;

typedef enum {
	ErrUsage = 0, ErrScreenTooSmall,
	ErrDirty, ErrWriteAnon,
} Errno;

#endif
