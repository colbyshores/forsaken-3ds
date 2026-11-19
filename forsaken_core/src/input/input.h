#pragma once

/* input.h -- input seam: key, mouse and joystick state shared with the platform layer
 *
 * The platform layer (input_3ds.cpp on 3DS, input_sdl.cpp on PC) implements
 * handle_events and the joystick setup; controls.cpp owns the state buffers.
 * State is double-buffered by old_input / new_input, swapped once per frame
 * by ReadInput; platform code must not swap them as well, or every
 * PRESSED / RELEASED edge test reads two equal states. On 3DS there is no
 * SDL: the SDLK_* codes and SDL_GetKeyboardState are emulated over
 * _3ds_key_state. input_buffer collects key codes for the menus (mouse codes
 * above SDLK_LAST, joystick codes from DIK_JOYSTICK).
 */

#include "game/main.h"

#ifdef __3DS__
/* On 3DS we don't have SDL - define the key constants we actually use */
#define SDLK_LAST    512
/* Minimal SDL key defines for menu navigation */
#define SDLK_RETURN  13
#define SDLK_ESCAPE  27
#define SDLK_SPACE   32
#define SDLK_UP      273
#define SDLK_DOWN    274
#define SDLK_RIGHT   275
#define SDLK_LEFT    276
#define SDLK_BACKSPACE 8
#define SDLK_DELETE  127
#define SDLK_TAB     9
#define SDLK_F1      282
#define SDLK_F2      283
#define SDLK_F3      284
#define SDLK_F4      285
#define SDLK_F5      286
#define SDLK_F6      287
#define SDLK_F7      288
#define SDLK_F8      289
#define SDLK_F9      290
#define SDLK_F10     291
#define SDLK_F11     292
#define SDLK_F12     293
#define SDLK_a       97
#define SDLK_b       98
#define SDLK_c       99
#define SDLK_e       101
#define SDLK_h       104
#define SDLK_i       105
#define SDLK_j       106
#define SDLK_k       107
#define SDLK_l       108
#define SDLK_m       109
#define SDLK_q       113
#define SDLK_r       114
#define SDLK_s       115
#define SDLK_u       117
#define SDLK_y       121
#define SDLK_z       122
#define SDLK_0       48
#define SDLK_1       49
#define SDLK_2       50
#define SDLK_3       51
#define SDLK_4       52
#define SDLK_5       53
#define SDLK_6       54
#define SDLK_7       55
#define SDLK_8       56
#define SDLK_9       57
#define SDLK_MINUS   45
#define SDLK_LSHIFT  304
#define SDLK_RSHIFT  303
#define SDLK_LCTRL   306
#define SDLK_RCTRL   305
#define SDLK_LALT    308
#define SDLK_RALT    307
#define SDLK_PAGEUP  280
#define SDLK_PAGEDOWN 281
#define SDLK_HOME    278
#define SDLK_END     279
#define SDLK_INSERT  277
#define SDLK_KP_MINUS    269
#define SDLK_KP_PLUS     270
#define SDLK_BACKQUOTE   96

/* SDL compatibility stubs for 3DS */
#include <stdio.h>
#include <string.h>

/* Keyboard state - maintained by input_3ds.cpp */
extern u_int8_t _3ds_key_state[SDLK_LAST];
static inline u_int8_t* SDL_GetKeyboardState(int *numkeys) {
	if (numkeys) *numkeys = SDLK_LAST;
	return _3ds_key_state;
}
static inline u_int8_t* SDL_GetKeyState(int *numkeys) {
	return SDL_GetKeyboardState(numkeys);
}

/* Mod key state - always 0 on 3DS (no keyboard modifiers) */
#define KMOD_CTRL  0x0040
#define KMOD_SHIFT 0x0001
static inline int SDL_GetModState(void) { return 0; }

static inline const char* SDL_GetKeyName(int key)
{
	static char buf[16];
	if (key >= 32 && key < 127) { buf[0] = (char)key; buf[1] = 0; return buf; }
	switch(key) {
		case SDLK_RETURN: return "return";
		case SDLK_ESCAPE: return "escape";
		case SDLK_SPACE:  return "space";
		case SDLK_UP:     return "up";
		case SDLK_DOWN:   return "down";
		case SDLK_LEFT:   return "left";
		case SDLK_RIGHT:  return "right";
		case SDLK_BACKSPACE: return "backspace";
		case SDLK_TAB:    return "tab";
		case SDLK_LSHIFT: return "left shift";
		case SDLK_RSHIFT: return "right shift";
		case SDLK_LCTRL:  return "left ctrl";
		case SDLK_RCTRL:  return "right ctrl";
		case SDLK_LALT:   return "left alt";
		case SDLK_RALT:   return "right alt";
		default: snprintf(buf, sizeof(buf), "key%d", key); return buf;
	}
}
#else
#include <SDL.h>
#define SDLK_LAST 512
#endif /* __3DS__ */

