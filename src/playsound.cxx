//
// Copyright 2015-2016 Christian Grabner.
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
static bool HAVE_PIDFILE =
#if !defined(WIN32) && defined(APLAY_HAVE_PIDFILE)
	true
#else
	false
#endif
	;

static void cleanup()
//-------------------------------------------------------------------------------
{
	if ( !HAVE_PIDFILE && pidFileName )
	{
		remove( pidFileName );
	}
}

static void writePidFile( unsigned long pid_ )
//-------------------------------------------------------------------------------
{
	FILE *f = fopen( pidFileName, "wb" );
	if ( f )
	{
		char buf[30];
		snprintf( buf, sizeof( buf ), "%lu", pid_ );
		fwrite( buf, strlen( buf ), 1, f );
		fclose( f );
	}
}

static void playSound( const char *file_, const char *pidFileName_ )
//-------------------------------------------------------------------------------
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
		// must create "pid file" on our own
		writePidFile( GetCurrentProcessId() );
	}
	PlaySound( file_, NULL, SND_FILENAME | SND_SYNC | SND_NOSTOP );
#else
	pid_t pid = fork();
	if ( pid < 0 )
	{
		perror( "fork" );
	}
	else if ( pid == 0 )
	{
		// child process
		static const char wav_player[] = "aplay";
		static const char ogg_player[] = "ogg123";

		// use ogg player for extension .ogg
		const char *ogg = strcasestr( file_, ".ogg" );
		if ( ogg && strlen( file_ ) > 4 && ogg != &file_[ strlen( file_ ) - 4 ] )
			ogg = 0;
		const char *player = ogg ? ogg_player : wav_player;

		if ( pidFileName && HAVE_PIDFILE )
			execlp( player, player, "-q", file_, "--process-id-file", pidFileName, (const char *)NULL );
		else
			execlp( player, player, "-q", file_, (const char*)NULL );

		// should not be reached
		exit( EXIT_FAILURE );
	}
	// parent process
	if ( pidFileName_ && !HAVE_PIDFILE )
	{
		// create a "pid file" if aplay has no support for --process-id-file
		writePidFile( pid );
	}
	// no zombies
	if ( pid > 0 )
		waitpid( pid, 0, 0 );
#endif
}

//-------------------------------------------------------------------------------
int main( int argc_, const char *argv_[] )
//-------------------------------------------------------------------------------
{
	if ( argc_ > 1 )
	{
		playSound( argv_[ 1 ], ( argc_ > 2 ? argv_[ 2 ] : 0 ) );
	}
	return 0;
}
