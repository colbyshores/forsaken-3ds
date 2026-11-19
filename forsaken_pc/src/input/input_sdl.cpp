/* input_sdl.cpp -- input.h for the PC, on SDL2
 *
 * handle_events drains the SDL event queue once a frame. Key presses go to
 * input_buffer as SDLK codes (Shift restores capitals for text entry;
 * Shift+F12 toggles full screen); mouse buttons and the wheel as the
 * LEFT_MOUSE.. / UP_MOUSE / DOWN_MOUSE codes; joystick button and hat presses
 * as joystick key codes (DIK_JOYSTICK, JOYSTICK_POVDIR_KEYCODE). Held state
 * goes to mouse_state and joy_*_state; joystick axes are scaled from SDL's
 * +-32768 to the engine's +-100 and zeroed inside the axis deadzone. Window
 * focus grabs or releases the mouse (always grabbed in full screen, in a
 * window only during play with no menu open). SDL_QUIT asks for confirmation.
 * An SDL_USEREVENT runs the callback in data1 with data2, so timers can call
 * back on the main thread.
 *
 * read_mouse hands out the accumulated motion at most every 0.028 s and
 * repeats the last value in between: the mouse reports at about 125 Hz,
 * slower than the frame rate. joysticks_init opens every SDL joystick (up to
 * MAX_JOYSTICKS) and names its axes, buttons and hats.
 */

#include "game/main.h"
#include "input/input.h"
#include "util/util.h"
#include "rendering/render.h"
#include <SDL.h>
#include "game/oct2.h"
#include "net/networking.h"
#include "ui/title.h"

JOYSTICKINFO JoystickInfo[MAX_JOYSTICKS];

int Num_Joysticks = 0;

//////////////////////////////////////////////
// Generic Routines
//////////////////////////////////////////////

void input_grab( bool grab )
{
	// 1. always acquire and hide mouse if in fullscreen
	// 2. took this out cause a player asked... if there is issues add it back...
	// 3. don't remember who asked or why ? need to enable again.
	if( render_info.fullscreen )
	{
		SDL_SetWindowGrab( render_info.window, SDL_TRUE );
		SDL_ShowCursor( SDL_DISABLE );
		return;
	}
	// window mode

	SDL_SetWindowGrab( render_info.window, grab ? SDL_TRUE : SDL_FALSE );
	SDL_ShowCursor( grab ? SDL_DISABLE : SDL_ENABLE );

}

//////////////////////////////////////////////
// Buffered High Level Input Events
//////////////////////////////////////////////

void input_buffer_send( int code )
{
	if( input_buffer_count >= MAX_INPUT_BUFFER )
		return;
	input_buffer[ input_buffer_count++ ] = code;
}

void input_buffer_reset( void )
{
	input_buffer_count = 0;
}

int input_buffer_find( int code )
{
	int i;
	for ( i = 0 ; i < input_buffer_count; i++ )
		if( input_buffer[ i ] == code )
			return 1;
	return 0;
}

//////////////////////////////////////////////
// Window and Activate Events
//////////////////////////////////////////////

// Despite the name, app_resize does its work only while this is true, and
// title.cpp sets it true only for the length of a RenderModeSelect call,
// during which handle_events does not run. So resizing the (resizable)
// window only logs.
bool bIgnoreWM_SIZE = false;
void app_resize( SDL_WindowEvent * /*window*/ )
{
	DebugPrintf("Window size changed.\n");

	// TODO - need to save resize.w/h somewhere

	if (!bIgnoreWM_SIZE)
		return;

	// we should save the size
	// then startup at this size next time

	if( MyGameStatus == STATUS_Title )
	{
		LastMenu = CurrentMenu;
		VduClear();
	}

	if( MyGameStatus == STATUS_Title )
	{
		FadeHoloLight(HoloLightBrightness);
		DarkenRoom2(RoomDarkness);
		ProcessVduItems( CurrentMenu );
		InitialTexturesSet = false;
	}

	SetGamePrefs();
}

void app_active( SDL_WindowEvent * window )
{

// SDL2 window events carry the specific gained/lost state
	bool gained  = window -> event == SDL_WINDOWEVENT_FOCUS_GAINED;
	bool lost    = window -> event == SDL_WINDOWEVENT_FOCUS_LOST;
	bool resized = window -> event == SDL_WINDOWEVENT_RESIZED;
	bool exposed = window -> event == SDL_WINDOWEVENT_EXPOSED;

	if( lost )
		input_grab( false );

	if( gained )
	{
		// fullscreen always has exclusive inputs
		if( render_info.fullscreen )
		{
			input_grab(true);
		}

		// window mode
		else
		{
			// only grab inputs if we are playing and not in menu
			if( !CurrentMenu && MyGameStatus == STATUS_Normal )
				input_grab(true);
			else
				input_grab(false);
		}

		RenderModeReset();
	}

	if( resized )
		app_resize( window );

	if( exposed )
		render_flip(&render_info);
}

