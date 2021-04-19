/* See COPYRIGHT file for copyright and license details */

/* focus point is focused place of buffer, for example
   if you set this to 1, focus will be on top,
   if you set this to be.windows.data[be.focusedwin].r - 2),
   focus will be on bottom */
#define FOCUSPOINT ((be.windows.data[be.focusedwin].r - 1) / 2)

/* language */
#include <lang/en_US.h>

/* key bindings */
#include <config/bindings.h>
