#ifndef UTIL_INCLUDED
#define UTIL_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void strtoupper(char *str);

int Msg( const char * fmt, ... );

void DebugPrintf_( const char * format, ... ); // no prefix timestamp
void DebugPrintf( const char * format, ... );  // prefix timestamp
void DebugState( const char * str );           // only prints if str changed
void DebugPuts( char * str );                  // 

void Change_Ext( const char * Src, char * Dest, const char * Ext );
void Get_Ext( u_int8_t * Src, u_int8_t * Dest );
void GetFilename( u_int8_t * Src, u_int8_t * Dest );

char* convert_path( const char* _str );

/* Shared helpers the 3DS build gets from main_3ds.c. memcpy_unaligned
 * copies byte-wise (ARM11 alignment); trace is a no-op in
 * production (the verbose implementation lives in the harness). */
void memcpy_unaligned(void *dst, const void *src, size_t n);
void trace(const char *msg);
void trace_dump(void);
void trace_enable(void);
char * convert_char( char from, char to, char* in );

#ifdef __cplusplus
};
#endif

#endif
