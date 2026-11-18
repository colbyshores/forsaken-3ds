/* handle keyboard and mouse control */

#include <math.h>

#include "input/input.h"
#include "game/oct2.h"
/* Definitions for input.h externs (were tentative definitions in the header;
   illegal as C++ TUs now include input.h). controls.cpp is the owner. */
int input_buffer[MAX_INPUT_BUFFER];
mouse_state_t mouse_state;
mouse_state_t mouse_states[ INPUT_BUFFERS ];
long joy_axis_state[ MAX_JOYSTICKS ][ MAX_JOYSTICK_AXIS ];
bool joy_button_state[ MAX_JOYSTICKS ][ MAX_JOYSTICK_BUTTONS ];
u_int8_t joy_hat_state[ MAX_JOYSTICKS ][ MAX_JOYSTICK_POVS ][ MAX_POV_DIRECTIONS ];
int input_buffer_count;

#include "data/config.h"
#include "ui/title.h"
#include "input/controls.h"
#include <stdio.h>
#include "rendering/text.h"
#include "game/main.h"
#include "game/compobjects.h"
#include "math/quat.h"
#include "game/object.h"
#include "net/networking.h"
#include "game/primary.h"
#include "game/secondary.h"
#include "audio/sfx.h"
#include "rendering/render.h"
#include "util/util.h"

// TEMP!! - for testing looping SFX
#include "audio/sfx.h"
#include "game/compobjects.h"
#include "math/quat.h"
#include "game/object.h"
#include "net/networking.h"
#include "game/ships.h"

/*===================================================================
	Some Keyboard and Mouse Globals
===================================================================*/


#define KEY_HELD( K )     ( key_state[ new_input ][ K ] )
#define KEY_PRESSED( K )    ( !( key_state[ old_input ][ K ] ) && ( key_state[ new_input ][ K ] ) )
#define KEY_RELEASED( K )   ( ( key_state[ old_input ][ K ] ) && !( key_state[ new_input ][ K ] ) )


#define JOYSTICK_BUTTON_HELD( J, B )    ( js_buttons[ new_input ][ J ][ B ] )
#define JOYSTICK_BUTTON_PRESSED( J, B )   ( !( js_buttons[ old_input ][ J ][ B ]) && ( js_buttons[ new_input ][ J ][ B ] ) )
#define JOYSTICK_BUTTON_RELEASED( J, B )  ( ( js_buttons[ old_input ][ J ][ B ]) && !( js_buttons[ new_input ][ J ][ B ] ) )

#define JOYSTICK_POV_HELD( J, P, D )    ( js_pov[ new_input ][ J ][ P ][ D ] )
#define JOYSTICK_POV_PRESSED( J, P, D )   ( !( js_pov[ old_input ][ J ][ P ][ D ] ) && ( js_pov[ new_input ][ J ][ P ][ D ] ) )
#define JOYSTICK_POV_RELEASED( J, P, D )  ( ( js_pov[ old_input ][ J ][ P ][ D ] ) && !( js_pov[ new_input ][ J ][ P ][ D ] ) )


bool CheatsEnabled = false;


int js_buttons[ INPUT_BUFFERS ][ MAX_JOYSTICKS ][ MAX_JOYSTICK_BUTTONS ];

int old_input = 0;
int new_input = 1;

bool joystick_poll( int joysticknum );

u_int8_t
js_pov[ INPUT_BUFFERS ][ MAX_JOYSTICKS ][ MAX_JOYSTICK_POVS ][ MAX_POV_DIRECTIONS ];

bool flush_input = true;

u_int8_t key_state[ INPUT_BUFFERS ][ SDLK_LAST ];

const char *ShipActionText[NUM_SHIP_ACTIONS] = {
  "No Action",
  "Rotate Up",
  "Rotate Down",
  "Rotate Left",
  "Rotate Right",
  "Roll Left",
  "Roll Right",
  "Slide Up",
  "Slide Down",
  "Slide Left",
  "Slide Right",
  "Move Forward",
  "Move Back",
  "Cruise Increase",
  "Cruise Decrease",
  "Nitro",
  "Fire Primary",
  "Fire Secondary",
  "Drop Mine",
  "Next Primary",
  "Prev Primary",
  "Next Secondary",
  "Prev Secondary",
  "Drop Primary",
  "Drop Secondary",
  "Drop Shield",
  "Drop Ammo",
  "Slide Mode",
  "Roll Mode",
};

