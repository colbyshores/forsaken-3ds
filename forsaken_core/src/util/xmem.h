#ifndef XMEM_INCLUDED
#define XMEM_INCLUDED

/* xmem.h -- heap byte counter and the DEBUG_ON allocation tracker

	MemUsed is the running total of tracked heap bytes. It is defined in
	every build (oct2.cpp's ShowInfo overlay prints it) but only the X_*
	functions change it, so without DEBUG_ON it stays 0.

	game/main.h includes this header everywhere. With DEBUG_ON, malloc /
	calloc / free / strdup are redirected through X_*, which record each
	block's size and allocation site in a fixed table of 16384 slots;
	main.cpp calls XMem_Init at startup and UnMallocedBlocks at exit to
	list blocks never freed. X_free of a pointer not in the table only
	logs it and does not free it. realloc is #undef'd but not redirected
	(nothing calls it today); a realloc'd tracked block would leave a
	stale entry. xmem.cpp defines XMEM_INCLUDED before including
	anything, so the redirect never applies to the tracker's own calls.
*/

#include <stddef.h>


extern size_t MemUsed;


#ifdef DEBUG_ON

#include <stdio.h>

#undef malloc
#undef calloc
#undef realloc
#undef free

#define malloc( S )		X_malloc( (S), __FILE__, __LINE__ )
#define calloc( N, S )	X_calloc( (N), (S), __FILE__, __LINE__ )
#define free( P )		X_free( (P), __FILE__, __LINE__ )
#define strdup( P )		X_strdup( (P), __FILE__, __LINE__ )


void XMem_Init( void );
void * X_calloc( size_t num,size_t size, const char *in_file, int in_line );
void * X_malloc( size_t size, const char *in_file, int in_line );
void X_free( void * Pnt, const char *in_file, int in_line );
void * X_strdup( const char * str , const char *in_file, int in_line );
int UnMallocedBlocks( void );


#endif	/* DEBUG_ON */

#endif	// XMEM_INCLUDED