void app_quit( void )
{
	// in case the window isn't in the foreground
	// we'd end up with a huge black spot from being previously covered
	render_flip(&render_info);

	// release mouse so they can interact with message box
	input_grab( false );

	// ask them to confirm clossing
	if(Msg("Are you sure you want to exit?") )
	{
		// user wants to quit
		// let our code know we're quitting and not failing
		CleanUpAndPostQuit();
		return;
	}

	// let them click to get focus again
	if( ! render_info.fullscreen )
		input_grab( true );
}

//////////////////////////////////////////////
// Keyboard Events
//////////////////////////////////////////////

void app_keyboard( SDL_KeyboardEvent * key )
{
	if( key->type == SDL_KEYUP )
	{
		if( key->keysym.sym == SDLK_F12 && key->keysym.mod & KMOD_SHIFT )
		{
				MenuGoFullScreen( nullptr );
		}
		else if( key->keysym.sym == SDLK_RSHIFT || key->keysym.sym == SDLK_LSHIFT )
		{
				const u_int8_t *keystate = SDL_GetKeyboardState(nullptr);
				if ( keystate[SDL_SCANCODE_F12] )
					MenuGoFullScreen( nullptr );
		}
	}
	if( key->type == SDL_KEYDOWN )
	{
		/* SDL2 has no keysym.unicode (SDL1 field). SDLK_* values for
		 * printable keys match lowercase ASCII; restore shifted letters
		 * so text entry (pilot names, console) keeps capitals. */
		int code = (int) key->keysym.sym;
		if( ( key->keysym.mod & KMOD_SHIFT ) && code >= 'a' && code <= 'z' )
			code += 'A' - 'a';
		input_buffer_send( code );
	}
}

//////////////////////////////////////////////
// Mouse Events
//////////////////////////////////////////////

// mouse wheel button down/up are sent at same time
// so if we react to the up event then we undo the down event !
// so we must ignore up events and reset the value each game loop
// since a wheel event can never be held down this is ok...

void mouse_wheel_up( void )
{
	mouse_state.wheel = 1;
}

void mouse_wheel_down( void )
{
	mouse_state.wheel = -1;
}

void reset_mouse_wheel( void )
{
	mouse_state.wheel = 0;
}

void app_mouse_wheel( SDL_MouseWheelEvent * _event )
{
	if(_event->y > 0) { mouse_wheel_up();   input_buffer_send( UP_MOUSE   ); }
	if(_event->y < 0) { mouse_wheel_down(); input_buffer_send( DOWN_MOUSE ); }
}

void app_mouse_button( SDL_MouseButtonEvent * _event )
{
	// we index mouse starting at 0
	int button = _event->button - 1;

	// pass down mouse events for menu processing
	// note: wheel events are sent above in app_mouse_wheel for sdl2
	if(  _event->type == SDL_MOUSEBUTTONDOWN )
		input_buffer_send( button + LEFT_MOUSE );

	switch( _event->button )
	{

	//
	// Every other mouse button works like normal
	//
	default:
		// save the button state
		mouse_state.buttons[ button ] = ( _event->type == SDL_MOUSEBUTTONDOWN );
		break;
	}
}

void app_mouse_motion( SDL_MouseMotionEvent * motion )
{
	mouse_state.xrel += motion->xrel;
	mouse_state.yrel += motion->yrel;
}

mouse_state_t* read_mouse(void)
{
	static mouse_state_t state;
	// the mouse only updates at about 125fps (8ms)
	// today we mostly render faster then input events from the mouse
	// so the mouse ends up with 0's most of the time cause there is no pending update
	// the game was designed to repeat the last mouse value for 0.028 seconds
	// other wise mouse responsiveness drops off greatly
	static float framelagfix = 0.0f;
	framelagfix -= real_framelag;
	if(framelagfix <= 0.0f)
	{
		framelagfix = 0.028f;
	}
	else
	{
		return &state;
	}
	state.xrel = mouse_state.xrel;
	state.yrel = mouse_state.yrel;
	mouse_state.xrel = 0;
	mouse_state.yrel = 0;
	return &state;
}

//////////////////////////////////////////////
// Joystick Events
//////////////////////////////////////////////