const char *ShipAxisText[NUM_SHIP_AXIS_ACTIONS] = {
  "No Action",
  "Rotate Up/Down",
  "Rotate Left/Right",
  "Roll Left/Right",
  "Slide Up/Down",
  "Slide Left/Right",
  "Move Forward/Back",
};

const char *ShipAxisSeperateText[NUM_SHIP_AXIS_ACTIONS * 2] = {
  "No Action",
  "No Action",
  "Rotate Up",
  "Rotate Down",
  "Rotate Left",
  "Rotate Right",
  "Roll Left",
  "Roll Right",
  "Slide Up",
  "Slide Down",
  "Slide Left",
  "Slide Right",
  "Move Forward",
  "Move Back",
};

char FlashText[ 128 ];
float FlashTextActive = 0.0F;

bool FullRearView = false;
bool ShowStats = false;
bool ShowMessages = false;	// display long list of chat messages
bool ShowStatistics = false;	// display statistics in game
bool ShowNetworkInfo = false;	// display enet info in game

void FlashMenuText( const char * text, float activetime, u_int16_t sfx )
{
  if( text )
  {
	strcpy( FlashText, text );
	FlashTextActive = activetime;
	if( sfx != 0xFF )
	{
	  PlaySfx( sfx, 1.0F );
	}
  }
}

void ProcessMenuFlashText( void )
{
  if ( FlashTextActive > 0.0F )
  {
	FlashTextActive -= framelag;
	if ( FlashTextActive > 0.0F )
	{
	  CenterPrint4x5Text( FlashText , render_info.window_size.cy - FontHeight * 2, 2 );
	}
  }
}

int EnableCheats( char * /*cheat*/ )
{
  CheatsEnabled = true;
  FlashMenuText( "cheat mode enabled", 120.0F, SFX_Secret );
  Cheated = true;

  return 1;
}

int JimBeam( char * /*cheat*/ )
{
  if( PrimaryWeaponCheat )
  {
	PrimaryToFireLookup[ PULSAR ] = PULSAR;
	PrimaryToFireLookup[ TROJAX ] = TROJAX;
	PrimaryToFireLookup[ PYROLITE_RIFLE ] = PYROLITE_RIFLE;
	PrimaryToFireLookup[ TRANSPULSE_CANNON ] = TRANSPULSE_CANNON;
	PrimaryToFireLookup[ SUSS_GUN ] = SUSS_GUN;
	PrimaryToFireLookup[ LASER ] = LASER;
	PrimaryWeaponCheat = false;
	FlashMenuText( "Beam me down", 120.0F, SFX_Secret );
  }
  else
  {
	PrimaryWeaponCheat = true;
	FlashMenuText( "Beam me up", 120.0F, SFX_Secret );
  }
  return 1;
}

int Lumberjack( char * /*cheat*/ )
{
  if( SecondaryWeaponCheat )
  {
	FlashMenuText( "...and he sleeps all day", 120.0F, SFX_Secret );
	SecondaryToFireLookup[ MUGMISSILE ] = MUGMISSILE;
	SecondaryToFireLookup[ SOLARISMISSILE ] = SOLARISMISSILE;
	SecondaryToFireLookup[ THIEFMISSILE ] = THIEFMISSILE;
	SecondaryToFireLookup[ SCATTERMISSILE ] = SCATTERMISSILE;
	SecondaryToFireLookup[ GRAVGONMISSILE ] = GRAVGONMISSILE;
	SecondaryToFireLookup[ MULTIPLEMISSILE ] = MULTIPLEMISSILE;
	SecondaryToFireLookup[ TITANSTARMISSILE ] = TITANSTARMISSILE;
	SecondaryToFireLookup[ PURGEMINE ] = PURGEMINE;
	SecondaryToFireLookup[ PINEMINE ] = PINEMINE;
	SecondaryToFireLookup[ QUANTUMMINE ] = QUANTUMMINE;
	SecondaryToFireLookup[ SPIDERMINE ] = SPIDERMINE;
	SecondaryWeaponCheat = false;
  }
  else
  {
	SecondaryWeaponCheat = true;
	FlashMenuText( "He works all night...", 120.0F, SFX_Secret );
  }
  return 1;
}

