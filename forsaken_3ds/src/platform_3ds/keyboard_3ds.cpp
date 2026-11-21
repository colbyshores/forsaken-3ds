/* keyboard_3ds.cpp -- the system keyboard for menu text fields
 *
 * The 3DS has no keyboard, so a menu text field (the pilot name, a macro,
 * an address) asks for the system keyboard applet instead. The applet takes
 * over both screens and the GPU, so it must not open inside a citro3d frame,
 * and the menu code that asks for it runs mid-frame. So
 * platform_text_input_begin only records the request; HidPoller::poll calls
 * keyboard_3ds_service at the start of the next main-loop pass, before
 * anything is drawn, and that opens the keyboard. The menu picks the result
 * up with platform_text_input_result and finishes the field through its
 * usual RETURN / ESCAPE handling.
 *
 * Every field takes printable ASCII only (the game's font has nothing else).
 * A field that becomes a file name (the pilot name: pilots\NAME.txt) also
 * refuses what the menu's filename filter would drop (\ / : * ? " < > |).
 * The keyboard says so and stays open, instead of the characters vanishing
 * after it closes.
 */
#include <3ds.h>
#include <string.h>

#include "util/platform.h"
#include "platform_3ds/keyboard_3ds.h"

namespace {

/* One keyboard request at a time: requested by the menu, opened by the poll,
 * collected by the menu. */
class SystemKeyboard {
	public:
	void request( const char *initial, int max_len, bool file_name );
	bool take( char *out, size_t out_size, bool *accepted );
	bool service();

	private:
	static SwkbdCallbackResult filter( void *user, const char **message, const char *text, size_t len );

	char initial_text[ 256 ] = {};
	char result_text[ 256 ] = {};
	int  limit = 0;
	bool file_name = false;
	bool requested = false;
	bool finished = false;
	bool was_accepted = false;
};

SystemKeyboard g_keyboard;

void SystemKeyboard::request( const char *initial, int max_len, bool name_is_file )
{
	size_t n = strlen( initial );
	if ( n >= sizeof( initial_text ) )
		n = sizeof( initial_text ) - 1;
	memcpy( initial_text, initial, n );
	initial_text[ n ] = 0;
	limit = ( max_len > 0 && max_len < (int)sizeof( result_text ) ) ? max_len : (int)sizeof( result_text ) - 1;
	file_name = name_is_file;
	requested = true;
	finished = false;
}

bool SystemKeyboard::take( char *out, size_t out_size, bool *accepted )
{
	if ( !finished || !out_size )
		return false;
	finished = false;
	size_t n = strlen( result_text );
	if ( n >= out_size )
		n = out_size - 1;
	memcpy( out, result_text, n );
	out[ n ] = 0;
	*accepted = was_accepted;
	return true;
}

SwkbdCallbackResult SystemKeyboard::filter( void *user, const char **message, const char *text, size_t len )
{
	bool name_is_file = *(const bool *)user;
	for ( size_t i = 0; i < len; i++ )
	{
		unsigned char c = (unsigned char)text[ i ];
		if ( c < 0x20 || c > 0x7E )
		{
			*message = "Accented letters and symbols\noutside plain ASCII can't be shown.";
			return SWKBD_CALLBACK_CONTINUE;
		}
		if ( name_is_file && strchr( "\\/:*?\"<>|", c ) )
		{
			*message = "A name can't contain\n\\ / : * ? \" < > |";
			return SWKBD_CALLBACK_CONTINUE;
		}
	}
	return SWKBD_CALLBACK_OK;
}

/* Open the keyboard if one was requested. Blocks until the player closes it.
 * Returns true if it was shown. */
bool SystemKeyboard::service()
{
	if ( !requested )
		return false;
	requested = false;

	SwkbdState kb;
	swkbdInit( &kb, SWKBD_TYPE_WESTERN, 2, limit );
	swkbdSetInitialText( &kb, initial_text );
	swkbdSetValidation( &kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0 );
	swkbdSetFilterCallback( &kb, filter, &file_name );

	char buf[ sizeof( result_text ) ] = {};
	SwkbdButton button = swkbdInputText( &kb, buf, sizeof( buf ) );

	was_accepted = button == SWKBD_BUTTON_CONFIRM;
	memcpy( result_text, buf, sizeof( result_text ) );
	result_text[ sizeof( result_text ) - 1 ] = 0;
	finished = true;
	return true;
}

}

void platform_text_input_begin( const char *initial, int max_len, bool file_name ) { g_keyboard.request( initial, max_len, file_name ); }
bool platform_text_input_result( char *out, size_t out_size, bool *accepted ) { return g_keyboard.take( out, out_size, accepted ); }
bool keyboard_3ds_service( void ) { return g_keyboard.service(); }
