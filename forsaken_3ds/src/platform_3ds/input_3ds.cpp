/* input_3ds.cpp -- input.h for the 3DS: HID buttons and sticks
 *
 * The 3DS presents as one joystick. handle_events runs once per frame: it
 * reads HID and routes the analog sticks to fixed engine axes — the circle
 * pad drives movement (thrust on the MoveForward axis, strafe on the
 * SlideLeft axis) and the C-stick drives look (yaw on RotateLeft, pitch on
 * RotateUp), each scaled to the engine's +-100 range and gated by the axis
 * deadzone; Swap Sticks exchanges the two sticks' roles. It drives joystick
 * button slots 0-15 from the user-rebindable
 * bindings in controls_3ds.cpp (slot = controls_action_t); reports the D-pad
 * as POV hat 0; and fakes SDL key codes (RETURN, ESCAPE, arrows) for the
 * menus in input_buffer and _3ds_key_state. config.cpp maps those slots and
 * the hat to ship actions. The default bindings are A/B fire, X/Y thrust,
 * L/R strafe, and on New 3DS SELECT rear view, ZL nitro, ZR drop mine. The
 * Old 3DS has no ZL/ZR, so there SELECT is nitro (needed to finish SP level
 * 3) and rear view and drop mine start unbound.
 *
 * The first poll checks APT_CheckNew3DS once and loads the bindings
 * (controls_3ds_init). While the remap overlay is open it takes all input
 * and the engine sees none. START+SELECT requests quit on New 3DS only (on
 * Old 3DS SELECT is held for nitro). old_input / new_input are swapped by
 * ReadInput, not here.
 */

#ifdef __3DS__

#include <3ds.h>
#include <string.h>
#include "game/main.h"
#include "ui/title.h"
#include "input/input.h"
#include "util/util.h"
#include "platform_3ds/controls_3ds.h"
#include "platform_3ds/keyboard_3ds.h"

/* ---- globals expected by the engine ---- */

int Num_Joysticks = 1; /* pretend we have one joystick (the circle pad) */

namespace {

/* The per-frame HID poll (one instance, g_hid): whether this is a New 3DS,
 * cached at the first poll (APT_CheckNew3DS is cheap but not free, no
 * point hitting it every frame), and whether that first poll, which also
 * loads the bindings, has run. Written once by the first handle_events,
 * read by every later one. */
class HidPoller {
	public:
	bool poll();

	private:
	bool s_is_n3ds = false;
	bool s_n3ds_checked = false;
};

} // namespace

JOYSTICKINFO JoystickInfo[MAX_JOYSTICKS];


/* old_input, new_input, mouse_state, mouse_states, input_buffer*,
   joy_axis_state, joy_button_state, joy_hat_state are declared extern
   in input.h and defined in controls.cpp. We do NOT redefine them here. */

/* ---- keyboard state array ---- */

/* Forsaken reads a key_state array indexed by SDLK_* values.
   On 3DS we don't have a keyboard, but the engine still references it.
   handle_events clears it each frame and sets RETURN, ESCAPE and SPACE
   from buttons.
   This is extern'd in input.h as _3ds_key_state for SDL compat. */

u_int8_t _3ds_key_state[SDLK_LAST];

/* ---- init / cleanup ---- */

void input_grab(bool grab)
{
}