#include "input/controls.h"

/////////////
// Buffers //
/////////////

constexpr int INPUT_BUFFERS = (2);

extern int old_input;
extern int new_input;

////////////
// Events //
////////////

/* Set while the platform layer resizes its own window, so the resize
   event it generates is not taken as a user request (stubbed on 3DS). */
extern bool bIgnoreWM_SIZE;
void input_grab( bool grab );

bool handle_events( void );

//////////////////
// Input Buffer //
//////////////////

// stores buffered input events
// mouse_input_enum if (code > SDL_LAST && < DIK_JOYSTICK)
// joystick event if code > DIK_JOYSTICK

// TODO - probably ok to rewrite this to simply have codes

constexpr int MAX_INPUT_BUFFER = 100;
extern int input_buffer[MAX_INPUT_BUFFER];
extern int input_buffer_count;
int input_buffer_find( int code );
void input_buffer_reset( void );

///////////
// Mouse //
///////////

constexpr int MAX_MOUSE_BUTTONS = (255);

typedef struct {

	// wheel state -1 (down) 0 (nothing) 1 (up)
	int wheel;

	// left (0) , middle (1) , right (2)
	int buttons[MAX_MOUSE_BUTTONS];

	// relative mouse movement
	int xrel;
	int yrel;

} mouse_state_t;

// this holds the current state each loop
extern mouse_state_t mouse_state;

// special keydefs for mouse actions
// this is only used by MenuProcess
enum {
	MOUSE_RANGE = SDLK_LAST,
	LEFT_MOUSE,
	MIDDLE_MOUSE,
	RIGHT_MOUSE,
	UP_MOUSE,
	DOWN_MOUSE
};

#define MOUSE_RANGE_END LEFT_MOUSE + MAX_MOUSE_BUTTONS

extern mouse_state_t mouse_states[ INPUT_BUFFERS ];

#define MOUSE_BUTTON_HELD( B )    ( mouse_states[ new_input ].buttons[ B ] )
#define MOUSE_BUTTON_PRESSED( B ) ( !( mouse_states[ old_input ].buttons[ B ] ) && ( mouse_states[ new_input ].buttons[ B ] ) )
#define MOUSE_BUTTON_RELEASED( B )  ( ( mouse_states[ old_input ].buttons[ B ] ) && !( mouse_states[ new_input ].buttons[ B ] ) )

#define MOUSE_WHEEL_UP()				( mouse_states[ new_input ].wheel > 0 )
#define MOUSE_WHEEL_DOWN()				( mouse_states[ new_input ].wheel < 0 )
#define MOUSE_WHEEL_UP_PRESSED()		( !( mouse_states[ old_input ].wheel > 0 ) && ( mouse_states[ new_input ].wheel > 0 ) )
#define MOUSE_WHEEL_DOWN_PRESSED()      ( !( mouse_states[ old_input ].wheel < 0 ) && ( mouse_states[ new_input ].wheel < 0 ) )

mouse_state_t* read_mouse(void);

///////////////
// Joysticks //
///////////////

constexpr int MAX_JOYSTICKS = 16;
constexpr int MAX_JOYSTICK_BUTTONS = 128;
constexpr int MAX_JOYSTICK_POVS = 4;
constexpr int MAX_JOYSTICK_AXIS = 8;
constexpr int MAX_JOYNAME = 16;

extern int Num_Joysticks;

typedef struct {
	bool assigned;
	bool connected;
#if !defined(__3DS__)
	SDL_Joystick * sdl_joy;
#endif
	char *Name;
	int NumButtons;
	JOYSTICKBTN Button[MAX_JOYSTICK_BUTTONS];
	int NumPOVs;
	JOYSTICKPOV POV[MAX_JOYSTICK_POVS];
	int NumAxis;
	JOYSTICKAXIS Axis[MAX_JOYSTICK_AXIS];
} JOYSTICKINFO;

extern JOYSTICKINFO JoystickInfo[MAX_JOYSTICKS];

bool joysticks_init(void);
bool joysticks_cleanup( void );

// this holds the current state joysticks
extern long joy_axis_state[ MAX_JOYSTICKS ][ MAX_JOYSTICK_AXIS ];
extern bool joy_button_state[ MAX_JOYSTICKS ][ MAX_JOYSTICK_BUTTONS ];
// hat has 4 directions, diagnols turn on both adjecent angles
extern u_int8_t joy_hat_state[ MAX_JOYSTICKS ][ MAX_JOYSTICK_POVS ][ MAX_POV_DIRECTIONS ];

enum {
	JOY_HAT_UP,
	JOY_HAT_RIGHT,
	JOY_HAT_DOWN,
	JOY_HAT_LEFT
};

// TODO - we need settings/implementation for joystick dead zones
