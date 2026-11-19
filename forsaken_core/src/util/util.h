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
 * or the size its owner documents for a pointer. copy_field is copy_string
 * for a fixed-width record field (a network message or file record): it
 * also zeroes the bytes after the string, so the record carries no
 * leftover data. format_display_text is snprintf for text that is only
 * shown on screen, where cutting it to fit is the intended result.
 */

#include "game/main.h"

void strtoupper(char *str);

int Msg( const char * fmt, ... );

void DebugPrintf_( const char * format, ... ); // no prefix timestamp
void DebugPrintf( const char * format, ... );  // prefix timestamp
void DebugState( const char * str );           // only prints if str changed
void DebugPuts( char * str );                  //

/* Dest (size bytes) = Src with its extension replaced by Ext (or Ext added). */
void Change_Ext( const char * Src, char * Dest, size_t size, const char * Ext );
void Get_Ext( u_int8_t * Src, u_int8_t * Dest );

char* convert_path( const char* _str );

char * convert_char( char from, char to, char* in );

bool copy_string( char * dst, size_t size, const char * src );
bool append_string( char * dst, size_t size, const char * src );
bool copy_field( char * dst, size_t size, const char * src );
[[gnu::format( printf, 3, 4 )]] void format_display_text( char * dst, size_t size, const char * fmt, ... );

/* Defined in util.cpp; shared with the modules that use it. */
extern bool DebugLog;