int ToggleGodMode( char * /*cheat*/ )
{
  if( !GodMode )
  {
	GodMode = true;
	FlashMenuText( "God mode enabled", 120.0F, SFX_Secret );
	GivemeAllWeapons();
  }
  else
  {
	FlashMenuText( "God mode disabled", 120.0F, SFX_Secret );
	LoseAllWeapons();
	GodMode = false;
  }
  return 1;
}

static struct
{
  unsigned char cheatcode[ 16 ];
  int (*cheatfunc)( char * );
  bool allowmultiplayer;
  int next;
} CheatTable[] =
{
  { { SDLK_b, SDLK_u, SDLK_b, SDLK_b, SDLK_l, SDLK_e, SDLK_s, 0 }, EnableCheats, true, 0 },
  { { SDLK_j, SDLK_i, SDLK_m, SDLK_b, SDLK_e, SDLK_a, SDLK_m, 0 }, JimBeam, false, 0 },
  { { SDLK_l, SDLK_u, SDLK_m, SDLK_b, SDLK_e, SDLK_r, SDLK_j, SDLK_a, SDLK_c, SDLK_k, 0 }, Lumberjack, false, 0 },
  { { SDLK_i, SDLK_a, SDLK_m, SDLK_z, SDLK_e, SDLK_u, SDLK_s, 0 }, ToggleGodMode, false, 0 },
};
// NOTE: add any new cheats to DisableCheats function to ensure they are not active in multiplayer game

int ShipAxisLookup[NUM_SHIP_AXIS_ACTIONS] = { SHIPACTION_Nothing, SHIPACTION_RotateUp, SHIPACTION_RotateLeft, SHIPACTION_RollLeft,
						SHIPACTION_SlideUp, SHIPACTION_SlideLeft, SHIPACTION_MoveForward };


// this function receives higher level encoded bits

short key_pressed( USERKEY *k )
{
  int j;
  VirtualKeycode c;

  for ( j = 0; j < k->keys; j++ )
  {
	c = k->key[ j ];
	if ( KEY_ON_KEYBOARD( c ) )
	{
	  if ( KEY_PRESSED( c ) )
		return 1;
	}
	else if ( KEY_ON_MOUSE( c ) )
	{
	  switch( c )
	  {
	  case UP_MOUSE:
		if ( MOUSE_WHEEL_UP_PRESSED() )
		  return 1;
		break;
	  case DOWN_MOUSE:
		if ( MOUSE_WHEEL_DOWN_PRESSED() )
		  return 1;
		break;
	  default:
		if ( MOUSE_BUTTON_PRESSED( c - LEFT_MOUSE ) )
		  return 1;
		break;
	  }
	}
	else if ( KEY_ON_JOYSTICK( c ) )
	{
	  int joystick;

	  joystick = KEY_JOYSTICK( c );
	  if ( !JoystickInfo[ joystick ].connected )
		continue;
	  if ( KEY_ON_JOYSTICK_BUTTON( c ) )
	  {
		int button;

		button = KEY_JOYSTICK_BUTTON( c );
		if ( JOYSTICK_BUTTON_PRESSED( joystick, button ) )
		  return 1;
	  }
	  else if ( KEY_ON_JOYSTICK_POV( c ) )
	  {
		int pov, dir;

		pov = KEY_JOYSTICK_POV( c );
		dir = KEY_JOYSTICK_POVDIR( c );
		if ( JOYSTICK_POV_PRESSED( joystick, pov, dir ) )
		  return 1;
	  }
	}
  }
  return 0;
}

// this function receives higher level encoded bits

