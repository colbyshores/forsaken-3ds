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

/* The actions the engine cares about. Order matches the 10 slots
 * input_3ds.c writes into joy_button_state[0][N], so the index
 * doubles as the joy-button slot. */
typedef enum {
	CTRL_FIRE_PRIMARY = 0,    /* slot 0 */
	CTRL_FIRE_SECONDARY,      /* slot 1 */
	CTRL_NEXT_WEAPON,         /* slot 2 */
	CTRL_PREV_WEAPON,         /* slot 3 */
	CTRL_STRAFE_LEFT,         /* slot 4 */
	CTRL_STRAFE_RIGHT,        /* slot 5 */
	CTRL_MENU,                /* slot 6 */
	CTRL_REAR_VIEW,           /* slot 7 */
	CTRL_NITRO,               /* slot 8 */
	CTRL_DROP_MINE,           /* slot 9 */
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
void controls_3ds_remap_menu(void);

#endif /* __3DS__ */

#endif /* CONTROLS_3DS_INCLUDED */
