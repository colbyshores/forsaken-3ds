#include	<fcntl.h>
#include	<string.h>
#include	<sys/stat.h>
#include	<stdarg.h>
#include	<stdio.h>

#include	"game/main.h"
#include	"data/file.h"
#include	"util/util.h"

#include	<time.h>      // for file_time function
#define		O_BINARY 	0 // no such thing on unixa

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

	Handle = open( path, O_CREAT | O_TRUNC | O_BINARY | O_RDWR ,
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

long Read_File( char * Filename, char * File_Buffer, long Read_Size )
{
	int	Handle = -1;
	long	Bytes_Read = 0;

	char * path = convert_path(Filename);

	// get the size of the file
	if( Read_Size == 0 ) 
		Read_Size = Get_File_Size( path );

	// open the file handle
	Handle = open( path, O_RDONLY | O_BINARY );

	// file opened successfully
	if( Handle != -1 )
	{
		// read in the file
		Bytes_Read = read( Handle, File_Buffer, Read_Size );

		// set bytes read to 0
		// if no data was read
		if( Bytes_Read == -1 ) Bytes_Read = 0;

		// close up the file
		close( Handle );

	}

	// return the size of the file
	return ( Bytes_Read );
}

bool delete_file( char * str )
{
	char * path = convert_path(str);
	return (unlink( path )==0);
}

//
// Find File Wrapper
//


#include <glob.h>
/* devkitARM's glob.h uses GLOB_ABEND instead of GLOB_ABORTED */
#ifndef GLOB_ABORTED
#define GLOB_ABORTED GLOB_ABEND
#endif

static glob_t glob_handle;
static int glob_pos = -1;

static char *find_strip_path( char *path )
{
	char *c = strrchr(path, '/');
	if( c )
		return &c[1];
	else
		return path;
}

static int find_globerr( const char *epath, int eerrno )
{
	DebugPrintf("file: glob error while searching for \"%s\": %s\n", epath, strerror(eerrno));
	DebugPrintf("Trying to continue anyway...\n");
	return 0;
}

char* find_file( const char * str )
{
	char * path = convert_path(str);
	DebugPrintf("file: find_file called with %s\n",path);
	if (glob_pos >= 0)
		globfree(&glob_handle);
	switch( glob(path, 0, find_globerr, &glob_handle) )
	{
		case 0:
			glob_pos = 0;
			DebugPrintf("file: found file \"%s\" (1 of %d)\n", glob_handle.gl_pathv[0], glob_handle.gl_pathc);
			return find_strip_path(glob_handle.gl_pathv[glob_pos++]);
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
	return NULL;
}

char* find_next_file( void )
{
	if (glob_pos < 0 || glob_pos >= (int)glob_handle.gl_pathc)
		return NULL;
	DebugPrintf("file: found file \"%s\" (%d of %d)\n", glob_handle.gl_pathv[glob_pos], glob_pos+1, glob_handle.gl_pathc);
	return find_strip_path(glob_handle.gl_pathv[glob_pos++]);
}

void find_close( void )
{
	globfree(&glob_handle);
	glob_pos = -1;
}