short key_held( USERKEY *k )
{
  int j;
  VirtualKeycode c;

  for ( j = 0; j < k->keys; j++ )
  {
	c = k->key[ j ];
	if ( KEY_ON_KEYBOARD( c ) )
	{
	  if ( KEY_HELD( c ) )
		return 1;
	}
	else if ( KEY_ON_MOUSE( c ) )
	{
	  switch( c )
	  {
	  case UP_MOUSE:
		if ( MOUSE_WHEEL_UP() )
		  return 1;
		break;
	  case DOWN_MOUSE:
		if ( MOUSE_WHEEL_DOWN() )
		  return 1;
		break;
	  default:
		if ( MOUSE_BUTTON_HELD( c - LEFT_MOUSE ) )
		  return 1;
		break;
	  }
	}
	else if ( KEY_ON_JOYSTICK( c ) )
	{
	  int joystick;

	  joystick = KEY_JOYSTICK( c );

	  if ( !JoystickInfo[ joystick ].connected )
		continue;

	  if ( KEY_ON_JOYSTICK_BUTTON( c ) )
	  {
		int button;

		button = KEY_JOYSTICK_BUTTON( c );
		if ( JOYSTICK_BUTTON_HELD( joystick, button ) )
		  return 1;
	  }
	  else if ( KEY_ON_JOYSTICK_POV( c ) )
	  {
		int pov, dir;
		pov = KEY_JOYSTICK_POV( c );
		dir = KEY_JOYSTICK_POVDIR( c );
		if ( JOYSTICK_POV_HELD( joystick, pov, dir ) )
		  return 1;
	  }
	}
  }
  return 0;
}

void DisableCheats( void )
{
  CheatsEnabled = false;
  DebugInfo = false;

  // used to indicate cheats disabled for multiplayer game...
  CheatsDisabled = true;

  if( PrimaryWeaponCheat )
	JimBeam( NULL );
  if( SecondaryWeaponCheat )
	Lumberjack( NULL );
  if( GodMode )
	ToggleGodMode( NULL );
}

void CheckCheats( int key )
{
  int j, i;
  int cheats;
  unsigned char ch;

  cheats = sizeof( CheatTable ) / sizeof( CheatTable[ 0 ] );
  if ( key )
  {
	for ( j = 0; j < cheats; j++ )
	{
	  ch = CheatTable[ j ].cheatcode[ CheatTable[ j ].next ];
	  if ( key == ch )
	  {
		CheatTable[ j ].next++;
		if ( !CheatTable[ j ].cheatcode[ CheatTable[ j ].next ] )
		{
		  if ( ( MyGameStatus != STATUS_Normal ) || ( CheatTable[ j ].allowmultiplayer ) )
		  {
			if ( CheatTable[ j ].cheatfunc && ( CheatsEnabled || !j ) )
			{
			  CheatTable[ j ].cheatfunc( (char*)CheatTable[ j ].cheatcode );

			  // reset all cheats...
			  for( i = 0; i < cheats; i++ )
			  {
				CheatTable[ i ].next = 0;
			  }
			  break;
			}
		  }
		  CheatTable[ j ].next = 0;
		}
	  }
	  else
	  {
		CheatTable[ j ].next = 0;
	  }
	}
  }
}

static void ReadKeyboard( void )
{
	int i, nkeys;
	u_int8_t * keys;

	if( flush_input )
	{
		memset( &key_state[ new_input ], 0, SDLK_LAST );
		return;
	}

#if SDL_VERSION_ATLEAST(2,0,0)
	keys = (u_int8_t*)SDL_GetKeyboardState( &nkeys );
#else
	keys = SDL_GetKeyState( &nkeys );
#endif

	memset( &key_state[ new_input ], 0, SDLK_LAST );

	for( i = 0; i < SDLK_LAST; i++ )
		key_state[ new_input ][ i ] = keys[ i ];
}

void clear_last_mouse_state(void)
{
	int i;
	for ( i = 0; i < MAX_MOUSE_BUTTONS; i++ )
	{
		mouse_states[ new_input ].buttons[ i ] = 0;
		mouse_states[ old_input ].buttons[ i ] = 0;
		mouse_state.buttons[ i ] = 0;
	}
}

