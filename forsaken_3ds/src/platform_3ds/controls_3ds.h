#pragma once

/* controls_3ds.h -- 3DS button bindings, their save file and the remap overlay
 *
 * controls_3ds.cpp owns one process-lifetime controls_3ds_t: platform
 * defaults (OG hardware lacks ZL/ZR, so nitro moves to SELECT) overlaid
 * by sdmc:/3ds/forsaken/controls.cfg, a hand-editable action=BUTTON file.
 * Each controls_action_t value is also the engine joystick-button slot
 * that input_3ds.cpp writes and config.cpp's DefaultJoystickSettings
 * binds; the two must stay in lockstep. The remap menu is not modal: the
 * pause menu opens it, oct2.cpp draws it each frame on the bottom screen,
 * and controls_3ds_handle_input swallows HID input while it is open.
 * The analog sticks have one option, Swap Sticks (circle pad <-> C-stick
 * roles), edited in the same overlay; the stick->axis routing itself is
 * fixed (C-stick = movement, circle pad = look) in input_3ds.cpp.
 */

#include "util/platform.h"


#ifdef __3DS__

#include <stdbool.h>
#include <3ds.h>

/* The actions the engine cares about. The index doubles as BOTH the
 * joy-button slot input_3ds.cpp writes into joy_button_state[0][N] AND
 * the engine joystick-button index config.cpp::DefaultJoystickSettings
 * binds via AddButton( j, N, &u->... ). Those two MUST stay in lockstep
 * — a mismatch is exactly the "Next Weapon actually does Thrust Forward"
 * class of bug. Slots 2/3 are thrust (not weapon switch); real weapon
 * switching lives on slots 10-13 and on the D-pad POV (see config.cpp). */
enum controls_action_t {
	CTRL_FIRE_PRIMARY = 0,    /* slot 0  -> fire primary    */
	CTRL_FIRE_SECONDARY,      /* slot 1  -> fire secondary  */
	CTRL_MOVE_FORWARD,        /* slot 2  -> thrust forward  */
	CTRL_MOVE_BACK,           /* slot 3  -> thrust back     */
	CTRL_STRAFE_LEFT,         /* slot 4  -> strafe left     */
	CTRL_STRAFE_RIGHT,        /* slot 5  -> strafe right    */
	CTRL_HEADLIGHTS,          /* slot 6  -> headlights      */
	CTRL_REAR_VIEW,           /* slot 7  -> rear view       */
	CTRL_NITRO,               /* slot 8  -> turbo / nitro   */
	CTRL_DROP_MINE,           /* slot 9  -> drop mine       */
	CTRL_NEXT_PRIMARY,        /* slot 10 -> next primary    */
	CTRL_PREV_PRIMARY,        /* slot 11 -> prev primary    */
	CTRL_NEXT_SECONDARY,      /* slot 12 -> next secondary  */
	CTRL_PREV_SECONDARY,      /* slot 13 -> prev secondary  */
	CTRL_SLIDE_UP,            /* slot 14 -> slide up        */
	CTRL_SLIDE_DOWN,          /* slot 15 -> slide down      */
	CTRL_NUM_ACTIONS
};

struct controls_3ds_t {
	/* KEY_* mask per action (KEY_A, KEY_B, ..., KEY_ZL, KEY_ZR,
	 * KEY_L, KEY_R, KEY_START, KEY_SELECT). 0 = unbound. */
	u32 button[CTRL_NUM_ACTIONS];

	/* Analog stick mapping is fixed: the C-stick drives movement
	 * (thrust + strafe) and the circle pad drives look (yaw + pitch), routed
	 * in input_3ds.cpp into the engine axes bound in config.cpp. The only
	 * stick option is swap_sticks, which exchanges the two sticks' roles
	 * (so the circle pad moves and the C-stick looks). */
	int swap_sticks;         /* 1 = circle pad and C-stick exchange roles */
};

/* Initialize bindings: load defaults (platform-aware), then overlay
 * any saved customisations from sdmc:/3ds/forsaken/controls.cfg.
 * Safe to call multiple times. Pass is_n3ds = result of
 * APT_CheckNew3DS so OG gets the SELECT-as-nitro fallback. */
void controls_3ds_init(bool is_n3ds);

/* Save current bindings to sdmc:/3ds/forsaken/controls.cfg. */
void controls_3ds_save(void);

/* Restore the platform-default bindings (OG vs N3DS aware). Used by the
 * rebind menu's "Reset to Default" row. Does not save on its own. */
void controls_3ds_reset_defaults(void);

/* Get current KEY_* mask for an action. 0 if unbound. */
u32  controls_3ds_button(controls_action_t action);

/* Stick option getter for the input poll. */
int  controls_3ds_swap_sticks(void);

/* Modal bottom-screen rebind menu. Takes over the bottom screen
 * with consoleInit, runs a navigation loop (D-pad to select action,
 * A to rebind, B to back out, START to save & exit). Returns when
 * the user exits — caller should then re-establish their own
 * console / GFX state if they had one on the bottom screen. */

#endif /* __3DS__ */

/* Feed one frame of HID state to the remap overlay; true = it consumed input. */
bool controls_3ds_handle_input(u32 kDown, u32 kHeld);
