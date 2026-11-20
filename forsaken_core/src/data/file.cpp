#include	<fcntl.h>
#include	<string.h>
#include	<sys/stat.h>
#include	<stdarg.h>
#include	<stdio.h>

#include	"game/main.h"
#include	"data/file.h"
#include	"util/util.h"

#include	<time.h>      // for file_time function

#include <unistd.h>

#ifndef S_IREAD
#define S_IREAD 0400
#endif

#ifndef S_IWRITE
#define S_IWRITE 0200
#endif

FILE * file_open(const char * filename, const char * mode)
{
	return fopen(convert_path(filename), mode);
}

void touch_file( char* path )
{
	if(!File_Exists(path))
		Write_File(path, "", 0);
}

bool is_folder( const char * str )
{
	static struct stat s;
	char * path = convert_path(str);
	if ( stat( path, &s ) == 0 && s.st_mode & S_IFDIR )
		return true;
	DebugPrintf("folder '%s' is not a directory\n",path);
	return false;
}

int folder_exists( const char * pathspec, ... )
{
	char * path;
	static char pathname[ 256 ];
	static struct stat s;
	va_list args;

	va_start( args, pathspec );
	vsnprintf( pathname, sizeof(pathname)-1, pathspec, args );
	va_end( args );

	pathname[sizeof(pathname)-1] = 0;

	path = convert_path(pathname);

	if ( stat( path, &s ) )
	{
		// no such path exists...attempt to create a directory with that name
		return mkdir( path, 0777 ) == 0;
	}
	else if ( s.st_mode & S_IFDIR )
	{
		// path exists and is a directory
		return 1;
	}
	else
	{
		// path exists but is not a directory
		DebugPrintf("'%s' is not a folder\n",path);
		return 0;
	}
	DebugPrintf("folder '%s' does not exist\n",path);
	return 0;
}

bool File_Exists( const char * str )
{
	char * path = convert_path(str);
	int rval  = (access( path, 0 ) == 0);
	DebugPrintf("file: check exists '%s' = %s\n",
		path, (rval)?"exits":"missing");
	return rval;
}

long Write_File( const char * str, const char * File_Buffer, long Write_Size )
{
	int		Handle = -1;
	long	Bytes_Written = 0;

	char * path = convert_path(str);

	Handle = open( path, O_CREAT | O_TRUNC | O_RDWR ,
							  S_IREAD | S_IWRITE );
	if( Handle != -1 ) {
		Bytes_Written = write( Handle, File_Buffer, Write_Size );
		close( Handle );
	}

	return ( Bytes_Written );
}

long Get_File_Size( char * Filename )
{

	struct stat st;

	char * path = convert_path(Filename);

	if ( !stat( path, &st ) )
	{
		return (long) st.st_size;
	}
	else
	{
		DebugPrintf("file '%s' does not exist\n",path);
		perror("stat");
		return 0;
	}

}

bool file_time( const char * path, struct filetime *t )
{
	struct stat st;
	struct tm lt;

	if ( stat( convert_path(path), &st ) != 0 )
	{
		DebugPrintf("failed to retrieve file '%s' modification time\n", path);
		perror("stat");
		return false;
	}
	gmtime_r( &st.st_mtime, &lt );
	t->year = lt.tm_year + 1900;
	t->month = lt.tm_mon + 1;
	t->day = lt.tm_mday;
	t->hour = lt.tm_hour;
	t->minute = lt.tm_min;
	t->second = lt.tm_sec;

	return true;
}

/* Read Read_Size bytes (0: the whole file) of Filename into File_Buffer
   with a single read(). Returns the bytes read, 0 if the file can't be
   opened or read; callers compare it with the size they asked for. */