static void ReadMouse( void )
{
	int i;
	mouse_state_t * state = read_mouse();
	mouse_states[ new_input ].xrel = state->xrel;
	mouse_states[ new_input ].yrel = state->yrel;
	mouse_states[ new_input ].wheel = mouse_state.wheel;
	for ( i = 0; i < MAX_MOUSE_BUTTONS; i++ )
		mouse_states[ new_input ].buttons[ i ] = mouse_state.buttons[ i ];
}

void ReadInput( void )
{
	int i;

	if ( WaitingToQuit )
		return;

	old_input = new_input;
	new_input++;

	if ( new_input >= INPUT_BUFFERS )
		new_input = 0;

	ReadMouse();
	ReadKeyboard();

	if ( JoystickInput )
	{
		for (i = 0; i < Num_Joysticks; i++)
		{
			if (JoystickInfo[i].connected)
				joystick_poll(i);
		}
	}

	flush_input = false;
}

void control_ship( USERCONFIG *conf, SHIPCONTROL *ctrl )
{
  int   j, joystick;
  float mouse_dx;
  float mouse_dy;
  float pitch_sign;
  float turn_sign;
  float MaxMove, MaxTurbo, MaxTurn, MaxRoll, MaxBank;

  ctrl->pitch   = 0.0F;
  ctrl->yaw   = 0.0F;
  ctrl->roll    = 0.0F;
  ctrl->bank    = 0.0F;
  ctrl->right   = 0.0F;
  ctrl->up    = 0.0F;
  ctrl->forward = 0.0F;
  ctrl->cruise_control = 0;
  ctrl->turbo   = 0;
  ctrl->fire_primary = 0;
  ctrl->fire_secondary = 0;
  ctrl->fire_mine = 0;
  ctrl->select_primary = 0;
  ctrl->select_secondary = 0;
  ctrl->select_next_primary = 0;
  ctrl->select_prev_primary = 0;
  ctrl->select_next_secondary = 0;
  ctrl->select_prev_secondary = 0;
  ctrl->drop_primary = 0;
  ctrl->drop_secondary = 0;
  ctrl->drop_shield = 0;
  ctrl->drop_ammo = 0;

  if ( !conf )
	return; // bail if no config supplied

  if ( CurrentMenu )
	return; // disable bike controls if using menus

  mouse_dx = mouse_states[ new_input ].xrel * conf->mouse_x_sensitivity * 4.0F;
  mouse_dy = mouse_states[ new_input ].yrel * conf->mouse_y_sensitivity * 4.0F;


// Squared mouse response (dx * |dx|). TODO: make it a pilot setting; the
// size of relative mouse motion varies by platform and resolution.
	mouse_dx = mouse_dx * (float) fabsf( mouse_dx ) * 0.01f; // tweaked for linux
	mouse_dy = mouse_dy * (float) fabsf( mouse_dy ) * -0.01f;


  pitch_sign = ( conf->invert_pitch ) ? -1.0F : 1.0F;
  turn_sign = ( conf->invert_turn ) ? -1.0F : 1.0F;

  if ( key_held( &conf->full_rear_view ) )
	FullRearView = true;
  else
	FullRearView = false;

  if( key_held( &conf->show_messages))
	  ShowMessages = true;
  else
	  ShowMessages = false;

  if( key_held( &conf->show_stats))
	  ShowStatistics = true;
  else
	  ShowStatistics = false;

  if( key_held( &conf->show_networkinfo))
	  ShowNetworkInfo = true;
  else
	  ShowNetworkInfo = false;

  if ( key_pressed( &conf->headlights ) )
		Ships[WhoIAm].headlights = !Ships[WhoIAm].headlights;


  ctrl->slide_mode = key_held( &conf->move );
  ctrl->roll_mode = key_held( &conf->roll );
  if ( ctrl->slide_mode )
  {
	if ( key_held( &conf->left ) )
	{
	  ctrl->right -= MoveAccell * MaxMoveSpeed * framelag;
	}
	if ( key_held( &conf->right ) )
	{
	  ctrl->right += MoveAccell * MaxMoveSpeed * framelag;
	}
	if ( mouse_dx != 0.0F )
	{
	  ctrl->right += MoveAccell * MaxMoveSpeed * mouse_dx;
	}

	if ( key_held( &conf->up ) )
	{
	  ctrl->up += MoveAccell * MaxMoveSpeed * framelag;
	}
	if ( key_held( &conf->down ) )
	{
	  ctrl->up -= MoveAccell * MaxMoveSpeed * framelag;
	}
	if ( mouse_dy != 0.0F )
	{
	  ctrl->up += MoveAccell * MaxMoveSpeed * mouse_dy;
	}
  }
  else
  {
	if ( ctrl->roll_mode )
	{
	  if ( key_held( &conf->left ) )
	  {
		ctrl->roll += RollAccell * MaxRollSpeed * framelag;
	  }
	  if ( key_held( &conf->right ) )
	  {
		ctrl->roll -= RollAccell * MaxRollSpeed * framelag;
	  }
	  if ( mouse_dx != 0.0F )
	  {
		ctrl->roll -= RollAccell * MaxRollSpeed* mouse_dx;
	  }
	}
	else
	{
	  if ( key_held( &conf->left ) )
	  {
		ctrl->yaw  -= TurnAccell * MaxTurnSpeed * framelag;
		ctrl->bank += BankAccell * MaxBankAngle * framelag;
	  }
	  if ( key_held( &conf->right ) )
	  {
		ctrl->yaw  += TurnAccell * MaxTurnSpeed * framelag;
		ctrl->bank -= BankAccell * MaxBankAngle * framelag;
	  }
	  if ( mouse_dx != 0.0F )
	  {
		ctrl->yaw  += TurnAccell * MaxTurnSpeed * mouse_dx * turn_sign;
		ctrl->bank -= BankAccell * MaxBankAngle * mouse_dx * turn_sign;
	  }
	}

	if ( key_held( &conf->down ) )
	{
	  ctrl->pitch -= TurnAccell * MaxTurnSpeed * framelag;
	}
	if ( key_held( &conf->up ) )
	{
	  ctrl->pitch += TurnAccell * MaxTurnSpeed * framelag;
	}
	if ( mouse_dy != 0.0F )
	{
	  ctrl->pitch += TurnAccell * MaxTurnSpeed * pitch_sign * mouse_dy ;
	}
  }

  if ( key_held( &conf->move_left ) )
  {
	ctrl->right -= MoveAccell * MaxMoveSpeed * framelag;
  }
  if ( key_held( &conf->move_right ) )
  {
	ctrl->right += MoveAccell * MaxMoveSpeed * framelag;
  }

  if ( key_held( &conf->move_down ) )
  {
	ctrl->up -= MoveAccell * MaxMoveSpeed * framelag;
  }
  if ( key_held( &conf->move_up ) )
  {
	ctrl->up += MoveAccell * MaxMoveSpeed * framelag;
  }

  if ( key_held( &conf->roll_left ) )
  {
	ctrl->roll += RollAccell * MaxRollSpeed * framelag;
  }
  if ( key_held( &conf->roll_right ) )
  {
	ctrl->roll -= RollAccell * MaxRollSpeed * framelag;
  }

  if( NitroFuel && ( key_pressed ( &conf->turbo ) ) )
  {
	PlaySfx( SFX_NitroStart, 0.66F );
  }

  ctrl->turbo = key_held( &conf->turbo );
  if ( ctrl->turbo )
  {
	if ( NitroFuel > 0.0F )
	{
	  ctrl->forward += TurboAccell * MaxTurboSpeed * framelag;
	}
	else
	{
	  ctrl->forward += MoveAccell * MaxMoveSpeed * framelag;
	}
  }
  else if ( key_held( &conf->move_forward ) )
  {
	ctrl->forward += MoveAccell * MaxMoveSpeed * framelag;
  }
  if ( key_held( &conf->move_backward ) )
  {
	ctrl->forward -= MoveAccell * MaxMoveSpeed * framelag;
  }

  if ( key_pressed( &conf->cruise_faster ) )
	ctrl->cruise_control++;
  if ( key_pressed( &conf->cruise_slower ) )
	ctrl->cruise_control--;

  ctrl->fire_primary      = key_held( &conf->fire_primary );
  ctrl->fire_secondary    = key_held( &conf->fire_secondary );
  ctrl->fire_mine       = key_pressed( &conf->fire_mine );

  ctrl->select_next_primary = key_pressed( &conf->select_next_primary );
  ctrl->select_prev_primary = key_pressed( &conf->select_prev_primary );

  ctrl->select_next_secondary = key_pressed( &conf->select_next_secondary );
  ctrl->select_prev_secondary = key_pressed( &conf->select_prev_secondary );

  ctrl->drop_primary      = key_pressed( &conf->drop_primary );
  ctrl->drop_secondary    = key_pressed( &conf->drop_secondary );
  ctrl->drop_shield     = key_pressed( &conf->drop_shield );
  ctrl->drop_ammo       = key_pressed( &conf->drop_ammo );

  ctrl->select_primary = 0;
  for ( j = 0; j < MAX_PRIMARY_WEAPONS; j++ )
  {
	if ( key_pressed( &conf->select_primary[ j ] ) )
	{
	  ctrl->select_primary = j + 1;
	}
  }
  ctrl->select_secondary = 0;
  for ( j = 0; j < MAX_SECONDARY_WEAPONS; j++ )
  {
	if ( key_pressed( &conf->select_secondary[ j ] ) )
	{
	  ctrl->select_secondary = j + 1;
	}
  }

  if ( ( SDL_GetModState() & KMOD_CTRL ) && key_pressed( &conf->send_msg ) && MyGameStatus == STATUS_Normal )
  {
	MenuRestart( &MENU_QuickTextSendWhisper );
  }
  else if ( ( key_pressed( &conf->send_msg ) ) && MyGameStatus == STATUS_Normal )
  {
	MenuRestart( &MENU_QuickTextSend );
  }


	if ( JoystickInput )
	{
		for (joystick = 0; joystick < Num_Joysticks; joystick++)
		{
			if (JoystickInfo[joystick].connected && JoystickInfo[joystick].assigned)
				ReadJoystickInput(ctrl, joystick);
		}
	}


  MaxMove = MoveAccell * MaxMoveSpeed * framelag;
  MaxTurbo = TurboAccell * MaxTurboSpeed * framelag;
  MaxTurn = TurnAccell * MaxTurnSpeed * framelag;
  MaxRoll = RollAccell * MaxRollSpeed * framelag;
  MaxBank = BankAccell * MaxBankAngle * framelag;

  CLAMP( ctrl->pitch, MaxTurn );
  CLAMP( ctrl->yaw, MaxTurn );
  CLAMP( ctrl->roll, MaxRoll );
  CLAMP( ctrl->bank, MaxBank );
  CLAMP( ctrl->right, MaxMove );
  CLAMP( ctrl->up, MaxMove );

  if ( ctrl->turbo && NitroFuel > 0.0F )
  {
	if ( ctrl->forward > MaxTurbo )
	  ctrl->forward = MaxTurbo;
	else if ( ctrl->forward < -MaxMove )
	  ctrl->forward = -MaxMove;
  }
  else
  {
	CLAMP( ctrl->forward, MaxMove );
  }
}

