//
// Copyright 2015 Christian Grabner.
//
// This file is part of FLTrator.
//
// FLTrator is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation,  either version 3 of the License, or
// (at your option) any later version.
//
// FLTrator is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY;  without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details:
// http://www.gnu.org/licenses/.
//
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cassert>
#include <cstring>

#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

static const char *pidFileName = 0;

void cleanup()
{
#ifndef APLAY_HAVE_PIDFILE
	if ( pidFileName )
	{
		remove( pidFileName );
	}
#endif
}

void playSound( const char *file_, const char *pidFileName_ )
{
	atexit( cleanup );
	pidFileName = pidFileName_;
#ifdef WIN32
	if ( getenv( "PLAYSOUND_SET_PRIORITY" ) )	// do not fiddle with priority unless requested
	{
		if ( !SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS ) )
			SetPriorityClass( GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS );
	}
	if ( pidFileName_ )
	{
		unsigned long pid = GetCurrentProcessId();
		FILE *f = fopen( pidFileName_, "wb" );
		if ( f )
		{
			char buf[100];
			snprintf( buf, sizeof(buf), "%lu", pid );
			fwrite( buf, strlen(buf), 1, f );
			fclose( f );
		}
	}
	PlaySound( file_, NULL, SND_FILENAME | SND_SYNC | SND_NOSTOP );
#else
	unsigned long pid = fork();
	if ( pid < 0 )
	{
		perror( "fork" );
	}
	else if ( pid == 0 )
	{
#ifdef APLAY_HAVE_PIDFILE
		if ( pidFileName )
			execlp( "aplay", "aplay", "-q", file_,	"--process-id-file", pidFileName, (const char *) NULL );
		else
			execlp( "aplay", "aplay", "-q", file_, (const char*) NULL );
#else
		execlp( "aplay", "aplay", "-q", file_, (const char*) NULL );
#endif
		exit( EXIT_FAILURE );
	}
#ifndef APLAY_HAVE_PIDFILE
	if ( pidFileName_ )
	{
		FILE *f = fopen( pidFileName_, "wb" );
		if ( f )
		{
			char buf[100];
			snprintf( buf, sizeof(buf), "%lu", pid );
			fwrite( buf, strlen(buf), 1, f );
			fclose( f );
		}
	}
#endif
	if ( pid > 0 )
		waitpid( pid, 0, 0 );
#endif
}

int main( int argc_, const char *argv_[] )
{
	if ( argc_ > 1 )
	{
		playSound( argv_[ 1 ], ( argc_ > 2 ? argv_[ 2 ] : 0 ) );
	}
	return 0;
}
