#ifndef _LANG_H
#define _LANG_H

typedef enum {
	LangCode = 0, LangName, LangWelcome,
} Lang;

typedef enum {
	InfoAlreadyBeg, InfoAlreadyBot, InfoAlreadyTop, InfoAlreadyEnd,
} Info;

typedef enum {
	ErrUsage = 0, ErrScreenTooSmall,
	ErrDirty, ErrWriteAnon,
} Errno;

#endif
