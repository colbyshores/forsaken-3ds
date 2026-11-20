#ifndef XMEM_INCLUDED
#define XMEM_INCLUDED

/* xmem.h -- heap byte counter and the DEBUG_ON allocation tracker

	XMem.bytes_in_use() is the running total of tracked heap bytes. The
	counter exists in every build (oct2.cpp's ShowInfo overlay prints it)
	but only the X_* functions change it, so without DEBUG_ON it stays 0.

	game/main.h includes this header everywhere. With DEBUG_ON, malloc /
	calloc / free / strdup are redirected through X_*, which record each
	block's size and allocation site in a fixed table of 16384 slots;
	main.cpp calls XMem_Init at startup and UnMallocedBlocks at exit to
	list blocks never freed. X_free of a pointer not in the table only
	logs it and does not free it. realloc is #undef'd but not redirected
	(nothing calls it today); a realloc'd tracked block would leave a
	stale entry. xmem.cpp defines XMEM_NO_REDIRECT and includes this
	header before anything else, so the redirect never applies to the
	tracker's own calls.
*/

#include <stddef.h>

/* Heap accounting: the tracked-byte counter, and the DEBUG_ON block
 * tracker behind the X_* redirects (one slot per live X_ allocation,
 * recording size and allocation site for leak reports). The X_* API is
 * the only writer. */
class XMemTracker {
	public:
	static constexpr int MAXBLOCKS = 16384;

	// tracked heap bytes handed out and not yet freed (0 without DEBUG_ON)
	size_t bytes_in_use() const { return MemUsed; }

	void init();
	int find_free();
	int find_same(void *p) const;

	private:
	// the X_* API in xmem.cpp records and releases blocks
	friend void XMem_Init( void );
	friend char * X_strdup( const char * str , const char *in_file, int in_line );
	friend void * X_malloc( size_t size, const char *in_file, int in_line );
	friend void * X_calloc( size_t num,size_t size, const char *in_file, int in_line );
	friend void X_free( void * Pnt, const char *in_file, int in_line );
	friend int UnMallocedBlocks( void );

	bool   used[MAXBLOCKS] = {};
	void  *pnts[MAXBLOCKS] = {};
	size_t sizes[MAXBLOCKS] = {};
	const char *files[MAXBLOCKS] = {};	/* __FILE__ of each live block */
	int    lines[MAXBLOCKS] = {};
	int    blocks_used = 0;
	// Static, so it stays its own 4-byte symbol as the old MemUsed global
	// was: release links read only this counter (ShowInfo), and section GC
	// can then drop the 278 KB tracker nothing else references.
	static size_t MemUsed;
};

extern XMemTracker XMem;


#if defined( DEBUG_ON ) && !defined( XMEM_NO_REDIRECT )

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
char * X_strdup( const char * str , const char *in_file, int in_line );
int UnMallocedBlocks( void );


#endif	/* DEBUG_ON && !XMEM_NO_REDIRECT */

#endif	// XMEM_INCLUDED
