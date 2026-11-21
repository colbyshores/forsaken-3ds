#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include "game/main.h"
#include "data/file.h"
#include "util/util.h"
#include <ctype.h>
#include "string.h"
#include "ui/title.h"
#include "input/input.h"
#include "game/debugview.h"

char * convert_char( char from, char to, char* in )
{
	char * pch;
	if(!in)
		return in;
	pch = strchr( in, from );
	while( pch != nullptr )
	{
		*pch = to;
		pch = strchr( pch+1, from );
	}
	return in;
}

char* convert_path( const char* _str )
{
	static char temp[500];
	char * str = temp;
	copy_string( temp, sizeof( temp ), _str );
	while (*str)
	{
		if (*str == '\\')
			*str = '/';
		else
			*str = (char) tolower(*str);
		str++;
	}
	return temp;
}

void strtoupper(char *str)
{
	while (*str)
	{
		*str = (char) toupper(*str);
		str++;
	}
}

void Get_Ext( u_int8_t * Src, u_int8_t * Dest )
{
	u_int8_t	*	Char_Ptr;

	Char_Ptr = ( Src + strlen( (const char *)Src ) ) -1;

	while( Char_Ptr != Src && *Char_Ptr != '\\' && *Char_Ptr != ':' && *Char_Ptr != '.' ) Char_Ptr--;

	if( *Char_Ptr == '.' )
	{
		Char_Ptr++;
		while( *Char_Ptr ) *Dest++ = *Char_Ptr++;
		*Dest = 0;
	}
	else
	{
		*Dest = 0;
	}
}

void Change_Ext( const char * Src, char * Dest, size_t size, const char * Ext )
{
	size_t length = strlen( Src );
	const char * p;
	size_t keep;

	if ( ! length )
	{
		copy_string( Dest, size, Ext ); // set the extension
		Msg("Change_Ext called with Src empty!");
		return;
	}

	// the extension starts at the last '.' after the last '\\' or ':'
	p = Src + length - 1;
	while( p != Src && *p != '\\' && *p != ':' && *p != '.' )
		p--;
	keep = ( *p == '.' ) ? (size_t)( p - Src ) : length;

	if( size == 0 )
		return;
	if( keep >= size )
		keep = size - 1;
	memcpy( Dest, Src, keep );
	Dest[ keep ] = 0;
	append_string( Dest, size, Ext );
}

void DebugPrintf( const char * format, ... ) // timestamp prefix
{
	static char buf[0x4000];
	char *buf2;
	int buf_length;
	va_list args;

	if(!DebugViews.Debug)
		return;

	struct timeval now;
	gettimeofday(&now, nullptr);
	snprintf( buf, sizeof( buf ), "%lld.%.3d ",
		(long long) now.tv_sec, (int)( now.tv_usec / 1000 ));

	buf2 = strchr(buf,0);
	buf_length = sizeof(buf)-strlen(buf);

	va_start( args, format );
	vsnprintf( buf2, buf_length, format, args );
	va_end( args );

	buf[sizeof(buf)-1]=0; // terminate safely with null byte

	DebugPuts(buf);
}

void DebugPrintf_( const char * format, ... ) // no timestamp prefix
{
	static char buf[0x4000];
	va_list args;

	if(!DebugViews.Debug)
		return;

	va_start( args, format );
	vsnprintf( buf, sizeof(buf), format, args );
	va_end( args );

	buf[sizeof(buf)-1]=0; // terminate safely with null byte

	DebugPuts(buf);
}

void DebugPuts( char * buf )
{
	static FILE * logfile_fp;

	if(!DebugViews.Debug)
		return;

	fputs( buf, stderr );

	if( !DebugViews.DebugLog )
		return;

	if(!logfile_fp)
	{
		time_t now;
		struct tm *ts;
		char buf[80];
		now = time(nullptr);
		ts = localtime(&now);
		strftime(buf,sizeof(buf),"Logs\\%m-%d-%y %H.%M.%S.txt",ts);
		logfile_fp = file_open( buf, "w" );
		if(logfile_fp)
		{
			strftime(buf,sizeof(buf),"time zone = %z\n",ts);
			fputs( buf, logfile_fp );
		}
	}

	if( logfile_fp )
	{
		fputs( buf, logfile_fp );
		fflush( logfile_fp );
	}
}

// prints a message only if it wasn't the last one to be printed...
// this way you can debug game state without getting a message every single loop
// for instance in the title screen if you print "in title screen" it will only print once
// instead of every single loop....
void DebugState( const char * str )
{
	static const char * last;
	if ( !DebugViews.Debug )
		return;
	if ( last && strcmp( str, last ) == 0 )
		return;
	last = str;
	DebugPrintf( str );
}

//
// Creates a message box and returns ok/cancel press
//

int Msg( const char * msg, ... )
{

	char txt[ 1024 ];
	va_list args;
	int res = 1; // return an ok

	va_start( args, msg );
	vsnprintf( txt, sizeof(txt), msg, args);
	va_end( args );

	txt[sizeof(txt)-1] = 0;


	DebugPrintf("Msg: %s\n",txt);

	// IDCANCEL	Cancel button was selected.
	// IDOK	OK button was selected.
	return res;
}

bool copy_string( char * dst, size_t size, const char * src )
{
	size_t len = strlen( src );
	size_t n;

	if( size == 0 )
		return false;
	n = ( len < size ) ? len : size - 1;
	memcpy( dst, src, n );
	dst[ n ] = 0;
	return n == len;
}

bool append_string( char * dst, size_t size, const char * src )
{
	size_t used = strnlen( dst, size );

	if( used >= size )
		return false;	// dst is not terminated within size
	return copy_string( dst + used, size - used, src );
}

bool copy_field( char * dst, size_t size, const char * src )
{
	bool fits = copy_string( dst, size, src );
	size_t used = strnlen( dst, size ) + 1;

	if( used < size )
		memset( dst + used, 0, size - used );
	return fits;
}

void format_display_text( char * dst, size_t size, const char * fmt, ... )
{
	va_list args;

	va_start( args, fmt );
	vsnprintf( dst, size, fmt, args );
	va_end( args );
}
