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
},
*lang_info[] = {
	[InfoAlreadyBeg]    = "Already on beginning of line",
	[InfoAlreadyBot]    = "Already on bottom",
	[InfoAlreadyTop]    = "Already on top",
	[InfoAlreadyEnd]    = "Already on end of line",
},
*lang_err[] = {
	[ErrUsage]          = "usage",
	[ErrScreenTooSmall] = "screen is too small (min. %dx%d)",
	[ErrDirty]          = "buffer have unsaved changes",
	[ErrWriteAnon]      = "cannot write anonymous buffer without filename",
};
