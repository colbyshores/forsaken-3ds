#include "util/platform.h"
/*===================================================================
	controls_3ds.h — user-customizable button + C-stick bindings.

	Storage struct loaded from sdmc:/3ds/forsaken/controls.cfg at
	startup, falls back to platform defaults (different defaults
	for OG vs N3DS — OG lacks ZL/ZR so we move turbo to SELECT).
	Bottom-screen rebind menu invoked from the in-game pause menu
	or via the START+L chord. Save persists across sessions.
===================================================================*/

#ifndef CONTROLS_3DS_INCLUDED
#define CONTROLS_3DS_INCLUDED

#ifdef __3DS__

#include <stdbool.h>
#include <3ds.h>

/* The actions the engine cares about. The index doubles as BOTH the
 * joy-button slot input_3ds.c writes into joy_button_state[0][N] AND
 * the engine joystick-button index config.c::DefaultJoystickSettings
 * binds via AddButton( j, N, &u->... ). Those two MUST stay in lockstep
 * — a mismatch is exactly the "Next Weapon actually does Thrust Forward"
 * class of bug. Slots 2/3 are thrust (not weapon switch); real weapon
 * switching lives on slots 10-13 and on the D-pad POV (see config.c). */
typedef enum {
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
} controls_action_t;

typedef struct {
	/* KEY_* mask per action (KEY_A, KEY_B, ..., KEY_ZL, KEY_ZR,
	 * KEY_L, KEY_R, KEY_START, KEY_SELECT). 0 = unbound. */
	u32 button[CTRL_NUM_ACTIONS];

	/* C-stick mapping. The engine sees axis 0=yaw, 1=pitch from the
	 * circle pad; axis 2/3 from the C-stick. cstick_axis_x/y let the
	 * user remap which engine axis the C-stick drives, and inversion
	 * lets them flip pitch/yaw direction. */
	int cstick_axis_x;       /* 0..3 = engine axis index for stick X */
	int cstick_axis_y;       /* 0..3 = engine axis index for stick Y */
	int cstick_invert_x;     /* 0 or 1 */
	int cstick_invert_y;     /* 0 or 1 */
} controls_3ds_t;

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

/* C-stick getters for the input poll. */
int  controls_3ds_cstick_axis_x(void);
int  controls_3ds_cstick_axis_y(void);
int  controls_3ds_cstick_invert_x(void);
int  controls_3ds_cstick_invert_y(void);

/* Modal bottom-screen rebind menu. Takes over the bottom screen
 * with consoleInit, runs a navigation loop (D-pad to select action,
 * A to rebind, B to back out, START to save & exit). Returns when
 * the user exits — caller should then re-establish their own
 * console / GFX state if they had one on the bottom screen. */

#endif /* __3DS__ */

/* Feed one frame of HID state to the remap overlay; true = it consumed input. */
bool controls_3ds_handle_input(u32 kDown, u32 kHeld);

#endif /* CONTROLS_3DS_INCLUDED */
