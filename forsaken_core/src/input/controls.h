#pragma once

/* controls.h -- ship controls: per-frame SHIPCONTROL, bindings and flight constants
 *
 * controls.cpp owns ReadInput (the once-per-frame input read) and
 * control_ship, which maps the player's USERCONFIG bindings onto a
 * SHIPCONTROL each frame; it also holds the HUD display toggles, menu flash
 * text and cheat codes. The flight constants are per framelag tick: speeds in
 * world units (scaled by GLOBAL_SCALE), turn and roll rates and MAXBANKANGLE
 * in degrees.
 */

#ifndef __3DS__
#include <SDL.h>
#endif
#include "game/main.h"
#include "data/config.h"

/*===================================================================
		Some Keyboard and Mouse Globals
===================================================================*/

constexpr int CRUISE_NORMAL = (0);
constexpr int CRUISE_FORWARD = (1);
constexpr int CRUISE_NITRO = (2);
constexpr int CRUISE_REVERSE = (-1);

#define MAXMOVESPEED	(45.0F*GLOBAL_SCALE)
constexpr float MOVEACCELL = 0.065F;
constexpr float MOVEDECELL = 0.0611F;
#define MAXTURBOSPEED   (MAXMOVESPEED * 2.0F)
constexpr float TURBOACCELL = 0.13F;
constexpr float TURBODECELL = 0.1151F;
constexpr float MAXTURNSPEED = 2.625F;
constexpr float TURNACCELL = 0.065F;
constexpr float TURNDECELL = 0.0611F;
constexpr float MAXROLLSPEED = 1.875F;
constexpr float ROLLACCELL = 0.25F;
constexpr float ROLLDECELL = 0.2F;
constexpr float MAXBANKANGLE = 30.0F;
constexpr float BANKACCELL = 0.03F;
constexpr float BANKDECELL = 0.03F;

#define CLAMP( V, MAX )\
	if ( (V) > (MAX) ) { (V) = (MAX);  }\
	else if ( (V) < -(MAX) ) { (V) = -(MAX); }

/* ship control structures */

typedef struct {
	float pitch;
	float yaw;
	float roll;
	float bank;
	float right;
	float up;
	float forward;
	int cruise_control;
	int turbo;
	int slide_mode;
	int roll_mode;
	int fire_primary;
	int fire_secondary;
	int fire_mine;
	int select_primary;			// #(1 to MAX_PRIMARY_WEAPONS) of primary weapon to select, or 0 otherwise
	int select_secondary;		// #(1 to MAX_SECONDARY_WEAPONS) of secondary weapon to select, or 0 otherwise
	int select_next_primary;
	int select_prev_primary;
	int select_next_secondary;
	int select_prev_secondary;
	int drop_primary;
	int drop_secondary;
	int drop_shield;
	int drop_ammo;
} SHIPCONTROL;

typedef struct {
	bool exists;
	char *name;
	int action;
	float sensitivity;
	int deadzone;	// percentage
	bool inverted;
	bool fine;
} JOYSTICKAXIS;

typedef struct {
	char *name;
	int action;
} JOYSTICKBTN;

constexpr int MAX_POV_DIRECTIONS = 4;
constexpr int POV_Centre = 0;
constexpr int POV_Up = 1;
constexpr int POV_Down = 2;
constexpr int POV_Left = 4;
constexpr int POV_Right = 8;

typedef struct {
	char *name;
	char *dirname[MAX_POV_DIRECTIONS];
	int action[MAX_POV_DIRECTIONS];
} JOYSTICKPOV;

enum
{
	AXIS_XAxis,
	AXIS_YAxis,
	AXIS_ZAxis,
	AXIS_RxAxis,
	AXIS_RyAxis,
	AXIS_RzAxis,
	AXIS_SliderAxis0,
	AXIS_SliderAxis1,
};					 

#define AXIS_Start AXIS_XAxis
#define AXIS_End AXIS_SliderAxis1

