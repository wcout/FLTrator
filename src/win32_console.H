// win_32_console.H
// taken from: http://www.williamwilling.com/blog/?p=74
// ... corrected and modified to my needs.
#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <fstream>

class Console
{
private:
	std::ofstream _out;
	std::ofstream _err;

	std::streambuf *_old_cout;
	std::streambuf *_old_cerr;

	FILE *_stdout;
	FILE *_stderr;

	bool _valid;
public:
	bool valid() const { return _valid; }
	Console( int attach_ = true, bool attach_stdout_ = false );
	~Console();
};

// win32_console.cpp
#include <cstdio>
#include <iostream>
#include <windows.h>

Console::Console( int attach_/* = true*/, bool attach_stdout_/* = false*/ ) :
	_stdout( 0 ),
	_stderr( 0 ),
	_valid( false )
{
	// create a console window
	if ( attach_ )
		_valid = AttachConsole( ATTACH_PARENT_PROCESS );
	else
		_valid = AllocConsole();

	if ( !_valid )
		return;

	// redirect std::cout to our console window
	_old_cout = std::cout.rdbuf();
	_out.open( "CONOUT$" );
	std::cout.rdbuf( _out.rdbuf() );

	// redirect std::cerr to our console window
	_old_cerr = std::cerr.rdbuf();
	_err.open( "CONERR$" );
	std::cerr.rdbuf( _err.rdbuf() );

	if ( attach_stdout_ )
	{
		// make stdout/stderr also use console window (how to reset?)
		_stdout = freopen( "CONOUT$", "w", stdout );
		_stderr = freopen( "CONERR$", "w", stderr );
	}
}

Console::~Console()
{
	if ( !_valid )
		return;

	// reset the standard streams
	_out.flush();
	_err.flush();

	std::cout.rdbuf( _old_cout );
	std::cerr.rdbuf( _old_cerr );

	if ( _stdout )
		fclose( _stdout );
	if ( _stderr )
		fclose( _stderr );

	// remove the console window
	FreeConsole();
}

#endif
