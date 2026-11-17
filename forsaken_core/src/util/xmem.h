#ifndef XMEM_INCLUDED
#define XMEM_INCLUDED

/*===================================================================
	xmem -- heap accounting and the DEBUG_ON allocation tracker

	MemUsed is the running total of tracked heap bytes; it is defined
	in every build (oct2.cpp's ShowInfo overlay prints it). With
	DEBUG_ON, malloc/calloc/realloc/free/strdup are redirected through
	X_* so every block records its allocation site for leak reports.
	xmem.cpp itself defines XMEM_INCLUDED before including anything,
	so the redirect never applies to the tracker's own allocations.
===================================================================*/

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
#define realloc( P, S )	X_realloc( (P), (S), __FILE__, __LINE__ )
#define free( P )		X_free( (P), __FILE__, __LINE__ )
#define strdup( P )		X_strdup( (P), __FILE__, __LINE__ )


void XMem_Init( void );
void * X_calloc( size_t num,size_t size, const char *in_file, int in_line );
void * X_malloc( size_t size, const char *in_file, int in_line );
void X_free( void * Pnt, const char *in_file, int in_line );
void * X_realloc( void * Pnt , size_t size, const char *in_file, int in_line );
void * X_strdup( const char * str , const char *in_file, int in_line );
int UnMallocedBlocks( void );


#endif	/* DEBUG_ON */

#endif	// XMEM_INCLUDED