// SDL joystick events carry the joystick's instance id, which is not its
// device index: ids keep counting up as devices are opened, so they drift
// from the JoystickInfo slot after a failed open or a re-init. Map the id
// to the slot that holds that joystick; -1 if none does.
static int joystick_slot( SDL_JoystickID which )
{
	int i;
	for( i = 0; i < Num_Joysticks; i++ )
		if( JoystickInfo[i].sdl_joy &&
			SDL_JoystickInstanceID( JoystickInfo[i].sdl_joy ) == which )
			return i;
	return -1;
}

static int get_deadzone( int joy, int axis )
{
	// make sure this is a valid joystick/axis
	if( joy >= Num_Joysticks || axis >= JoystickInfo[joy].NumAxis )
		return 0;
	return JoystickInfo[ joy ].Axis[ axis ].deadzone;
}

void app_joy_axis( SDL_JoyAxisEvent * axis )
{
	long value;
	int deadzone;

	if(axis->axis >= MAX_JOYSTICK_AXIS)
	{
		DebugPrintf(
			"sdl_input: ignoring joy %d axis %d >= max axises\n",
			axis->which, axis->axis);
		return;
	}

	int joy = joystick_slot( axis->which );
	if( joy < 0 )
		return;

	// sdl axis value (range: -32768 to 32767)
	// forsaken expects (range: -100 to 100)
	value = (long) (((float)axis->value) / 327.67f);

	// get the joystick deadzone
	deadzone = get_deadzone(joy,axis->axis);

	// if movement greater then deadzone then apply
	// other wise no movement at all is registered
	joy_axis_state[ joy ][ axis->axis ] =
		( abs(value) > deadzone ) ? value : 0 ;
}

void app_joy_ball( SDL_JoyBallEvent * /*ball*/ )
{
}

void app_joy_button( SDL_JoyButtonEvent * button )
{
	if(button->button >= MAX_JOYSTICK_BUTTONS)
	{
		DebugPrintf(
			"sdl_input: ignoring joy %d button %d >= max buttons\n",
			button->which, button->button);
		return;
	}

	int joy = joystick_slot( button->which );
	if( joy < 0 )
		return;

	joy_button_state[ joy ][ button->button ] =
		(button->type == SDL_JOYBUTTONDOWN);

	// pass down joystick button presses for menu processing
	if(  button->type == SDL_JOYBUTTONDOWN )
	{
		input_buffer_send(
			button->button + DIK_JOYSTICK
		);
	}
}

void app_joy_hat( SDL_JoyHatEvent * hat )
{
	if(hat->hat >= MAX_JOYSTICK_POVS)
	{
		DebugPrintf(
			"sdl_input: ignoring joy %d hat %d >= max hats\n",
			hat->which, hat->hat);
		return;
	}

	int joy = joystick_slot( hat->which );
	if( joy < 0 )
		return;

	if(hat->value == SDL_HAT_CENTERED)
	{
		int d;
		for( d = 0; d < MAX_POV_DIRECTIONS; d++ )
		{
			joy_hat_state[ joy ][ hat->hat ][ d ] = 0;
		}
		return;
	}

	if(hat->value & SDL_HAT_UP)
	{
		joy_hat_state[ joy ][ hat->hat ][ JOY_HAT_UP ] = 1;
		input_buffer_send(
			JOYSTICK_POVDIR_KEYCODE(
				joy,
				hat->hat,
				JOY_HAT_UP
			)
		);
	}
	else
	{
		joy_hat_state[ joy ][ hat->hat ][ JOY_HAT_UP ] = 0;
	}

	if(hat->value & SDL_HAT_RIGHT)
	{
		joy_hat_state[ joy ][ hat->hat ][ JOY_HAT_RIGHT ] = 1;
		input_buffer_send(
			JOYSTICK_POVDIR_KEYCODE(
				joy,
				hat->hat,
				JOY_HAT_RIGHT
			)
		);
	}
	else
	{
		joy_hat_state[ joy ][ hat->hat ][ JOY_HAT_RIGHT ] = 0;
	}

	if(hat->value & SDL_HAT_DOWN)
	{
		joy_hat_state[ joy ][ hat->hat ][ JOY_HAT_DOWN ] = 1;
		input_buffer_send(
			JOYSTICK_POVDIR_KEYCODE(
				joy,
				hat->hat,
				JOY_HAT_DOWN
			)
		);
	}
	else
	{
		joy_hat_state[ joy ][ hat->hat ][ JOY_HAT_DOWN ] = 0;
	}

	if(hat->value & SDL_HAT_LEFT)
	{
		joy_hat_state[ joy ][ hat->hat ][ JOY_HAT_LEFT ] = 1;
		input_buffer_send(
			JOYSTICK_POVDIR_KEYCODE(
				joy,
				hat->hat,
				JOY_HAT_LEFT
			)
		);
	}
	else
	{
		joy_hat_state[ joy ][ hat->hat ][ JOY_HAT_LEFT ] = 0;
	}
}