long Read_File( char * Filename, char * File_Buffer, long Read_Size )
{
	int	Handle = -1;
	long	Bytes_Read = 0;

	char * path = convert_path(Filename);

	if( Read_Size == 0 )
		Read_Size = Get_File_Size( path );

	Handle = open( path, O_RDONLY );

	if( Handle != -1 )
	{
		Bytes_Read = read( Handle, File_Buffer, Read_Size );
		if( Bytes_Read == -1 ) Bytes_Read = 0;

		close( Handle );
	}

	return ( Bytes_Read );
}

bool delete_file( char * str )
{
	char * path = convert_path(str);
	return (unlink( path )==0);
}

/* Rename from over to (both through convert_path). Where rename will not
   replace an existing file (the 3DS SD card), the old to is deleted first
   and the rename retried. */
bool replace_file( const char * from, const char * to )
{
	char src[ 500 ];
	copy_string( src, sizeof( src ), convert_path( from ) );
	char * dst = convert_path( to );
	if( rename( src, dst ) == 0 )
		return true;
	unlink( dst );
	return rename( src, dst ) == 0;
}

/* File search: find_file starts a glob (after convert_path) and returns the
   first match's file name without its folder, find_next_file the next, NULL
   when there are no more; find_close frees the search. Names point into
   the glob results and stay valid until the next find_file or find_close.
   One search at a time. */

#include <glob.h>
/* devkitARM's glob.h uses GLOB_ABEND instead of GLOB_ABORTED */
#ifndef GLOB_ABORTED
#define GLOB_ABORTED GLOB_ABEND
#endif

namespace {

/* The one active file search (one instance, g_search): the glob results
 * and the next name to hand out. pos is -1 when no search is open. */
class FileSearch {
	public:
	char *first( const char * str );
	char *next();
	void  close();

	private:
	glob_t handle = {};
	int    pos = -1;
};

char *find_strip_path( char *path )
{
	char *c = strrchr(path, '/');
	if( c )
		return &c[1];
	else
		return path;
}

int find_globerr( const char *epath, int eerrno )
{
	DebugPrintf("file: glob error while searching for \"%s\": %s\n", epath, strerror(eerrno));
	DebugPrintf("Trying to continue anyway...\n");
	return 0;
}

char *FileSearch::first( const char * str )
{
	char * path = convert_path(str);
	DebugPrintf("file: find_file called with %s\n",path);
	if (pos >= 0)
		globfree(&handle);
	switch( glob(path, 0, find_globerr, &handle) )
	{
	case 0:
		pos = 0;
		DebugPrintf("file: found file \"%s\" (1 of %d)\n", handle.gl_pathv[0], handle.gl_pathc);
		return find_strip_path(handle.gl_pathv[pos++]);
		break;
	case GLOB_NOSPACE:
		DebugPrintf("file: find_file: ran out of memory!\n");
		break;
	case GLOB_ABORTED:
		DebugPrintf("file: find_file: read error!\n");
		break;
	case GLOB_NOMATCH:
		DebugPrintf("file: find_file: no match\n");
		break;
	default:
		DebugPrintf("file: find_file: unknown return value from glob()\n");
		break;
	}
	return nullptr;
}

char *FileSearch::next()
{
	if (pos < 0 || pos >= (int)handle.gl_pathc)
		return nullptr;
	DebugPrintf("file: found file \"%s\" (%d of %d)\n", handle.gl_pathv[pos], pos+1, handle.gl_pathc);
	return find_strip_path(handle.gl_pathv[pos++]);
}

void FileSearch::close()
{
	globfree(&handle);
	pos = -1;
}

FileSearch g_search;

} // namespace

char* find_file( const char * str ) { return g_search.first( str ); }
char* find_next_file( void ) { return g_search.next(); }
void find_close( void ) { g_search.close(); }

bool file_read_cstring( FILE * fp, char * dst, size_t size )
{
	size_t n = 0;
	bool fits = true;
	int c;

	while( ( c = getc( fp ) ) != EOF && c != 0 )
	{
		if( n + 1 < size )
			dst[ n++ ] = (char) c;
		else
			fits = false;
	}
	dst[ n ] = 0;
	return fits && c == 0;
}
