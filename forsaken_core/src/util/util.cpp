#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/timeb.h>
#include "game/main.h"
#include "data/file.h"
#include "util/util.h"
#include <ctype.h>
#include "string.h"
#include "ui/title.h"
#include "input/input.h"

bool DebugLog = false;

char * convert_char( char from, char to, char* in )
{
	char * pch;
	if(!in)
		return in;
	pch = strchr( in, from );
	while( pch != NULL )
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
	strncpy( temp, _str, sizeof(temp) );
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

void Change_Ext( const char * Src, char * Dest, const char * Ext )
{
	u_int8_t	*	Char_Ptr;

	int length = strlen( Src );

	if ( ! length )
	{
		strcpy( Dest, Ext ); // set the extension
		Msg("Change_Ext called with Src empty!");
		return;
	}

	Char_Ptr = ( (u_int8_t*)Src + length ) - 1;

	while( Char_Ptr != (u_int8_t*)Src && *Char_Ptr != '\\' && *Char_Ptr != ':' && *Char_Ptr != '.' ) Char_Ptr--;

	if( *Char_Ptr == '.' )
	{
		while( (u_int8_t*)Src != Char_Ptr )
			*Dest++ = *Src++;
		strcpy( Dest, Ext );
	}
	else
	{
		strcpy( (char *)Dest, (const char *)Src );
		Dest += strlen( Src );
		strcpy( Dest, Ext );
	}
}

void DebugPrintf( const char * format, ... ) // timestamp prefix
{
	static char buf[0x4000];
	char *buf2;
	int buf_length;
	va_list args;

	if(!Debug)
		return;

	struct timeb now;
	ftime(&now);
	sprintf( buf, "%lld.%.3d ",
		(long long) now.time, now.millitm);

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

	if(!Debug)
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

	if(!Debug)
		return;

	fputs( buf, stderr );

	if( !DebugLog )
		return;

	if(!logfile_fp)
	{
		time_t now;
		struct tm *ts;
		char buf[80];
		now = time(NULL);
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
	if ( !Debug ) 
		return;
	if ( last && strcmp( str, last ) == 0 )
		return;
	last = str;
	DebugPrintf( str );
}

//
// Creates a message box and returns ok/cancel press
//

// temporarily jumps to desktop mode
//#include "render.h"
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


#ifndef __3DS__
/* memcpy_unaligned: the 3DS build gets it from main_3ds.c; elsewhere a
 * plain byte copy is alignment-safe on every target. */
void memcpy_unaligned(void *dst, const void *src, size_t n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	while (n--) *d++ = *s++;
}
#endif
