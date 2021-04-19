static char
*lang_base[] = {
	[0] = "en_US",
	[1] = "English (US)",
	[2] = "welcome to be, version " VERSION,
},
*lang_modes[] = {
	[ModeNormal]        = "Normal",
	[ModeEdit]          = "Edit",
	[ModeBuffer]        = "Buffer",
	[SubModeGlobal]     = "Global",
	[SubModeMovement]   = "Movement",
},
*lang_err[] = {
	[0] = "usage",
	[1] = "screen is too small (min. %dx%d)",
	[2] = "buffer have unsaved changes",
	[3] = "cannot write anonymous buffer without filename",
};