#define NUM_SHIP_ACTIONS		SHIPACTION_MAX
constexpr int NUM_SHIP_AXIS_ACTIONS = 7;

enum {
	SHIPACTION_Nothing,				// 0
	SHIPACTION_RotateUp,			// 1
	SHIPACTION_RotateDown,			// 2
	SHIPACTION_RotateLeft,			// 3
	SHIPACTION_RotateRight,			// 4
	SHIPACTION_RollLeft,			// 5
	SHIPACTION_RollRight,			// 6
	SHIPACTION_SlideUp,				// 7
	SHIPACTION_SlideDown,			// 8
	SHIPACTION_SlideLeft,			// 9
	SHIPACTION_SlideRight,			// 10
	SHIPACTION_MoveBack,			// 11
	SHIPACTION_MoveForward,			// 12
	SHIPACTION_CruiseIncrease,		// 13
	SHIPACTION_CruiseDecrease,		// 14
	SHIPACTION_Turbo,				// 15
	SHIPACTION_FirePrimary,			// 16
	SHIPACTION_FireSecondary,		// 17
	SHIPACTION_DropMine,			// 18
	SHIPACTION_SelectNextPrimary,	// 19
	SHIPACTION_SelectPrevPrimary,	// 20
	SHIPACTION_SelectNextSecondary, // 21
	SHIPACTION_SelectPrevSecondary, // 22
	SHIPACTION_DropPrimary,			// 23
	SHIPACTION_DropSecondary,		// 24
	SHIPACTION_DropShield,			// 25
	SHIPACTION_DropAmmo,			// 26
	SHIPACTION_SlideMode,			// 27
	SHIPACTION_RollMode,			// 28
	SHIPACTION_MAX					// always last in list
};

enum {
	JOYSTICK_XAxisPos,
	JOYSTICK_YAxisPos,
	JOYSTICK_ZAxisPos,
	JOYSTICK_XAxisRot,
	JOYSTICK_YAxisRot,
	JOYSTICK_ZAxisRot,
	JOYSTICK_ExtraAxisPos,
	JOYSTICK_POV0Up = JOYSTICK_ExtraAxisPos + 2,	// 2 possible extra axis
	JOYSTICK_POV0Down,
	JOYSTICK_POV0Left,
	JOYSTICK_POV0Right,
	JOYSTICK_Button0 = JOYSTICK_POV0Up + (4 * 4),		// 4 POV directions, 4 possible POV controls
};

extern void
control_ship( USERCONFIG *conf, SHIPCONTROL *ctrl );

extern int
AnyKeyReleased( void );

extern void
ReadInput( void );

void ReadJoystickInput(SHIPCONTROL *ctrl, int joysticknum);
void SetUpJoystickAxis(int joystick);
bool IsJoystickButtonPressed( int joysticknum );
bool IsJoystickButtonReleased( int joysticknum );
bool IsAnyJoystickButtonReleased( void );
int ToggleGodMode( char *cheat );
void FlashMenuText( const char * text, float activetime, u_int16_t sfx );
void ProcessMenuFlashText( void );
void DisableCheats( void );

/* Defined in controls.cpp; shared with the modules that use it. */
extern const char *JoystickPOVDirections[ MAX_POV_DIRECTIONS ];
extern bool FullRearView;
extern bool ShowStats;
extern bool ShowMessages;
extern bool ShowStatistics;
extern bool ShowNetworkInfo;
extern bool flush_input;
void clear_last_mouse_state(void);
extern float FlashTextActive;
extern const char *ShipAxisSeperateText[ NUM_SHIP_AXIS_ACTIONS*2 ];
extern const char *ShipAxisText[ NUM_SHIP_AXIS_ACTIONS ];
extern int ShipAxisLookup[ NUM_SHIP_AXIS_ACTIONS ];
extern const char *ShipActionText[ NUM_SHIP_ACTIONS ];
void CheckCheats( int key );
