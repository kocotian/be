static char
*lang_base[] = {
	[LangCode]          = "en_US",
	[LangName]          = "English (US)",
	[LangWelcome]       = "welcome to be, version " VERSION,
},
*lang_modes[] = {
	[ModeNormal]        = "Normal",
	[ModeEdit]          = "Edit",
	[ModeBuffer]        = "Buffer",
	[SubModeGlobal]     = "Global",
	[SubModeMovement]   = "Movement",
},
*lang_err[] = {
	[ErrUsage]          = "usage",
	[ErrScreenTooSmall] = "screen is too small (min. %dx%d)",
	[ErrDirty]          = "buffer have unsaved changes",
	[ErrWriteAnon]      = "cannot write anonymous buffer without filename",
};