int AnyKeyReleased( void )
{
  int k;

  if ( CurrentMenu )
	return 0;

  for ( k = 0; k < SDLK_LAST; k++ )
  {
	if ( KEY_RELEASED( k ) )
	  return 1;
  }

  for ( k = 0; k < MAX_MOUSE_BUTTONS; k++ )
  {
	if ( MOUSE_BUTTON_RELEASED( k ) )
	  return 1;
  }

  if( IsAnyJoystickButtonReleased() )
  {
	return 1;
  }

  return 0;
}

void DoShipAction( SHIPCONTROL *ctrl, int Action, float amount )
{
  switch (Action)
  {
  case SHIPACTION_RotateUp:
	if ( ctrl->slide_mode )
	{
	  ctrl->up += MoveAccell * MaxMoveSpeed * amount;
	}
	else
	{
	  ctrl->pitch += TurnAccell * MaxTurnSpeed * amount;
	}
	return;
  case SHIPACTION_RotateLeft:
	if ( ctrl->slide_mode )
	{
	  ctrl->right += MoveAccell * MaxMoveSpeed * amount;
	}
	else if ( ctrl->roll_mode )
	{
	  ctrl->roll -= RollAccell * MaxRollSpeed * amount;
	}
	else
	{
	  ctrl->yaw  += TurnAccell * MaxTurnSpeed * amount;
	  ctrl->bank -= BankAccell * MaxBankAngle * amount;
	}
	return;
  case SHIPACTION_RollLeft:
	ctrl->roll -= RollAccell * MaxRollSpeed * amount;
	return;
  case SHIPACTION_SlideUp:
	ctrl->up -= MoveAccell * MaxMoveSpeed * amount;
	return;
  case SHIPACTION_SlideLeft:
	ctrl->right += MoveAccell * MaxMoveSpeed * amount;
	return;
  case SHIPACTION_MoveForward:
	ctrl->forward -= MoveAccell * MaxMoveSpeed  * amount;
	return;
  }
}

