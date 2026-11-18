#pragma once


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

/* Byte-wise copy GCC cannot fuse into ldm/stm: safe for float-bearing
 * structs read out of byte-parsed file buffers on ARM11. */
void memcpy_unaligned(void *dst, const void *src, size_t n);
char * convert_char( char from, char to, char* in );

/* Defined in util.cpp; shared with the modules that use it. */
extern bool DebugLog;
