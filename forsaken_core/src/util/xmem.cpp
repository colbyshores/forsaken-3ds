/* xmem.cpp -- DEBUG_ON allocation tracker and MemUsed; see xmem.h. */
#define XMEM_INCLUDED // keep xmem.h (and its malloc/free redirects) out of this file
#include <stdio.h>
#include "game/main.h"
#include "util/util.h"

/* The block tracker below is state owned by this module (one instance,
 * reached through the X_* API that xmem.h's DEBUG_ON macros redirect to). */

size_t	MemUsed = 0;

namespace {

constexpr int MAXBLOCKS = 16384;

/* Debug block tracker: one slot per live X_ allocation, recording
 * size and allocation site for leak reports. */
struct XMemTracker {
	bool   used[MAXBLOCKS];
	void  *pnts[MAXBLOCKS];
	size_t sizes[MAXBLOCKS];
	const char *files[MAXBLOCKS];	/* __FILE__ of each live block */
	int    lines[MAXBLOCKS];
	int    blocks_used = 0;

	void init()
	{
		for (int i = 0; i < MAXBLOCKS; i++) {
			used[i] = false;
			pnts[i] = nullptr;
			sizes[i] = 0;
		}
	}
	int find_free()
	{
		for (int i = 0; i < MAXBLOCKS; i++)
			if (!used[i]) { blocks_used++; return i; }
		return -1;
	}
	int find_same(void *p) const
	{
		for (int i = 0; i < MAXBLOCKS; i++)
			if (used[i] && pnts[i] == p) return i;
		return -1;
	}
};

XMemTracker g_mem;

} // namespace

#define BlockUsed   g_mem.used
#define BlockPnts   g_mem.pnts
#define BlockSize   g_mem.sizes
#define BlockInFile g_mem.files
#define BlockInLine g_mem.lines
#define BlocksUsed  g_mem.blocks_used

void XMem_Init( void )
{
	MemUsed = 0;
	g_mem.init();
}

int XMem_FindFree( void )
{
	return g_mem.find_free();
}

int XMem_FindSame( void * Pnt )
{
	return g_mem.find_same(Pnt);
}

void * X_strdup( const char *str, const char *in_file, int in_line )
{
	void * Pnt;
	int i;

	i = XMem_FindFree();
	if( i == -1 )
	{
		DebugPrintf( "MEM: Ran out of free memory Blocks\n"); // break point
		return NULL;
	}

	Pnt = strdup( str );

	if( !Pnt )
		return Pnt;

	size_t size = strlen(str)+1;

	BlockUsed[i] = true;
	BlockPnts[i] = Pnt;
	BlockSize[i] = size;
	BlockInFile[i] = in_file;
	BlockInLine[i] = in_line;
	MemUsed += size;

	return Pnt;

}
void * X_malloc( size_t size, const char *in_file, int in_line )
{
	void * Pnt;
	int i;

	i = XMem_FindFree();
	if( i == -1 )
	{
		DebugPrintf( "MEM: Ran out of free memory Blocks\n"); // break point
		return NULL;
	}

	Pnt = malloc( size );

	if( !Pnt )
		return Pnt;

	BlockUsed[i] = true;
	BlockPnts[i] = Pnt;
	BlockSize[i] = size;
	BlockInFile[i] = in_file;
	BlockInLine[i] = in_line;
	MemUsed += size;

	memset(Pnt,0,size); // DEBUG_ON builds hand out zeroed blocks; release malloc does not

	return Pnt;

}

void * X_calloc( size_t num,size_t size, const char *in_file, int in_line )
{
	void * Pnt;
	int i;

	i = XMem_FindFree();
	if( i == -1 )
	{
		DebugPrintf( "MEM: Ran out of free memory Blocks\n"); // break point
		return NULL;
	}

	Pnt = calloc( num , size );

	if( !Pnt )
		return Pnt;

	BlockUsed[i] = true;
	BlockPnts[i] = Pnt;
	BlockSize[i] = num * size;
	BlockInFile[i] = in_file;
	BlockInLine[i] = in_line;
	MemUsed += num * size;

	memset(Pnt,0,size); // redundant: calloc already zeroed the block (this covers only size bytes)

	return Pnt;

}

void X_free( void * Pnt, const char *in_file, int in_line )
{
	int i;
	static const char *last_file = NULL;
	static int last_line = -1;

	if ( !Pnt )
	{
		if ( in_file != last_file || in_line != last_line )
			DebugPrintf( "MEM: Tried to free NULL block in %s line %d\n", in_file, in_line ); // break point
		last_file = in_file;
		last_line = in_line;
		return;
	}
	i = XMem_FindSame( Pnt );
	if( i == -1 )
	{
		if ( in_file != last_file || in_line != last_line )
			DebugPrintf( "MEM: Tried to free un-malloced block in %s line %d\n", in_file, in_line ); // break point
		last_file = in_file;
		last_line = in_line;
		return;
	}
	free(Pnt);
	BlockUsed[i] = false;
	BlockPnts[i] = NULL;
	BlockInFile[ i ] = NULL;
	BlockInLine[ i ] = 0;

	MemUsed -= BlockSize[i];

	BlockSize[i] = 0;
	BlocksUsed--;
}


int UnMallocedBlocks( void )
{
	int i;

	if ( BlocksUsed )
	{
		for ( i = 0; i < MAXBLOCKS; i++ )
		{
			if ( BlockUsed[ i ] )
			{
				DebugPrintf( "MEM: Block size %d allocated in %s line %d but not freed\n",
					BlockSize[ i ], BlockInFile[ i ], BlockInLine[ i ] ); // break point
			}
		}
	}
	DebugPrintf( "MEM: MemUsed = %d   BlocksUsed = %d\n",
		MemUsed, BlocksUsed ); // break point
	return BlocksUsed;
}
