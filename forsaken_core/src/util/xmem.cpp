/* xmem.cpp -- DEBUG_ON allocation tracker and the tracked-byte counter; see xmem.h. */
#define XMEM_NO_REDIRECT // keep xmem.h's malloc/free redirects out of this file
#include "util/xmem.h"
#include <stdio.h>
#include "game/main.h"
#include "util/util.h"

namespace {
constexpr int MAXBLOCKS = XMemTracker::MAXBLOCKS;
}

void XMemTracker::init()
{
	for (int i = 0; i < MAXBLOCKS; i++) {
		used[i] = false;
		pnts[i] = nullptr;
		sizes[i] = 0;
	}
}
int XMemTracker::find_free()
{
	for (int i = 0; i < MAXBLOCKS; i++)
		if (!used[i]) return i;
	return -1;
}
int XMemTracker::find_same(void *p) const
{
	for (int i = 0; i < MAXBLOCKS; i++)
		if (used[i] && pnts[i] == p) return i;
	return -1;
}

size_t	XMemTracker::MemUsed = 0;
XMemTracker XMem;

#define MemUsed     XMemTracker::MemUsed
#define BlockUsed   XMem.used
#define BlockPnts   XMem.pnts
#define BlockSize   XMem.sizes
#define BlockInFile XMem.files
#define BlockInLine XMem.lines
#define BlocksUsed  XMem.blocks_used

void XMem_Init( void )
{
	MemUsed = 0;
	XMem.init();
}

int XMem_FindFree( void )
{
	return XMem.find_free();
}

int XMem_FindSame( void * Pnt )
{
	return XMem.find_same(Pnt);
}

char * X_strdup( const char *str, const char *in_file, int in_line )
{
	char * Pnt;
	int i;

	i = XMem_FindFree();
	if( i == -1 )
	{
		DebugPrintf( "MEM: Ran out of free memory Blocks\n"); // break point
		return nullptr;
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
	BlocksUsed++;

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
		return nullptr;
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
	BlocksUsed++;

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
		return nullptr;
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
	BlocksUsed++;

	return Pnt;

}

void X_free( void * Pnt, const char *in_file, int in_line )
{
	int i;
	static const char *last_file = nullptr;
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
	BlockPnts[i] = nullptr;
	BlockInFile[ i ] = nullptr;
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
