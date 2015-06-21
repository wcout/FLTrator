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

void playSound( const char *file_ )
{
#ifdef WIN32
	PlaySound( file_, NULL, SND_FILENAME | SND_SYNC | SND_NOSTOP );
#else
	char cmd[ 1024 ];
	sprintf( cmd, "aplay -q -N %s  &", file_ );
	system( cmd );
#endif
}

int main( int argc_, const char *argv_[] )
{
	if ( argc_ > 1 )
	{
		playSound( argv_[ 1 ] );
	}
	return 0;
}
