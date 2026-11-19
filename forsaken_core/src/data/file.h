#pragma once

/* file.h -- thin file-system helpers over POSIX
 *
 * file.cpp passes every path through convert_path (util.cpp), which turns
 * backslashes into '/' and lowercases the whole path into one static
 * buffer, so a converted path is valid only until the next call. Engine
 * data paths are written DOS-style and depend on this; files on disk and
 * in romfs must have lowercase names. Read_File with Read_Size 0 reads
 * the whole file; the size and read helpers return 0 on failure.
 * folder_exists takes a printf-style path and creates the folder if it is
 * missing. find_file / find_next_file walk one glob at a time and return
 * bare file names.
 */

#include "game/main.h"

#include <stdio.h>

struct filetime
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
};

void touch_file( char* path );
bool is_folder( const char * path );
int folder_exists( const char * pathspec, ... );
long Get_File_Size( char * Filename );
long Read_File( char * Filename, char * File_Buffer, long Read_Size );
long Write_File( const char * Filename, const char * File_Buffer, long Write_Size );
long Get_File_Size( char * Filename );
bool file_time( const char * path, struct filetime *t );
long Read_File( char * Filename, char * File_Buffer, long Read_Size );
bool File_Exists( const char * Filename );
bool delete_file( char * path );
bool replace_file( const char * from, const char * to );

char* find_file( const char * path );
char* find_next_file( void );
void find_close( void );

FILE * file_open(const char * filename, const char * mode);
