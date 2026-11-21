#pragma once

/* keyboard_3ds.h -- the platform side of the system keyboard (keyboard_3ds.cpp).
 * The menu-facing calls (platform_text_input_begin / _result) are declared in
 * util/platform.h. */

/* Open the system keyboard if a menu asked for it. Called at the start of
 * the input poll, outside any citro3d frame; blocks while it is shown.
 * Returns true if it was shown. */
bool keyboard_3ds_service( void );