// this function doesn't really do anything important needed to port

void SetUpJoystickAxis(int /*joystick*/)
{
}

bool joystick_poll( int joysticknum )
{
	int i;

	if(joysticknum >= Num_Joysticks)
		return false;

	// copy joystick button state
	for( i = 0; i < MAX_JOYSTICK_BUTTONS; i++ )
	{
		js_buttons[new_input][joysticknum][i] =
			joy_button_state[joysticknum][i];
	}

	// copy hat state
	for( i = 0; i < MAX_JOYSTICK_POVS; i++ )
	{
		int b;
		for( b = 0; b < MAX_POV_DIRECTIONS; b++ )
		{
			js_pov[new_input][joysticknum][i][b] =
				joy_hat_state[joysticknum][i][b];
		}
	}

	return true;
}


void ReadJoystickInput(SHIPCONTROL *ctrl, int joysticknum)
{
   int  ShipAction, axis;
   float amount;

   JOYSTICKAXIS *joyaxis;


	if( joysticknum >= Num_Joysticks )
		return;

	for (axis = 0; axis < MAX_JOYSTICK_AXIS; axis++)
	{

			amount = (float) joy_axis_state[ joysticknum ][ axis ];

		if ((JoystickInfo[joysticknum].Axis[axis].exists) && amount)
		{
			/* the axis were looking at */
			joyaxis = &JoystickInfo[ joysticknum ].Axis[ axis ];

			/* the action it performs */
			ShipAction = joyaxis->action;

			/* continue if the action is nothing */
			if ( ShipAction == SHIPACTION_Nothing )
				continue;

			/* amount of movement detected */
			amount = amount * joyaxis->sensitivity;

			/* if were using fine control */
			if ( joyaxis->fine )
				amount *= (float) fabsf( amount );

			/* if the axis is inverted */
			if ( joyaxis->inverted )
				amount  = -amount;

			/* perform the action */
			DoShipAction( ctrl, ShipAction, framelag * amount );
		}
	}
}


/*===================================================================
  Procedure : Check if any buttons of a specific joystick are
		: released
  Input   : int     Joystick Number
  Output    : bool    true/false
===================================================================*/
bool IsJoystickButtonReleased( int joysticknum )
{
  int i;

  if( joysticknum >= Num_Joysticks )
	  return false;

  for( i = 0; i < JoystickInfo[joysticknum].NumButtons; i++)
  {
	if ( JOYSTICK_BUTTON_RELEASED( joysticknum, i ) )
	{
	  return( true );
	}
  }

  return( false );
}

/*===================================================================
  Procedure : Check if any buttons of a any connected joystick
		: are released
  Input   : Nothing
  Output    : bool    true/false
===================================================================*/
bool IsAnyJoystickButtonReleased( void )
{
  int joystick;

  for (joystick = 0; joystick < Num_Joysticks; joystick++)
  {
	if( IsJoystickButtonReleased( joystick ) )
	{
	  return( true );
	}
  }
  return( false );
}
