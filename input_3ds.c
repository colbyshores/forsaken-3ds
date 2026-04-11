/*
 * input_3ds.c - Nintendo 3DS HID input replacing input_sdl.c
 *
 * Maps 3DS controls to Forsaken's input system:
 *   Circle Pad  -> pitch / yaw
 *   C-Stick     -> strafe / vertical thrust  (New 3DS only)
 *   D-Pad       -> strafe / vertical thrust  (Old 3DS fallback)
 *   L/R         -> roll
 *   A           -> fire primary
 *   B           -> fire secondary
 *   X           -> next weapon
 *   Y           -> prev weapon
 *   Start       -> menu / pause
 *   Select      -> rear view
 */

#ifdef __3DS__

#include <3ds.h>
#include <string.h>
#include "main.h"
#include "input.h"
#include "util.h"

/* ---- globals expected by the engine ---- */

int Num_Joysticks = 1; /* pretend we have one joystick (the circle pad) */

JOYSTICKINFO JoystickInfo[MAX_JOYSTICKS];

bool input_grabbed = false;

/* old_input, new_input, mouse_state, mouse_states, input_buffer*,
   joy_axis_state, joy_button_state, joy_hat_state are all declared
   (as non-extern globals) in input.h and defined via controls.c.
   We do NOT redefine them here. */

/* ---- keyboard state array ---- */

/* Forsaken reads a key_state array indexed by SDLK_* values.
   On 3DS we don't have a keyboard, but the engine still references it.
   We provide a zeroed array so nothing crashes.
   This is extern'd in input.h as _3ds_key_state for SDL compat. */

u_int8_t _3ds_key_state[SDLK_LAST];

/* ---- init / cleanup ---- */

void input_grab(bool grab)
{
	input_grabbed = grab;
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

	JoystickInfo[0].assigned  = true;
	JoystickInfo[0].connected = true;
	JoystickInfo[0].Name      = "3DS Controls";
	JoystickInfo[0].NumButtons = 12;
	JoystickInfo[0].NumPOVs   = 1;
	JoystickInfo[0].NumAxis   = 4; /* circle pad X/Y + C-stick X/Y */

	/* name the axes */
	for (i = 0; i < JoystickInfo[0].NumAxis; i++)
	{
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
			return i;
	return -1;
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

/* Scale factor: circle pad raw range is roughly -155..+155
   We map to joystick axis range expected by the engine (±32767) */
#define CPAD_SCALE  (32767.0f / 155.0f)

bool handle_events(void)
{
	circlePosition cpad;
	circlePosition cstick;
	u_int32_t kDown, kHeld, kUp;

	/* pump HID */
	hidScanInput();

	kDown = hidKeysDown();
	kHeld = hidKeysHeld();
	kUp   = hidKeysUp();

	/* check for quit (Start + Select together) */
	if ((kHeld & KEY_START) && (kHeld & KEY_SELECT))
	{
		extern bool QuitRequested;
		extern void CleanUpAndPostQuit(void);
		CleanUpAndPostQuit();
		return true;
	}

	/* swap input buffers */
	old_input = new_input;
	new_input = (new_input + 1) % INPUT_BUFFERS;

	/* clear new state */
	memset(joy_button_state[0], 0, sizeof(joy_button_state[0]));
	memset(joy_hat_state[0],    0, sizeof(joy_hat_state[0]));
	memset(&mouse_states[new_input], 0, sizeof(mouse_state_t));
	input_buffer_reset();

	/* ---- analog sticks ---- */
	hidCircleRead(&cpad);
	hidCstickRead(&cstick);

	/* axis 0/1 = circle pad (yaw / pitch) */
	joy_axis_state[0][0] = (long)(cpad.dx  * CPAD_SCALE);
	joy_axis_state[0][1] = (long)(-cpad.dy * CPAD_SCALE); /* invert Y */

	/* axis 2/3 = C-stick (strafe / vertical) */
	joy_axis_state[0][2] = (long)(cstick.dx * CPAD_SCALE);
	joy_axis_state[0][3] = (long)(-cstick.dy * CPAD_SCALE);

	/* ---- buttons ---- */
	/* Map 3DS buttons to joystick button indices */
	joy_button_state[0][0] = (kHeld & KEY_A)     ? true : false; /* fire primary */
	joy_button_state[0][1] = (kHeld & KEY_B)     ? true : false; /* fire secondary */
	joy_button_state[0][2] = (kHeld & KEY_X)     ? true : false; /* next weapon */
	joy_button_state[0][3] = (kHeld & KEY_Y)     ? true : false; /* prev weapon */
	joy_button_state[0][4] = (kHeld & KEY_L)     ? true : false; /* roll left */
	joy_button_state[0][5] = (kHeld & KEY_R)     ? true : false; /* roll right */
	joy_button_state[0][6] = (kHeld & KEY_START) ? true : false; /* menu */
	joy_button_state[0][7] = (kHeld & KEY_SELECT)? true : false; /* rear view */
	joy_button_state[0][8] = (kHeld & KEY_ZL)    ? true : false; /* nitro */
	joy_button_state[0][9] = (kHeld & KEY_ZR)    ? true : false; /* drop mine */

	/* ---- D-pad as POV hat ---- */
	if (kHeld & KEY_DUP)    joy_hat_state[0][0][JOY_HAT_UP]    = 1;
	if (kHeld & KEY_DRIGHT) joy_hat_state[0][0][JOY_HAT_RIGHT] = 1;
	if (kHeld & KEY_DDOWN)  joy_hat_state[0][0][JOY_HAT_DOWN]  = 1;
	if (kHeld & KEY_DLEFT)  joy_hat_state[0][0][JOY_HAT_LEFT]  = 1;

	/* ---- inject key presses into input buffer for menu nav ---- */
	if (kDown & KEY_A)
		input_buffer[input_buffer_count++] = SDLK_RETURN;
	if (kDown & KEY_B)
		input_buffer[input_buffer_count++] = SDLK_ESCAPE;
	if (kDown & KEY_DUP)
		input_buffer[input_buffer_count++] = SDLK_UP;
	if (kDown & KEY_DDOWN)
		input_buffer[input_buffer_count++] = SDLK_DOWN;
	if (kDown & KEY_DLEFT)
		input_buffer[input_buffer_count++] = SDLK_LEFT;
	if (kDown & KEY_DRIGHT)
		input_buffer[input_buffer_count++] = SDLK_RIGHT;
	if (kDown & KEY_START)
		input_buffer[input_buffer_count++] = SDLK_ESCAPE;

	/* populate key_state for any code that reads it */
	memset(_3ds_key_state, 0, sizeof(_3ds_key_state));
	if (kHeld & KEY_A)      _3ds_key_state[SDLK_RETURN] = 1;
	if (kHeld & KEY_B)      _3ds_key_state[SDLK_ESCAPE] = 1;
	if (kHeld & KEY_DUP)    _3ds_key_state[SDLK_UP]     = 1;
	if (kHeld & KEY_DDOWN)  _3ds_key_state[SDLK_DOWN]   = 1;
	if (kHeld & KEY_DLEFT)  _3ds_key_state[SDLK_LEFT]   = 1;
	if (kHeld & KEY_DRIGHT) _3ds_key_state[SDLK_RIGHT]  = 1;
	if (kHeld & KEY_START)  _3ds_key_state[SDLK_ESCAPE]  = 1;
	if (kHeld & KEY_X)      _3ds_key_state[SDLK_SPACE]  = 1;

	return true;
}

#endif /* __3DS__ */