bool joysticks_init(void)
{
	int i;

	memset(JoystickInfo,    0, sizeof(JoystickInfo));
	memset(joy_axis_state,  0, sizeof(joy_axis_state));
	memset(joy_button_state,0, sizeof(joy_button_state));
	memset(joy_hat_state,   0, sizeof(joy_hat_state));
	memset(_3ds_key_state, 0, sizeof(_3ds_key_state));

	Num_Joysticks = 1;

	JoystickInfo[0].assigned  = false; /* let DefaultJoystickSettings bind buttons */
	JoystickInfo[0].connected = true;
	static char name[] = "3DS Controls";	/* Name is a writable, owned string on PC */
	JoystickInfo[0].Name      = name;
	JoystickInfo[0].NumButtons = 16;  /* 0-9 core + 10-13 weapon-switch + 14-15 slide (see controls_3ds.h / config.cpp) */
	JoystickInfo[0].NumPOVs   = 1;
	JoystickInfo[0].NumAxis   = 4; /* circle pad X/Y + C-stick X/Y */

	/* name the axes */
	for (i = 0; i < JoystickInfo[0].NumAxis; i++)
	{
		JoystickInfo[0].Axis[i].exists = true;
		JoystickInfo[0].Axis[i].action = SHIPACTION_Nothing;
		JoystickInfo[0].Axis[i].inverted = false;
		JoystickInfo[0].Axis[i].deadzone = 30;
		JoystickInfo[0].Axis[i].fine = false;
	}

	DebugPrintf("joysticks_init: 3DS HID initialized\n");
	return true;
}

bool joysticks_cleanup(void)
{
	return true;
}

/* ---- buffered input ---- */

int input_buffer_find(int code)
{
	int i;
	for (i = 0; i < input_buffer_count; i++)
		if (input_buffer[i] == code)
			return 1;  /* found — truthy; return index would make index 0 falsy */
	return 0;  /* not found — falsy */
}

void input_buffer_reset(void)
{
	input_buffer_count = 0;
}

/* ---- mouse stub (no mouse on 3DS) ---- */

mouse_state_t* read_mouse(void)
{
	memset(&mouse_states[new_input], 0, sizeof(mouse_state_t));
	return &mouse_states[new_input];
}

/* ---- main event handler called every frame ---- */

/* Scale factor: circle pad raw range is roughly -155..+155.
 * The engine expects axis values in the range ±100 (NOT ±32767).
 * The SDL path divides SDL's ±32768 by 327.67 to get ±100.
 * We do the same: map ±155 to ±100. */
constexpr auto CPAD_SCALE = (100.0f / 155.0f);

