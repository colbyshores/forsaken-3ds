#pragma once

/* util.h -- logging and file-name helpers
 *
 * Msg() only logs (no dialog) and always reports OK. DebugPrintf writes to
 * stderr only when Debug is set, and also to a timestamped file under Logs/
 * when DebugLog is set. convert_path() lower-cases and converts backslashes
 * into a static buffer that the next call overwrites.
 *
 * copy_string and append_string are the bounded strcpy and strcat: dst
 * holds size bytes and is always left terminated; they return false when
 * src did not fit and was cut short. Pass sizeof the destination array,
 * or the size its owner documents for a pointer.
 */

#include "game/main.h"

void strtoupper(char *str);

int Msg( const char * fmt, ... );

void DebugPrintf_( const char * format, ... ); // no prefix timestamp
void DebugPrintf( const char * format, ... );  // prefix timestamp
void DebugState( const char * str );           // only prints if str changed
void DebugPuts( char * str );                  //

void Change_Ext( const char * Src, char * Dest, const char * Ext );
void Get_Ext( u_int8_t * Src, u_int8_t * Dest );

char* convert_path( const char* _str );

char * convert_char( char from, char to, char* in );

bool copy_string( char * dst, size_t size, const char * src );
bool append_string( char * dst, size_t size, const char * src );

/* Defined in util.cpp; shared with the modules that use it. */
extern bool DebugLog;
