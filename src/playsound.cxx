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
#include <unistd.h>
#include <cassert>

#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

static const char *pidFileName = 0;

void cleanup()
{
	if ( pidFileName )
	{
		remove( pidFileName );
	}
}

void playSound( const char *file_, const char *pidFileName_ )
{
#ifdef WIN32
	pidFileName = pidFileName_;
	atexit( cleanup );
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
	char cmd[ 1024 ];
	if ( !pidFileName_ )
		sprintf( cmd, "aplay -q -N %s  &", file_ );
	else
		sprintf( cmd, "aplay -q -N --process-id-file %s %s  &", pidFileName_, file_ );
	system( cmd );
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