bool joysticks_init(void)
{
	int i, j, k;

	// initial memory cleaning
	ZERO_STACK_MEM(JoystickInfo);

	joysticks_cleanup();

	Num_Joysticks = SDL_NumJoysticks();

	DebugPrintf( "joysticks_init: %d joysticks connected\n", Num_Joysticks );

	if (Num_Joysticks > MAX_JOYSTICKS)
		Num_Joysticks = MAX_JOYSTICKS;

	for (i = 0; i < Num_Joysticks; i++)
	{
		SDL_Joystick * joy = SDL_JoystickOpen(i);

		// failed to open joystick
		if(!joy)
		{
			DebugPrintf(
				"joysticks_init: joystick (%d), '%s' failed to open\n",
				i, SDL_JoystickNameForIndex(i)
			);
			continue;
		}

		// setup defaults
		JoystickInfo[i].sdl_joy 	= joy;
		JoystickInfo[i].assigned	= false;
		JoystickInfo[i].connected	= true;
		JoystickInfo[i].NumAxis		= SDL_JoystickNumAxes(joy);
		JoystickInfo[i].NumButtons	= SDL_JoystickNumButtons(joy);
		JoystickInfo[i].NumPOVs		= SDL_JoystickNumHats(joy);

		// use only as many as JOYSTICKINFO has room for
		if (JoystickInfo[i].NumAxis > MAX_JOYSTICK_AXIS)
			JoystickInfo[i].NumAxis = MAX_JOYSTICK_AXIS;
		if (JoystickInfo[i].NumButtons > MAX_JOYSTICK_BUTTONS)
			JoystickInfo[i].NumButtons = MAX_JOYSTICK_BUTTONS;
		if (JoystickInfo[i].NumPOVs > MAX_JOYSTICK_POVS)
			JoystickInfo[i].NumPOVs = MAX_JOYSTICK_POVS;

		JoystickInfo[i].Name = (char *)strdup( SDL_JoystickNameForIndex(i) );

		DebugPrintf(
			"joysticks_init: joystick (%d), name='%s', axises=%d, buttons=%d, hats=%d\n",
			i, JoystickInfo[i].Name, JoystickInfo[i].NumAxis, JoystickInfo[i].NumButtons,
			JoystickInfo[i].NumPOVs
		);

		for (j = 0; j < JoystickInfo[i].NumAxis; j++)
		{
			JoystickInfo[i].Axis[j].action		= SHIPACTION_Nothing;
			JoystickInfo[i].Axis[j].inverted	= false;
			JoystickInfo[i].Axis[j].deadzone	= 20;
			JoystickInfo[i].Axis[j].fine		= true;
			JoystickInfo[i].Axis[j].exists		= true;

			JoystickInfo[i].Axis[j].name =
				(char*) malloc (MAX_JOYNAME+1);

			sprintf(
				JoystickInfo[i].Axis[j].name,
				"Axis %d",
				j
			);
		}

		for( j = 0; j < JoystickInfo[i].NumButtons; j++ )
		{
			JoystickInfo[i].Button[j].action = SHIPACTION_Nothing;

			JoystickInfo[i].Button[j].name =
				(char *) malloc (MAX_JOYNAME+1);

			sprintf(
				JoystickInfo[i].Button[j].name,
				"Button %d",
				j
			);
		}

		for (j = 0; j < JoystickInfo[i].NumPOVs; j++)
		{
			JoystickInfo[i].POV[j].name =
				(char *) malloc (MAX_JOYNAME+1);

			sprintf(
				JoystickInfo[i].POV[j].name,
				"Hat %d",
				j
			);

			for (k = 0; k < MAX_POV_DIRECTIONS; k++)
			{
				JoystickInfo[i].POV[j].action[k] =
					SHIPACTION_Nothing;

				 JoystickInfo[i].POV[j].dirname[k] =
					(char *) malloc (MAX_JOYNAME+1);

				sprintf(
					JoystickInfo[i].POV[j].dirname[k],
					"%s %d",
					JoystickInfo[i].POV[j].name,
					k
				);
			}
		}
	}

	return true;
}