bool HidPoller::poll()
{
	circlePosition cpad;
	circlePosition cstick;
	u_int32_t kDown, kHeld;
	int deadzone;

	/* A menu text field asked for the system keyboard: it opens here, before
	 * this pass draws anything. Afterwards scan twice, so the press that
	 * closed it is not also a new press for the game. */
	if (keyboard_3ds_service())
		hidScanInput();

	/* pump HID */
	hidScanInput();

	kDown = hidKeysDown();
	kHeld = hidKeysHeld();

	if (!s_n3ds_checked)
	{
		APT_CheckNew3DS(&s_is_n3ds);
		s_n3ds_checked = true;
		/* First-poll initialisation of the user-rebindable controls.
		 * Loads defaults (platform-aware via is_n3ds) then overlays
		 * any saved customisations from sdmc:/3ds/forsaken/controls.cfg. */
		controls_3ds_init(s_is_n3ds);
	}

	/* Controls menu input — runs first so menu can intercept B,
		 * D-pad, A, etc. before the engine's own handlers see them.
		 * Returns true if menu consumed input, in which case we still
		 * fall through to update axis/button state but the menu will
		 * have already absorbed the relevant kDown bits. */
	{
		if (controls_3ds_handle_input(kDown, kHeld)) {
			/* Menu open — fully suppress input propagation:
			 *   joy_*: gameplay axes/buttons
			 *   _3ds_key_state[]: SDL-shaped keyboard state the
			 *     engine's menu reads (without clearing here, the
			 *     pause menu sees stale RETURN/ESCAPE from prior
			 *     frames and re-fires "3DS Controls" → menu beeps
			 *     and selection resets every frame).
			 *   input_buffer[]: edge-triggered key events queue,
			 *     same problem — must be drained too. */
			memset(joy_button_state[0], 0, sizeof(joy_button_state[0]));
			memset(joy_axis_state[0],   0, sizeof(joy_axis_state[0]));
			memset(joy_hat_state[0],    0, sizeof(joy_hat_state[0]));
			memset(_3ds_key_state,      0, sizeof(_3ds_key_state));
			input_buffer_reset();
			return true;
		}
	}

	/* check for quit (Start + Select together) — N3DS only.
		 * On OG 3DS, SELECT is the nitro button so this chord would
		 * fire any time the player paused while nitroing. The player
		 * can quit via HOME on OG. */
	if (s_is_n3ds && (kHeld & KEY_START) && (kHeld & KEY_SELECT))
	{
		/* Only SIGNAL quit here — running CleanUpAndPostQuit in the middle
		 * of the input-poll call crashes on return, because ReadInput and
		 * the game loop continue to use render/sound/lua resources that
		 * the teardown just freed. Main loop sees QuitRequested on its
		 * next iteration and cleanup runs cleanly post-loop. */
		QuitRequested = true;
		return true;
	}

	/* Do NOT swap old_input/new_input here — ReadInput() in controls.cpp
	 * handles the swap.  The SDL input path (input_sdl.cpp) also does not
	 * swap here.  Double-swapping breaks button release detection: the
	 * old_input buffer is never written by joystick_poll, so
	 * JOYSTICK_BUTTON_RELEASED always returns false and AnyKeyReleased()
	 * never fires — preventing respawn after death. */

	/* clear current state */
	memset(joy_button_state[0], 0, sizeof(joy_button_state[0]));
	memset(joy_hat_state[0],    0, sizeof(joy_hat_state[0]));
	input_buffer_reset();

	/* ---- analog sticks ---- */
	/* irrstScanInput latches the C-stick / Circle Pad Pro sample for this
	 * frame; hidCstickRead (a macro for irrstCstickRead) returns the last
	 * latched value, so without this call the read is always zero. */
	irrstScanInput();
	hidCircleRead(&cpad);
	hidCstickRead(&cstick);

	/* The circle pad drives movement and the C-stick drives look; Swap
	 * Sticks exchanges the two. The engine axes are fixed in config.cpp:
	 *   AXIS_XAxis  -> RotateLeft  (yaw)     AXIS_ZAxis  -> MoveForward
	 *   AXIS_YAxis  -> RotateUp    (pitch)   AXIS_RxAxis -> SlideLeft (strafe)
	 * Signs match the digital buttons in control_ship(): stick up = forward,
	 * stick right = strafe right, stick right = look right, stick up = look
	 * up. Values are scaled to the engine's +-100 range and gated by the
	 * axis deadzone (the engine applies no deadzone of its own). */
	{
		int swap = controls_3ds_swap_sticks();
		const circlePosition *mv = swap ? &cstick : &cpad;   /* movement stick */
		const circlePosition *lk = swap ? &cpad   : &cstick; /* look stick     */
		deadzone = JoystickInfo[0].Axis[0].deadzone;

		long fwd    = (long)( mv->dy  * CPAD_SCALE);  /* up (-dy) -> forward      */
		long strafe = (long)( mv->dx  * CPAD_SCALE);  /* right    -> strafe right */
		long yaw    = (long)( lk->dx  * CPAD_SCALE);  /* right    -> look right   */
		long pitch  = (long)( -lk->dy * CPAD_SCALE);  /* up (-dy) -> look up      */

		memset(joy_axis_state[0], 0, sizeof(joy_axis_state[0]));
		if (abs(fwd)    > deadzone) joy_axis_state[0][AXIS_ZAxis]  = fwd;
		if (abs(strafe) > deadzone) joy_axis_state[0][AXIS_RxAxis] = strafe;
		if (abs(yaw)    > deadzone) joy_axis_state[0][AXIS_XAxis]  = yaw;
		if (abs(pitch)  > deadzone) joy_axis_state[0][AXIS_YAxis]  = pitch;
	}

	/* ---- buttons ---- */
	/* Drive joystick button slots from the user-rebindable bindings.
	 * Action enum index is the joy-button slot (see controls_3ds.h). */
	for (int act = 0; act < CTRL_NUM_ACTIONS; act++) {
		u32 mask = controls_3ds_button((controls_action_t)act);
		joy_button_state[0][act] = (mask && (kHeld & mask)) ? true : false;
	}



	/* ---- D-pad as POV hat ---- */
	if (kHeld & KEY_DUP)    joy_hat_state[0][0][JOY_HAT_UP]    = 1;
	if (kHeld & KEY_DRIGHT) joy_hat_state[0][0][JOY_HAT_RIGHT] = 1;
	if (kHeld & KEY_DDOWN)  joy_hat_state[0][0][JOY_HAT_DOWN]  = 1;
	if (kHeld & KEY_DLEFT)  joy_hat_state[0][0][JOY_HAT_LEFT]  = 1;

	/* ---- inject key presses into input buffer for menu nav ---- */
	/* B only injects ESCAPE when a menu is open (go back / close).
	 * During gameplay B is fire secondary (joy button 1). */
	{
		int menu_open = _3ds_is_menu_open();
		/* A confirms a menu item. In gameplay A must NOT drive RETURN: the
		 * engine binds send_msg to RETURN, so an ungated A opened the chat
		 * keyboard mid-match instead of strafing. Mirror the B/ESCAPE gate
		 * below — A is a menu key only while a menu is up. */
		if ((kDown & KEY_A) && menu_open)
			input_buffer[input_buffer_count++] = SDLK_RETURN;
		if ((kDown & KEY_B) && menu_open)
			input_buffer[input_buffer_count++] = SDLK_ESCAPE;
		if (kDown & KEY_DUP)
			input_buffer[input_buffer_count++] = SDLK_UP;
		if (kDown & KEY_DDOWN)
			input_buffer[input_buffer_count++] = SDLK_DOWN;
		if (kDown & KEY_DLEFT)
			input_buffer[input_buffer_count++] = SDLK_LEFT;
		if (kDown & KEY_DRIGHT)
			input_buffer[input_buffer_count++] = SDLK_RIGHT;
		/* Only inject ESCAPE from Start when Select is NOT held (N3DS only)
		 * — Select+Start is a common combo that crashes if the menu opens
		 * during rear view. On OG 3DS, SELECT is nitro so the player holds
		 * it constantly; the gate would block all pauses mid-flight. */
		if ((kDown & KEY_START) && (!s_is_n3ds || !(kHeld & KEY_SELECT)))
			input_buffer[input_buffer_count++] = SDLK_ESCAPE;

		/* populate key_state for any code that reads it.
		 * D-pad is NOT mapped here — the default keyboard config maps arrow keys
		 * to camera movement, which would override the D-pad POV hat weapon
		 * selection.  D-pad menu navigation uses input_buffer (kDown events above). */
		memset(_3ds_key_state, 0, sizeof(_3ds_key_state));
		if ((kHeld & KEY_A) && menu_open)
			_3ds_key_state[SDLK_RETURN] = 1;
		/* SELECT drives the chat keyboard in gameplay: send_msg is bound to
		 * RETURN, so holding SELECT (no menu up) raises the send_msg edge and
		 * opens the quick-text menu — the keyboard now lives on SELECT, not A.
		 * On New 3DS SELECT is also rear-view by default; rebind rear-view off
		 * SELECT in the controls menu if you want chat without the overlap. */
		if ((kHeld & KEY_SELECT) && !menu_open)
			_3ds_key_state[SDLK_RETURN] = 1;
		if ((kHeld & KEY_B) && menu_open)
			_3ds_key_state[SDLK_ESCAPE] = 1;
	}
	if ((kHeld & KEY_START) && !(kHeld & KEY_SELECT))
		_3ds_key_state[SDLK_ESCAPE] = 1;
	if (kHeld & KEY_X)      _3ds_key_state[SDLK_SPACE]  = 1;

	return true;
}

namespace { HidPoller g_hid; }

bool handle_events(void) { return g_hid.poll(); }

#endif /* __3DS__ */