bool joysticks_cleanup( void )
{
	int i, j, k;
	for (i = 0; i < MAX_JOYSTICKS; i++)
	{
		DebugPrintf("cleaning up joystick: %s %d\n",
			JoystickInfo[i].Name, i);

		JoystickInfo[i].assigned = false;
		JoystickInfo[i].connected = false;

		if(JoystickInfo[i].Name)
		{
			free(JoystickInfo[i].Name);
		}
		JoystickInfo[i].Name = nullptr;

		if(JoystickInfo[i].sdl_joy)
		{
			SDL_JoystickClose(
				JoystickInfo[i].sdl_joy
			);
		}
		JoystickInfo[i].sdl_joy = nullptr;

		for (j = 0; j < JoystickInfo[i].NumButtons; j++)
		{
			if(JoystickInfo[i].Button[j].name)
			{
				free(JoystickInfo[i].Button[j].name);
			}
			JoystickInfo[i].Button[j].name = nullptr;
			JoystickInfo[i].Button[j].action = SHIPACTION_Nothing;
		}

		for (j = 0; j < JoystickInfo[i].NumPOVs; j++)
		{
			if(JoystickInfo[i].POV[j].name)
			{
				free(JoystickInfo[i].POV[j].name);
			}
			JoystickInfo[i].POV[j].name = nullptr;
			for ( k = 0; k < MAX_POV_DIRECTIONS; k++ )
			{
				if ( JoystickInfo[ i ].POV[ j ].dirname[ k ] )
				{
					free( JoystickInfo[ i ].POV[ j ].dirname[ k ] );
				}
				JoystickInfo[ i ].POV[ j ].dirname[ k ] = nullptr;
				JoystickInfo[ i ].POV[ j ].action[ k ] = SHIPACTION_Nothing;
			}
		}

		for (j = 0; j < MAX_JOYSTICK_AXIS; j++)
		{
			if(JoystickInfo[i].Axis[j].name)
			{
				free(JoystickInfo[i].Axis[j].name);
			}
			JoystickInfo[i].Axis[j].name = nullptr;
			JoystickInfo[i].Axis[j].exists = false;
			JoystickInfo[i].Axis[j].action = SHIPACTION_Nothing;
			JoystickInfo[i].Axis[j].sensitivity = 0.0f;
			JoystickInfo[i].Axis[j].deadzone = 0;
			JoystickInfo[i].Axis[j].inverted = false;
			JoystickInfo[i].Axis[j].fine = false;
		}

		// the counts bound the frees above, so clear them last
		JoystickInfo[i].NumButtons = 0;
		JoystickInfo[i].NumPOVs = 0;
		JoystickInfo[i].NumAxis = 0;
	}
	return true;
}


//////////////////////////////////////////////
// Main Events
//////////////////////////////////////////////

void reset_events( void )
{
	reset_mouse_wheel();
	input_buffer_reset();
}

bool handle_events( void )
{
	SDL_Event _event;

	reset_events();

	while( SDL_PollEvent( &_event ) )
	{
		switch( _event.type )
		{


// mouse wheel is an x/y axis event in SDL2 (was a button code in SDL1)
		case SDL_MOUSEWHEEL:
			app_mouse_wheel( &_event.wheel );
			break;


		case SDL_WINDOWEVENT:
			app_active( &_event.window );
			break;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			app_keyboard( &_event.key );
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			app_mouse_button( &_event.button );
			break;

		case SDL_MOUSEMOTION:
			app_mouse_motion( &_event.motion );
			break;

		case SDL_JOYAXISMOTION:
			app_joy_axis( &_event.jaxis );
			break;

		case SDL_JOYBALLMOTION:
			app_joy_ball( &_event.jball );
			break;

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			app_joy_button( &_event.jbutton );
			break;

		case SDL_JOYHATMOTION:
			app_joy_hat( &_event.jhat );
			break;


		case SDL_QUIT:
			app_quit();
			break;

		// platform specific _event type
		// must be enabled using SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE)
		case SDL_SYSWMEVENT:
			DebugPrintf("recived a platform specific _event type\n");
			break;

		// to avoid threading issues timers will add an event to the queue
		// so that we can call the callbacks from the same thread
		case SDL_USEREVENT:
			{
				void (*p) (void*) = (void (*)(void*))_event.user.data1; // callback
				p(_event.user.data2); // callback( data )
			}
			break;

// TODO
		default:
			printf("Unknown event type: %d\n",_event.type);
			break;
		}
	}

	if ( quitting )
	{
		DebugPrintf("about to CleanUpAndPostQuit ( from WindowProc )\n");
		quitting = false;
		CleanUpAndPostQuit();
	}

	return true;
}

