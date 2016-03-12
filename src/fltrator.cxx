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

#define VERSION "2.1"

#ifdef WIN32
// needed for mingw 4.6 to accept AttachConsole()
#define _WIN32_WINNT 0x0501
#endif

#include <FL/Fl.H>
#include <FL/Enumerations.H>

// We like to use Fl_Image::RGB_scaling and Fl_Window::default_icon(Fl_RGB_Image *)
// which are available from FLTK 1.3.3 on.
#define FLTK_HAS_NEW_FUNCTIONS FL_MAJOR_VERSION > 1 || \
    (FL_MAJOR_VERSION == 1 && \
    ((FL_MINOR_VERSION == 3 && FL_PATCH_VERSION >= 3) || FL_MINOR_VERSION > 3))

#if !(FLTK_HAS_NEW_FUNCTIONS)
#define NO_PREBUILD_LANDSCAPE
#pragma message( "!FLTK_HAS_NEW_FUNCTIONS" )
#endif

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_GIF_Image.H>
#ifndef NO_PREBUILD_LANDSCAPE
#include <FL/Fl_Image_Surface.H>
#endif
#include <FL/Fl_Preferences.H>
#include <FL/filename.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#if ( defined APPLE ) || ( defined __linux__ ) || ( defined __MING32__ )
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#endif
#include <fstream>
#include <sstream>
#include <iostream>
#include <ctime>
#include <cstring>
#include <cassert>
#include <signal.h>
#include <climits>


#ifdef WIN32
#include <mmsystem.h>
#include <io.h>	// _access
#define random rand
#define srandom srand
#else
#include <sys/time.h>
#endif

// fallback Windows native
#ifndef R_OK
#define R_OK	4
#endif
#define _access access

#include "debug.H"

static const unsigned MAX_LEVEL = 10;
static const unsigned MAX_LEVEL_REPEAT = 3;

static const unsigned SCREEN_W = 800;
static const unsigned SCREEN_H = 600;

static const int TIMER_CALLBACK = 9999;

struct KeySet
{
	int left;
	int right;
	int up;
	int down;
};

static const KeySet KEYSET[ 2 ] = {
	{ 'o', 'p', 'q', 'a', },
	{ 'q', 'w', 'p', 'l', }
};
#ifdef WIN32
#define FAST_FPS 100
#pragma message( "FAST_FPS = 100" )
#else
#define FAST_FPS 200
#endif
#define SLOW_FPS 40

#if HAVE_SLOW_CPU
#undef USE_FLTK_RUN
#define USE_FLTK_RUN 1
#define DEFAULT_FPS SLOW_FPS
#else
#define DEFAULT_FPS FAST_FPS
#endif

static bool _USE_FLTK_RUN =
#ifdef USE_FLTK_RUN
	USE_FLTK_RUN
#else
	0
#endif
;

#ifdef WIN32
#if USE_FLTK_RUN
static unsigned FPS = 40;
#pragma message( "USE_FLTK_RUN with FPS 40" )
#else
#pragma message( "USE DEFAULT_FPS" )
static unsigned FPS = DEFAULT_FPS;
#endif
#else
static unsigned FPS = DEFAULT_FPS;
#endif

static double FRAMES = 1. / FPS;
static unsigned DX = 200 / FPS;
static unsigned SCORE_STEP = (int)( 200. / (double)DX ) * DX;

static bool G_paused = true;

static bool G_leftHanded = false;


static unsigned _DX = DX;

#define KEY_LEFT   KEYSET[G_leftHanded].left
#define KEY_RIGHT  KEYSET[G_leftHanded].right
#define KEY_UP     KEYSET[G_leftHanded].up
#define KEY_DOWN   KEYSET[G_leftHanded].down

using namespace std;

#include "random.H"
#include "fullscreen.H"

//-------------------------------------------------------------------------------
enum ObjectType
//-------------------------------------------------------------------------------
{
	O_ROCKET = 1,
	O_DROP = 2,
	O_BADY = 4,
	O_CUMULUS = 8,
	O_RADAR = 16,
	O_PHASER = 32,
	O_COLOR_CHANGE = 64,

	// internal
	O_MISSILE = 1 << 16,
	O_BOMB = 1 << 17,
	O_SHIP = 1 << 18
};

static void setup( int fps_, bool have_slow_cpu_, bool use_fltk_run_  = true )
//-------------------------------------------------------------------------------
{
	if ( fps_ && fps_ != -1 )
	{
		int fps( fps_ );
		bool round_up = fps < 0;
		fps = abs( fps );
		if ( fps > 200 )
			fps = 200;
		if ( fps < 20 )
			fps = 20;
		while ( round_up ? fps <= 200 : fps >= 20 )
		{
			if ( 200 == ( 200 / fps ) * fps )
			{
				FPS = fps;
				break;
			}
			round_up ? fps++ : fps--;
		}
	}
	else if ( have_slow_cpu_ )
	{
		use_fltk_run_ = true;
		FPS = SLOW_FPS;
	}
	else
	{
		FPS = FAST_FPS;
	}

#ifdef WIN32
	if ( use_fltk_run_ )
		FPS = 40;
#endif
	_USE_FLTK_RUN = use_fltk_run_;

	FRAMES = 1. / FPS;
	DX = 200 / FPS;
	SCORE_STEP = (int)( 200. / (double)DX ) * DX;
	_DX = DX;
}

static const string& homeDir()
//-------------------------------------------------------------------------------
{
	static string home;
	if ( home.empty() )
	{
		char home_path[ FL_PATH_MAX ];
		if ( access( "wav", R_OK ) == 0  &&
		     access( "levels", R_OK ) == 0 &&
		     access( "images", R_OK ) == 0 )
		{
			home = "./";
		}
		else
		{
#ifdef WIN32
			fl_filename_expand( home_path, "$APPDATA/" );
#else
			fl_filename_expand( home_path, "$HOME/." );
#endif
			home = home_path;
			home += "fltrator/";
			if ( access( (home + "wav").c_str(), R_OK ) == 0  &&
			     access( (home + "levels").c_str(), R_OK ) == 0 &&
			     access( (home + "images").c_str(), R_OK ) == 0 )
				;
			else
			{
				fl_message( "%s", "Required resources not in place!\nAborting..." );
				exit( -1 );
			}
		}
	}
	return home;
}

static string asString( int n_ )
//-------------------------------------------------------------------------------
{
	ostringstream os;
	os << n_;
	return os.str();
}

static string mkPath( const string& dir_, const string sub_ = "",
	const string& file_ = "" )
//-------------------------------------------------------------------------------
{
	string dir( dir_ );
	if ( dir.size() && dir[ dir.size() - 1 ] != '/' )
		dir.push_back( '/' );
	string sub( sub_ );
	if ( sub.size() && sub[ sub.size() - 1 ] != '/' )
		sub.push_back( '/' );
	return homeDir() + dir + sub + file_;
}

//-------------------------------------------------------------------------------
class Waiter
//-------------------------------------------------------------------------------
{
#ifndef WIN32
	unsigned long startTime;
	unsigned long endTime;
#ifdef _POSIX_MONOTONIC_CLOCK
	struct timespec ts;
#else
	struct timeval tv;
#endif // _POSIX_MONOTONIC_CLOCK
#else // ifndef WIN32

	LARGE_INTEGER startTime, endTime, elapsedMicroSeconds;
	LARGE_INTEGER frequency;
#endif // ifndef WIN32

public:
	Waiter() :
		_elapsedMilliSeconds( 0 ),
		_fltkWaitDelay(
#ifndef WIN32
			0.0005
//			0.001
#else
		0.0
#endif
		)
	{
		char *fltkWaitDelayEnv = getenv( "FLTK_WAIT_DELAY" );
		if ( fltkWaitDelayEnv )
		{
			stringstream ss;
			double fltkWaitDelay;
			ss << fltkWaitDelayEnv;
			ss >> fltkWaitDelay;
			if ( fltkWaitDelay >= 0.0 && fltkWaitDelay <= 0.001 )
				_fltkWaitDelay = fltkWaitDelay;
		}
#ifndef WIN32
#ifdef _POSIX_MONOTONIC_CLOCK
//		printf( "Using clock_gettime()\n" );
		clock_gettime( CLOCK_MONOTONIC, &ts );
		startTime = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
#else
//		printf( "Using gettimeofday()\n" );
		gettimeofday( &tv, NULL );
		startTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif // _POSIX_MONOTONIC_CLOCK
#else
		if ( getenv( "FLTRATOR_SET_PRIORITY" ) )
			SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

		QueryPerformanceFrequency( &frequency );
		QueryPerformanceCounter( &startTime );
#endif // ifndef WIN32

		endTime = startTime;	// -gcc warning with -O3
	}

	unsigned int wait()
	{
		unsigned elapsedMilliSeconds = 0;
		unsigned delayMilliSeconds = 1000 / FPS;

		while ( elapsedMilliSeconds < delayMilliSeconds && Fl::first_window() )
		{
			Fl::wait( _fltkWaitDelay );

#ifndef WIN32
#ifdef _POSIX_MONOTONIC_CLOCK
			clock_gettime( CLOCK_MONOTONIC, &ts );
			endTime = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
#else
			gettimeofday( &tv, NULL );
			endTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
			elapsedMilliSeconds = endTime - startTime;
#else // ifndef WIN32

			// Activity to be timed

			QueryPerformanceCounter( &endTime );
			elapsedMicroSeconds.QuadPart = endTime.QuadPart - startTime.QuadPart;

			//
			// We now have the elapsed number of ticks, along with the
			// number of ticks-per-second. We use these values
			// to convert to the number of elapsed microseconds.
			// To guard against loss-of-precision, we convert
			// to microseconds *before* dividing by ticks-per-second.
			//

			elapsedMicroSeconds.QuadPart *= 1000000;
			elapsedMicroSeconds.QuadPart /= frequency.QuadPart;
			elapsedMilliSeconds = elapsedMicroSeconds.QuadPart / 1000;
#endif // ifndef WIN32

		}
		startTime = endTime;
		_elapsedMilliSeconds = elapsedMilliSeconds;
		return elapsedMilliSeconds;
	}
	unsigned int elapsedMilliSeconds() const { return _elapsedMilliSeconds; }
	double fltkWaitDelay() const { return _fltkWaitDelay; }
private:
	unsigned int _elapsedMilliSeconds;
	double _fltkWaitDelay;
};

//-------------------------------------------------------------------------------
class LevelPath
//-------------------------------------------------------------------------------
{
public:
	LevelPath( const string& baseDir_ ) :
		_baseDir( baseDir_ ), _level( 0 ) {}
	string get( const string& file_ ) const
	{
		if ( file_.find( '/' ) != string::npos )	// do not change paths
			return file_;
		if ( _level )
		{
			string p = mkPath( _baseDir, asString( _level ), file_ + _ext );
			if ( access( p.c_str(), R_OK ) == 0 )
				return p;
		}
		return mkPath( _baseDir, "", file_ + _ext );
	}
	void level( size_t level_ ) { _level = level_; }
	void ext( const string ext_ )
	{
		_ext = ext_;
		if ( _ext.size() )
			_ext.insert( 0, "." );
	}
private:
	string _baseDir;
	size_t _level;
	string _ext;
};

static string levelPath( const string& file_ )
//-------------------------------------------------------------------------------
{
	return mkPath( "levels", "", file_ );
}

static string demoPath( const string& file_ )
//-------------------------------------------------------------------------------
{
	return mkPath( "demo", "", file_ );
}

static LevelPath wavPath( "wav" );
static LevelPath imgPath( "images" );

static int rangedValue( int value_, int min_, int max_ )
//-------------------------------------------------------------------------------
{
	int v( value_ );
	if ( min_ > max_ )
	{
		int t = min_;
		min_ = max_;
		max_ = t;
	}
	if ( v < min_ )
		v = min_;
	if ( v > max_ )
		v = max_;
	return v;
}

static int rangedRandom( int min_, int max_ )
//-------------------------------------------------------------------------------
{
	if ( min_ > max_ )
	{
		int t = min_;
		min_ = max_;
		max_ = t;
	}
	return min_ + Random::Rand() % ( max_ - min_ + 1 );
}

//-------------------------------------------------------------------------------
struct Point
//-------------------------------------------------------------------------------
{
	Point( int x_ = 0, int y_ = 0 ) :
		x( x_ ),
		y( y_ )
	{}
	int x;
	int y;
};

//-------------------------------------------------------------------------------
class Rect
//-------------------------------------------------------------------------------
{
public:
	Rect( int x_, int y_, int w_, int h_ ) :
		_x( x_ ),
		_y( y_ ),
		_w( w_ ),
		_h( h_ )
	{
	}
	bool intersects( const Rect& r_ ) const
	{
		return ! ( _x + _w - 1 < r_.x()      ||
		           _y + _h - 1 < r_.y()      ||
	              _x > r_.x() + r_.w() - 1  ||
		           _y > r_.y() + r_.h() - 1 );
	}
	Rect intersection_rect( const Rect& r_ ) const
	{
		int x = max( _x, r_.x() );
		int y = max( _y, r_.y() );
		int xr = min( _x + _w, r_.x() + r_.w() );
		int yr = min( _y + _h, r_.y() + r_.h() );
		if ( xr > x && yr > y )
			return Rect( x, y, xr - x, yr - y );
		return Rect( 0, 0, 0, 0 );
	}
	Rect relative_rect( const Rect& r_ ) const
	{
		return Rect( r_.x() - _x, r_.y() - _y, r_.w(), r_.h() );
	}
	bool contains( const Rect& r_ ) const
	{
		return within( r_.x(), r_.y(), *this ) &&
		       within( r_.x() + r_.w() - 1, r_.y() + r_.h() - 1, *this );
	}
	bool inside( const Rect& r_ ) const
	{
		return within( _x, _y, r_ ) &&
		       within( _x + _w - 1, _y + _h - 1, r_ );
	}
	int x() const { return _x; }
	int y() const { return _y; }
	int w() const { return _w; }
	int h() const { return _h; }
#ifdef DEBUG_COLLISION
	virtual std::ostream &printOn( std::ostream &os_ ) const;
#endif
private:
	bool within( int x_, int y_, const Rect& r_ ) const
	{
		return x_ >= r_.x() && x_ < r_.x() + r_.w() &&
		       y_ >= r_.y() && y_ < r_.y() + r_.h();
	}
private:
	int _x;
	int _y;
	int _w;
	int _h;
};

#ifdef DEBUG_COLLISION
inline std::ostream &operator<<( std::ostream &os_, const Rect &r_ )
{ return r_.printOn( os_ ); }

/*virtual*/
ostream &Rect::printOn( ostream &os_ ) const
//--------------------------------------------------------------------------
{
	os_ << "Rect(" << x() << "/" << y() <<  "-" << w() << "x" << h() << ")";
	return os_;
} // printOn
#endif

//-------------------------------------------------------------------------------
class Cfg : public Fl_Preferences
//-------------------------------------------------------------------------------
{
	typedef Fl_Preferences Inherited;
public:
	struct User
	{
		User() : score( 0 ), level( 0 ), completed( 0 ) {}
		string name;
		int score;
		int level;
		int completed;
	};
	Cfg( const char *vendor_, const char *appl_ );
	void load();
	void writeUser( const string& user_,
	                int score_ = -1,
	                int level_ = -1,
	                int completed_ = 0 );
	const User& readUser( const string& user_ );
	const User& readUser( User& user_ );
	const User& writeUser( User& user_ );
	bool removeUser( const string& user_ );
	size_t non_zero_scores() const;
	const User& best() const;
	unsigned hiscore() const { return best().score; }
	const vector<User>& users() const { return _users; }
	const string& last_user() const { return _last_user; }
private:
	vector<User> _users;
	string _last_user;
};
typedef Cfg::User User;

//-------------------------------------------------------------------------------
// class Cfg : public Fl_Preferences
//-------------------------------------------------------------------------------
Cfg::Cfg( const char *vendor_, const char *appl_ ) :
	Inherited( USER, vendor_, appl_ )
//-------------------------------------------------------------------------------
{
	load();
}

void Cfg::load()
//-------------------------------------------------------------------------------
{
	_users.clear();
	for ( int i = 0; i < groups(); i++ )
	{
		// last playing user
		char *last_user = 0;
		get( "last_user", last_user, 0 );
		_last_user.erase();
		if ( last_user )
			_last_user = last_user;
		free( last_user );

		// collect all users data
		string name( group( i ) );
		Fl_Preferences user( this, name.c_str() );
		User e;
		e.name = name;
		user.get( "score", e.score, 0 );
		user.get( "level", e.level, 0 );
		user.get( "completed", e.completed, 0 );
//		printf(" entry: '%s' %d %d %d\n", e.name.c_str(), e.score, e.level, e.completed );
		bool inserted = false;
		for ( size_t j = 0; j < _users.size(); j++ )
		{
			if ( _users[j].score <= e.score )
			{
				_users.insert( _users.begin() + j,  e );
				inserted = true;
				break;
			}
		}
		if ( !inserted )
			_users.push_back( e );

		// check if last_user is valid
		bool found = false;
		for ( size_t j = 0; j < _users.size(); j++ )
		{
			if ( _users[j].name == _last_user )
			{
				found = true;
				break;
			}
		}
		if ( !found )
		{
			_last_user.erase();
		}
	}
}

void Cfg::writeUser( const string& user_,
	                  int score_/* = -1*/,
	                  int level_/* = -1*/,
	                  int completed_/* = 0*/ )
//-------------------------------------------------------------------------------
{
	set( "last_user", user_.c_str() );
	Fl_Preferences user( this, user_.c_str() );
	if ( score_ >= 0 )
		user.set( "score", (int)score_ );
	if ( level_ >= 0 )
		user.set( "level", (int)level_ );
	user.set( "completed", completed_ );
	load();
}

const User& Cfg::readUser( const string& user_ )
//-------------------------------------------------------------------------------
{
	load();
	for ( size_t i = 0; i < _users.size(); i++ )
	{
		if ( _users[i].name == user_ )
			return _users.at( i );
	}
	Fl_Preferences user( this, user_.c_str() );
	writeUser( user_, 0, 0 );
	return readUser( user_ );
}

const User& Cfg::readUser( User& user_ )
//-------------------------------------------------------------------------------
{
	User user = readUser( user_.name );
	user_ = user;
	return user_;
}

const User& Cfg::writeUser( User& user_ )
//-------------------------------------------------------------------------------
{
	writeUser( user_.name, user_.score, user_.level, user_.completed );
	return readUser( user_ );
}

bool Cfg::removeUser( const string& user_ )
//-------------------------------------------------------------------------------
{
	bool ret = deleteGroup( user_.c_str() ) != 0;
	load();
	return ret;
}

size_t Cfg::non_zero_scores() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < _users.size(); i++ )
		if ( !_users[i].score )
			return i;
	return _users.size();
}

const User& Cfg::best() const
//-------------------------------------------------------------------------------
{
	static const User zero;
	return _users.size() ? _users[0] : zero;
}

//-------------------------------------------------------------------------------
class Audio
//-------------------------------------------------------------------------------
{
public:
	static Audio *instance( bool create_ = true );
	bool play( const char *file_, bool bg_ = false );
	bool disabled() const { return _disabled; }
	bool bg_disabled() const { return hasBgSound() ? _bg_disabled : true; }
	bool enabled() const { return !_disabled; }
	bool bg_enabled() const { return hasBgSound() ? !_bg_disabled : false; }
	static bool hasBgSound()
	{
#ifdef WIN32
		return true;
#elif __APPLE_
		return false;
#else
		return true;
#endif
	}
	void disable( bool disable_ = true ) { _disabled = disable_; }
	void enable() { disable( false ); }
	void bg_disable( bool disable_ = true ) { _bg_disabled = disable_; }
	void cmd( const string& cmd_ );
	string cmd( bool bg_ = false ) const { return bg_ ? _bgPlayCmd : _playCmd; }
	string ext() const { return _ext; }
	void stop_bg();
	void check( bool killOnly = false );
	void shutdown();
private:
	Audio();
	~Audio();
	void stop( const string& pidfile_ );
	bool kill_sound( const string& pidfile_ );
	bool terminate_player( pid_t pid_, const string& pidfile_ );
	string _playCmd;
	string _bgPlayCmd;
	string _ext;
	bool _disabled;
	bool _bg_disabled;
	int _id;
	string _bgpidfile;
	vector<string> _retry_bgpidfile;
	string _bgsound;
};

//-------------------------------------------------------------------------------
// class Audio
//-------------------------------------------------------------------------------

// helper
#ifdef WIN32
BOOL TerminateProcess(DWORD dwProcessId, UINT uExitCode)
{
	DWORD dwDesiredAccess = PROCESS_TERMINATE;
	BOOL  bInheritHandle  = FALSE;
	HANDLE hProcess = OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId);
	if (hProcess == NULL)
		return FALSE;

	BOOL result = TerminateProcess(hProcess, uExitCode);

	CloseHandle(hProcess);

	return result;
}
#endif

static string bgPidFileName( int id_ )
//-------------------------------------------------------------------------------
{
	ostringstream os;
#ifdef WIN32
	os << "playsound_pid_" << id_;
#else
	os << "/tmp/aplay_pid_" << id_;
#endif
	return os.str();
}

Audio::Audio() :
	_disabled( false ),
	_bg_disabled( false),
	_id( 0 )
//-------------------------------------------------------------------------------
{
	cmd( string() );
	// aplay might still be running from a previous invocation,
	// so look for the first non existing pidfile.
	// (Should we use mkstemp() to create a filename?)
	int id( _id );
	while ( !access( bgPidFileName( ++id ).c_str(), R_OK ) ) ;
	_id = --id;
}

Audio::~Audio()
//-------------------------------------------------------------------------------
{
	stop_bg();
	if ( _retry_bgpidfile.size() )
	{
		cerr << "Waiting to stop " << _retry_bgpidfile.size() << " bgsound(s)" << endl;
#ifndef WIN32
//		 NOTE: WIN32 has no sleep(sec) - it has Sleep(msec).
//	          But anyway this waiting makes only sense for aplay (Linux).
		sleep( 1 );
#endif
		check( true );
	}
}

void Audio::shutdown()
//-------------------------------------------------------------------------------
{
	instance( false );
}

static void trim( string& s_ )
//-------------------------------------------------------------------------------
{
	while ( s_.size() && isspace( s_[0] ) )
		s_.erase( 0, 1 );
	while ( s_.size() && isspace( s_[s_.size() - 1] ) )
		s_.erase( s_.size() - 1 );
}

static string quote( string s_ )
//-------------------------------------------------------------------------------
{
	s_.insert( 0, "\"" );
	s_.append( "\"" );
	return s_;
}

static void parseAudioCmd( const string &cmd_,
                           string& playCmd_, string& bgPlayCmd_, string& ext_ )
//-------------------------------------------------------------------------------
{
	string cmd( cmd_ );
	trim( cmd );
	if ( cmd.empty() )
		return;
	string type;
	string arg;
	size_t pos = cmd.find( '=' );
	if ( pos != string::npos )
	{
		type = cmd.substr( 0, pos );
		if ( cmd.size() > pos )
			arg = cmd.substr( pos + 1 );
	}
	else
	{
		arg = cmd;
	}
	trim( type );
	trim( arg );
	if ( arg.empty() )
		return;
//	printf( "type: '%s' arg: '%s'\n", type.c_str(), arg.c_str() );
	if ( type.empty() || "cmd" == type )
		playCmd_ = arg;
	else if ( "bgcmd" == type)
		bgPlayCmd_ = arg;
	else if ( "ext" == type )
		ext_ = arg;
	else
		fprintf( stderr, "Invalid audio command: '%s'\n", arg.c_str() );
}

static void parseAudioCmdLine( const string &cmd_,
                               string& playCmd_, string& bgPlayCmd_, string& ext_ )
//-------------------------------------------------------------------------------
{
	string cmd( cmd_ );
	size_t pos;
	while ( ( pos = cmd.find( ';' )) != string::npos )
	{
		parseAudioCmd( cmd.substr( 0, pos ), playCmd_, bgPlayCmd_, ext_ );
		cmd.erase( 0, pos + 1 );
	}
	parseAudioCmd( cmd, playCmd_, bgPlayCmd_, ext_ );
}

void Audio::cmd( const string& cmd_ )
//-------------------------------------------------------------------------------
{
	string playCmd;
	string bgPlayCmd;
	string ext;
	parseAudioCmdLine( cmd_, playCmd, bgPlayCmd, ext );
	static const string DefaultCmd =
#ifdef WIN32
	"playsound %F"
#elif __APPLE__
	"play %f"
#else
	// linux
	"aplay -q -N %f"
#endif
	;
	static const string DefaultBgCmd =
#ifdef WIN32
	"playsound %F %p"
#elif __APPLE__
	"play %f"
#else
	// linux
	"aplay -q -N --process-id-file %p %f"
#endif
	;
	_playCmd = playCmd.empty() ? DefaultCmd : playCmd;
	_bgPlayCmd = bgPlayCmd.empty() ? DefaultBgCmd : bgPlayCmd;
	_ext = ext.empty() ? "wav" : ext;
}

/*static*/
Audio *Audio::instance( bool create_/* = true*/)
//-------------------------------------------------------------------------------
{
	static Audio *inst = 0;
	if ( !create_ )
	{
		delete inst;
		inst = 0;
	}
	else if ( !inst )
		inst = new Audio();
	return inst;
}

bool Audio::play( const char *file_, bool bg_/* = false*/ )
//-------------------------------------------------------------------------------
{
	int ret = 0;
	bool disabled( ( bg_ && _bg_disabled ) || ( !bg_ && _disabled ) );
	if ( !disabled && file_ )
	{
		string file( file_ );
		string cmd( bg_ ? _bgPlayCmd : _playCmd );
		bool runInBg( false );

		if ( bg_ )
		{
			stop_bg();
			_bgpidfile = bgPidFileName( ++_id );
			_bgsound = file;
		}

		size_t pos = cmd.find( "%f" );
		if ( pos != string::npos )
			runInBg = true;
		else
			pos = cmd.find( "%F" );
		if ( pos == string::npos )
			cmd += ( ' ' + quote( wavPath.get( file ) ) );
		else
		{
			cmd.erase( pos, 2 );
			cmd.insert( pos, quote( wavPath.get( file ) ) );
		}
		if ( bg_ )
		{
			size_t pos = cmd.find( "%p" );
			if ( pos == string::npos )
				pos = cmd.find( "%P" );
			if ( pos == string::npos )
				cmd += ( ' ' + _bgpidfile );
			else
			{
				cmd.erase( pos, 2 );
				cmd.insert( pos, _bgpidfile );
			}
		}
		if ( runInBg )
			cmd += " &";

//		printf( "cmd: %s\n", cmd.c_str() );
#ifdef WIN32
		STARTUPINFO si = { 0 };
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi;
		ret = !CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
		                     CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
#elif __APPLE__
		ret = system( cmd.c_str() );
#else
		// linux
		ret = system( cmd.c_str() );
#endif
	}
	return !disabled && !ret;
}

bool Audio::terminate_player( pid_t pid_, const string& pidfile_ )
//-------------------------------------------------------------------------------
{
//	printf( "%s: kill player %d\n", pidfile_.c_str(), pid );
	bool success( true );
#ifndef WIN32
	if ( -1 == kill( pid_, SIGTERM ) && errno != ESRCH )
	{
		perror( "kill SIGTERM" );
		success = false;
	}
#else
	success = TerminateProcess( pid_, 0 );
	// WIN32: terminated process can't close pidfile, so we must do this
	DBG(( "remove pidfile '%s'", pidfile_.c_str() ));
	std::remove( pidfile_.c_str() );
#endif
	return success;
}

bool Audio::kill_sound( const string& pidfile_ )
//-------------------------------------------------------------------------------
{
	bool success( true );
	ifstream ifs( pidfile_.c_str() );
	if ( ifs.is_open() )
	{
		string line;
		getline( ifs, line );
		ifs.close();	// free handle to file, otherwise remove() fails under WIN32!
		pid_t pid = atoi( line.c_str() );
		if ( pid > 0 )
			success = terminate_player( pid, pidfile_ );
	}
	else
	{
		//	pidfile not yet created or already gone
		success = false;
	}
	return success;
}

void Audio::stop_bg()
//-------------------------------------------------------------------------------
{
	check( true );
	if ( _bgpidfile.size() )
	{
		if ( !kill_sound( _bgpidfile ) )
		{
			_retry_bgpidfile.push_back( _bgpidfile );	// try later
		}
		_bgpidfile.erase();
	}
}

void Audio::check( bool killOnly_/* = false*/ )
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < _retry_bgpidfile.size(); i++ )
	{
		if ( kill_sound( _retry_bgpidfile[i] ) )
		{
			_retry_bgpidfile.erase( _retry_bgpidfile.begin() + i );
			i--;
		}
	}
	while ( _retry_bgpidfile.size() > 5 )
		_retry_bgpidfile.erase( _retry_bgpidfile.begin() );

	if ( !killOnly_ && _bgpidfile.size() )
	{
		ifstream ifs( _bgpidfile.c_str() );
		if ( ifs.fail() )
		{
//			printf( "restart bgsound '%s' '%s'\n", _bgpidfile.c_str(), _bgsound.c_str() );
			play( _bgsound.c_str(), true );
		}
	}
}

//-------------------------------------------------------------------------------
class Object
//-------------------------------------------------------------------------------
{
public:
	Object( ObjectType o_, int x_, int y_, const char *image_ = 0, int w_ = 0, int h_ = 0 );
	virtual ~Object();
	void animate();
	bool collision( const Object& o_ ) const;
	void stop_animate() { Fl::remove_timeout( cb_animate, this ); }
	bool hit();
	int hits() const { return _hits; }
	void image( const char *image_ );
	bool isTransparent( size_t x_, size_t y_ ) const;
	bool started() const { return _state > 0; }
	virtual double timeout() const { return _timeout; }
	void timeout( double timeout_ ) { _timeout = timeout_; }
	virtual const char* start_sound() const { return 0; }
	virtual const char* image_started() const { return 0; }
	void start( size_t speed_ = 1 );
	virtual void update();
	virtual void draw();
	void draw_collision() const;
	bool nostart() const { return _nostart; }
	void nostart( bool nostart_) { _nostart = nostart_; }

	void x( int x_ ) { _x = x_ + _w / 2; }
	void y( int y_ ) { _y = y_ + _h / 2; }
	int x() const { return _x - _w / 2; }	// left x
	int y() const { return _y - _h / 2; }	// top y

	void cx( int cx_ ) { _x = cx_; }
	void cy( int cy_ ) { _y = cy_; }
	int cx() const { return _x; }	// center x
	int cy() const { return _y; }	// center y

	int w() const { return _w; }
	int h() const { return _h; }

	int state() const { return _state; }
	unsigned speed() const { return _speed; }
	void speed( unsigned speed_ ) { _speed = speed_; }
	const Rect rect() const { return Rect( x(), y(), w(), h() ); }
	Fl_Image *image() const { return _image; }
	ObjectType o() const { return _o; }
	const bool isType( ObjectType o_ ) const { return _o == o_; }
	void onExplosionEnd();
	static void cb_explosion_end( void *d_ );
	void crash();
	void explode( double to_ = 0.05 );
	bool exploding() const { return _exploding; }
	bool exploded() const { return _exploded; }
	long data1() const { return _data1; }
	long data2() const { return _data2; }
	void data1( long data1_ ) { _data1 = data1_; }
	void data2( long data2_ ) { _data2 = data2_; }
private:
	void _explode( double to_ = 0. );
	virtual bool onHit() { return false; }
private:
	static void cb_animate( void *d_ );
	static void cb_update( void *d_ );
protected:
	ObjectType _o;
	int _x, _y;
	int _w, _h;
	unsigned _speed;
	unsigned _state;
	long _data1;
	long _data2;
private:
	bool _nostart;
	bool _exploding;	// state exploding
	bool _exploded;
	bool _hit;	// state 'hit' for explosion drawing
	double _timeout;
	double _animate_timeout;
	Fl_Shared_Image *_image;
	Fl_RGB_Image * _imageForDrawing;
	int _ox;
	int _hits;
};

//-------------------------------------------------------------------------------
// class Object
//-------------------------------------------------------------------------------
Object::Object( ObjectType o_, int x_, int y_,
                const char *image_/* = 0*/, int w_/* = 0*/, int h_/* = 0*/ ) :
	_o( o_ ),
	_x( x_ ),
	_y( y_ ),
	_w( w_ ),
	_h( h_ ),
	_speed( 1 ),
	_state( 0 ),
	_data1( 0 ),
	_data2( 0 ),
	_nostart( false ),
	_exploding( false ),
	_exploded( false ),
	_hit( false ),
	_timeout( 0.05 ),
	_animate_timeout( 0.0 ),
	_image( 0 ),
	_imageForDrawing( 0 ),
	_ox( 0 ),
	_hits( 0 )
//-------------------------------------------------------------------------------
{
	if ( image_ )
	{
		image( image_ );
		assert( _image && _image->count() );
		if ( _animate_timeout )
			Fl::add_timeout( _animate_timeout, cb_animate, this );
	}
}

/*virtual*/
Object::~Object()
//-------------------------------------------------------------------------------
{
	Fl::remove_timeout( cb_animate, this );
	Fl::remove_timeout( cb_update, this );
	Fl::remove_timeout( cb_explosion_end, this );
	if ( _image )
		_image->release();
	delete _imageForDrawing;
}

void Object::animate()
//-------------------------------------------------------------------------------
{
	if ( G_paused ) return;
	_ox += _w;
	if ( _ox >= _image->w() )
		_ox = 0;
}

bool Object::collision( const Object& o_ ) const
//-------------------------------------------------------------------------------
{
	if ( rect().intersects( o_.rect() ) )
	{
		const Rect ir( rect().intersection_rect( o_.rect() ) );
		const Rect r_this( rect().relative_rect( ir ) );
		const Rect r_that( o_.rect().relative_rect( ir ) );
#ifdef DEBUG_COLLISION
		cout << rect() << "/" << o_.rect() << ": ir (" << ir << " : r_this " << r_this << " r_that " << r_that << endl;
		fl_color( FL_GREEN );
		fl_rect( ir.x(), ir.y(), ir.w(), ir.h() );
#endif
		for ( int y = 0; y < r_this.h(); y++ )
		{
			for ( int x = 0; x < r_this.w(); x++ )
			{
#ifndef DEBUG_COLLISION
				if ( !isTransparent( r_this.x() + x, r_this.y() + y ) &&
				     !o_.isTransparent( r_that.x() + x, r_that.y() + y ) )
					return true;
			}
		}
#else
				bool p_this = isTransparent( r_this.x() + x, r_this.y() + y );
				bool p_that = o_.isTransparent( r_that.x() + x, r_that.y() + y );
				if ( p_this && p_that )
					printf( "%c", '.' );
				else if ( !p_this && !p_that )
					printf( "%c", 'X');
				else if ( !p_this )
					printf( "%c", 'R' );
				else if ( !p_that )
					printf( "%c", 'S' );
				else
					printf( "%c", '?' );
			}
			printf( "\n" );
		}
		printf( "\n" );
#endif
	}
	return false;
}

bool Object::hit()
//-------------------------------------------------------------------------------
{
	_hits++;
	_explode();
	return onHit();
}

void Object::image( const char *image_ )
//-------------------------------------------------------------------------------
{
	assert( image_ );
	Fl::remove_timeout( cb_animate, this );
	Fl_Shared_Image *image = Fl_Shared_Image::get( imgPath.get( image_ ).c_str() );
	if ( image && image->count() )
	{
		if ( _image )
			_image->release();
		_image = image;
		delete _imageForDrawing;
#if FLTK_HAS_NEW_FUNCTIONS
		_imageForDrawing = new Fl_RGB_Image( (Fl_Pixmap *)_image );
#endif
		_animate_timeout = 0.;
		const char *p = strstr( image_, "_" );
		if ( p && isdigit( p[1] ) )
		{
			p++;
			int frames = atoi( p );
			_w = _image->w() / frames;
			p = strstr( p, "_" );
			if ( p )
			{
				p++;
				_animate_timeout = (double)atoi( p ) / 1000;
			}
		}
		else
		{
			_w = _image->w();
		}
		_h = _image->h();
	}
}

bool Object::isTransparent( size_t x_, size_t y_ ) const
//-------------------------------------------------------------------------------
{
	// valid only for pixmaps!
	assert( (int)x_ < _image->w() && (int)y_ < _image->h() );
	return _image->data()[y_ + 2][x_] == ' ';
}

void Object::start( size_t speed_/* = 1*/ )
//-------------------------------------------------------------------------------
{
	const char *img = image_started();
	if ( img )
		image( img );
	_speed = speed_;
	Fl::add_timeout( timeout(), cb_update, this );
	Audio::instance()->play( start_sound() );
	_state = 1;
}

/*virtual*/
void Object::update()
//-------------------------------------------------------------------------------
{
	_state++;
}

#if !defined(FLTK_USES_XRENDER) && !defined(WIN32)
#if FLTK_HAS_NEW_FUNCTIONS
static void clipImage( int& x_, int& y_, int& w_, int &h_, int& ox_, int& oy_,
                       int W_, int H_ )
//-------------------------------------------------------------------------------
{
	ox_ = 0;
	oy_ = 0;
	if ( x_ <  0 )
	{
		ox_ = -x_;
		x_ = 0;
		w_ -= ox_;
	}
	if ( y_ <  0 )
	{
		oy_ = -y_;
		y_ = 0;
		h_ -= oy_;
	}
	if ( x_ + w_ >= W_ )
	{
		w_ -= ( ( x_ + w_ ) - W_ );
	}
	if ( y_ + h_ >= H_ )
	{
		h_ -= ( ( y_ + h_ ) - H_ );
	}
}
#endif
#endif

/*virtual*/
void Object::draw()
//-------------------------------------------------------------------------------
{
	if ( !_exploded )
	{
#if FLTK_HAS_NEW_FUNCTIONS
#if !defined(WIN32) && !defined(FLTK_USES_XRENDER)
		int X = x();
		int Y = y();
		int W = _w;
		int H = _h;
		int ox;
		int oy;
		// RGB image must be clipped to screen otherwise drawing artefacts!
#pragma message( "No Xrender support - fix clipping of RGB image with alpha" )
		clipImage( X, Y, W, H, ox, oy, SCREEN_W, SCREEN_H );
		_imageForDrawing->draw( X, Y, W, H, _ox + ox, oy );
#else	// !defined(WIN32) && !defined(FLTK_USES_XRENDER)
		_imageForDrawing->draw( x(), y(), _w, _h, _ox, 0 );
#endif
#else // FLTK_HAS_NEW_FUNCTIONS
		_image->draw( x(), y(), _w, _h, _ox, 0 );
#endif // FLTK_HAS_NEW_FUNCTIONS
	}
#ifdef DEBUG_COLLISION
	fl_color( FL_YELLOW );
	Rect r( rect() );
	fl_rect( r.x(), r.y(), r.w(), r.h() );
	fl_point( cx(), cy() );
#endif
	if ( _exploding || _hit )
		draw_collision();
}

void Object::draw_collision() const
//-------------------------------------------------------------------------------
{
	int pts = w() * h() / 20;
	int sz = pts / 30;
	++sz &= ~1;
	for ( int i = 0; i < pts; i++ )
	{
		unsigned X = Random::pRand() % w();
		unsigned Y = Random::pRand() % h();
		if ( !isTransparent( X + _ox, Y ) )
		{
			fl_rectf( x() + X - sz / 2, y() + Y - sz / 2, sz, sz,
			          ( Random::pRand() % 2 ? ( Random::pRand() % 2 ? 0xff660000 : FL_RED ) : FL_YELLOW ) );
		}
	}
}

/*static*/
void Object::cb_update( void *d_ )
//-------------------------------------------------------------------------------
{
	((Object *)d_)->update();
	Fl::repeat_timeout( ((Object *)d_)->_timeout, cb_update, d_ );
}

/*static*/
void Object::cb_animate( void *d_ )
//-------------------------------------------------------------------------------
{
	((Object *)d_)->animate();
	Fl::repeat_timeout( ((Object *)d_)->_animate_timeout, cb_animate, d_ );
}

void Object::onExplosionEnd()
//-------------------------------------------------------------------------------
{
	_hit = false;
	if ( _exploding )
	{
//		_exploding = false;
		_exploded = true;
	}
}

/*static*/
void Object::cb_explosion_end( void *d_ )
//-------------------------------------------------------------------------------
{
	((Object *)d_)->onExplosionEnd();
}

void Object::crash()
//-------------------------------------------------------------------------------
{
	_explode( 0.2 );
}

void Object::explode( double to_/* = 0.05*/ )
//-------------------------------------------------------------------------------
{
	_explode( to_ );
}

void Object::_explode( double to_ )
//-------------------------------------------------------------------------------
{
	if ( !_exploding && !_exploded )
	{
		if ( to_ )	// real explosion
		{
			_exploding = true;
			Fl::add_timeout( to_, cb_explosion_end, this );
		}
		else	// hit feedback explosion
		{
			_hit = true;
			Fl::add_timeout( 0.02, cb_explosion_end, this );
		}
	}
}

//-------------------------------------------------------------------------------
class Rocket : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Rocket( int x_ = 0, int y_ = 0 ) :
		Inherited( O_ROCKET, x_, y_, "rocket.gif" )
	{
	}
	bool lifted() const { return started(); }
	virtual const char *start_sound() const { return "launch"; }
	virtual const char* image_started() const { return "rocket_launched.gif"; }
	virtual void update()
	{
		if ( G_paused ) return;
		if ( !exploding() && !exploded() )
		{
			size_t delta = ( 1 + _state / 10 ) * _speed;
			if ( delta > 12 )
				delta = 12;
			_y -= delta;
		}
		Inherited::update();
	}
};

//-------------------------------------------------------------------------------
class Radar : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Radar( int x_ = 0, int y_ = 0) :
		Inherited( O_RADAR, x_, y_, "radar_00014_0200.gif" )
	{
	}
	bool defunct() const { return hits() > 1; }
	virtual bool onHit()
	{
		if ( hits() == 1 )
			stop_animate();
		return hits() > 2;
	}
};

//-------------------------------------------------------------------------------
class Drop : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Drop( int x_ = 0, int y_ = 0) :
		Inherited( O_DROP, x_, y_, "drop.gif" )
	{
	}
	bool dropped() const { return started(); }
	virtual const char *start_sound() const { return "drop"; }
	virtual bool onHit()
	{
		// TODO: other image?
		return false;
	}
	virtual void update()
	{
		if ( G_paused ) return;
		size_t delta = ( 1 + _state / 10 ) * _speed;
		if ( delta > 12 )
			delta = 12;
		_y += delta;
		Inherited::update();
	}
};

//-------------------------------------------------------------------------------
class Bady : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Bady( int x_ = 0, int y_ = 0 ) :
		Inherited( O_BADY, x_, y_, "bady.gif" ),
		_up( false ),	// default: go down first
		_stamina( 5 )
	{
	}
	bool moving() const { return started(); }
	virtual const char *start_sound() const { return "bady"; }
	void turn() { _up = !_up; }
	bool turned() const { return _up; }
	void stamina( int stamina_ ) { _stamina = stamina_; }
	virtual bool onHit()
	{
		if ( hits() >= _stamina - 1 )
			image( "bady_hit.gif" );
		return hits() >= _stamina;
	}
	virtual void update()
	{
		if ( G_paused ) return;
		size_t delta = _speed;
		if ( delta > 12 )
			delta = 12;
		_y = _up ? _y - delta : _y + delta;
		Inherited::update();
	}
private:
	bool _up;
	int _stamina;
};

//-------------------------------------------------------------------------------
class Cumulus : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Cumulus( int x_ = 0, int y_ = 0 ) :
		Inherited( O_CUMULUS, x_, y_, "cumulus.gif" ),
		_up( false )	// default: go down first
	{
	}
	bool moving() const { return started(); }
	void turn() { _up = !_up; }
	bool turned() const { return _up; }
	virtual void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		size_t delta = _speed;
		if ( delta > 12 )
			delta = 12;
		_y = _up ? _y - delta : _y + delta;
	}
private:
	bool _up;
};

//-------------------------------------------------------------------------------
class Missile : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Missile( int x_, int y_ ) :
		Object( O_MISSILE, x_, y_, 0, 20, 3 ),
		_ox( x_ ),
		_oy( y_ )
	{
		update();
	}
	virtual const char *start_sound() const { return "missile"; }
	virtual void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		_x += 15;
	}
	int dx() const { return _x - _ox; }
	bool exhausted() const { return dx() > 450; }
	virtual void draw()
	{
		Fl_Color c( FL_WHITE );
		if ( dx() > 250 )
			c = fl_darker( c );
		if ( dx() > 350 )
			c = fl_darker( c );
		fl_rectf( x(), y(), w(), h(), c );
#ifdef DEBUG_COLLISION
		fl_color(FL_YELLOW);
		Rect r(rect());
		fl_rect(r.x(), r.y(), r.w(), r.h());
#endif
	}
private:
	int _ox;
	int _oy;
};

//-------------------------------------------------------------------------------
class Bomb : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Bomb( int x_, int y_ ) :
		Inherited( O_BOMB, x_, y_, "bomb.gif" ),
		_dy( 10 )
	{
		update();
	}
	virtual const char *start_sound() const { return "bomb_f"; }
	virtual void update()
	{
		if ( G_paused ) return;
		_y += _dy;
		if ( _state < 5 )
			_x += 16;
		else if ( _state > 15 )
			_x -= 5;
		_x -= 3;
		_x -= ( _speed / 30 );
		if ( _state % 2)
			_dy++;
		_speed /= 2;
		Inherited::update();
	}
	virtual double timeout() const { return started() ? 0.05 : 0.1; }
private:
	int _dy;
};

//-------------------------------------------------------------------------------
class Phaser : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Phaser( int x_ = 0, int y_ = 0 ) :
		Inherited( O_PHASER, x_, y_, "phaser.gif" ),
		_max_height( -1 ),
		_bg_color( FL_GREEN ),
		_disabled( false )
	{
		_state = Random::Rand() % 400;
	}
	virtual void update()
	{
		if ( G_paused ) return;
		Inherited::update();
	}
	void max_height( int max_height_ )
	{
		_max_height = max_height_;
	}
	void bg_color( Fl_Color bg_color_ )
	{
		_bg_color = bg_color_;
	}
	void disable( bool disable_ = true )
	{
		_disabled = disable_;
	}
	void draw()
	{
		Inherited::draw();
		int state = _state % 40;
		if ( _disabled )
			state = 0;
		if ( state == 0 )
			image( "phaser.gif" );
		if ( state == 20 )
			image( "phaser_active.gif" );
		if ( state >= 36 )
		{
			if ( state == 36 )
				Audio::instance()->play( "phaser" );
			fl_color( fl_contrast( FL_BLUE, _bg_color ) );
			fl_line_style( FL_SOLID, 3 );
			fl_line( cx(), y(), cx(), _max_height );
			fl_line_style( 0 );
		}
	}
private:
	int _max_height;
	Fl_Color _bg_color;
	bool _disabled;
};

//-------------------------------------------------------------------------------
class AnimText : public Object
//-------------------------------------------------------------------------------
{
// Note: This is just a quick impl. for showing an amimated level name
//       and should probably be made more flexible. Also the text drawing
//       could be combined with the FltWin::drawText().
	typedef Object Inherited;
public:
	AnimText( int x_, int y_, int w_, const char *text_,
	          Fl_Color color_ = FL_YELLOW,
	          Fl_Color bgColor_ = FL_BLACK,
	          int maxSize_ = 30,
	          int minSize_ = 10,
	          bool once_ = true ) :
		Inherited( (ObjectType)0, x_, y_, 0, w_ ),
		_text( text_ ),
		_color( color_ ),
		_bgColor( bgColor_ ),
		_maxSize( maxSize_ ),
		_minSize( minSize_ ),
		_once( once_ ),
		_sz( _minSize ),
		_up( true ),
		_hold( 0 ),
		_done ( false )
	{
		update();
	}
	void color( Fl_Color c_ ) { _color = c_; }
	void bgColor( Fl_Color c_ ) { _bgColor = c_; }
	void draw()
	{
		if ( _done )
			return;
		fl_font( FL_HELVETICA_BOLD_ITALIC, _sz );
		int W = 0;
		int H = 0;
		fl_measure( _text.c_str(), W, H, 1 );
		fl_color( _bgColor );
		fl_draw( _text.c_str(), _x + 2, _y + 2, _w, H, FL_ALIGN_CENTER, 0, 1 );
		fl_color( _color );
		fl_draw( _text.c_str(), _x, _y, _w, H, FL_ALIGN_CENTER, 0, 1 );
		if ( W > _w - 30 )
			_hold = 1;
	}
	virtual void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		if ( _up )
		{
			if ( !_hold )
			{
				_sz++;
				if ( _sz > _maxSize )
					_hold = 1;
			}
			else
			{
				_hold++;
				if ( _hold > 15 )
					_up = false;
			}
		}
		else
		{
			_sz--;
			if ( _sz < _minSize )
			{
				if ( _once )
					_done = true;
				else
				{
					_hold = false;
					_up = true;
				}
				return;
			}
		}
	}
	bool done() const { return _done; }
	virtual double timeout() const { return started() ? 0.02 : 0.02; }
private:
	string _text;
	Fl_Color _color;
	Fl_Color _bgColor;
	int _maxSize;
	int _minSize;
	bool _once;
	int _sz;
	unsigned char _g;
	bool _up;
	int _hold;
	bool _done;
};

//-------------------------------------------------------------------------------
class TerrainPoint
//-------------------------------------------------------------------------------
{
public:
	TerrainPoint( int ground_level_, int sky_level_ = -1, int object_ = 0 ) :
		_ground_level( ground_level_ ),
		_sky_level( sky_level_ ),
		_object( object_ )
	{
	}
	int ground_level() const { return _ground_level; }
	int sky_level() const { return _sky_level; }
	void sky_level( int sky_level_ ) { _sky_level = sky_level_; }
	void ground_level( int ground_level_ ) { _ground_level = ground_level_; }
	void object( unsigned int object_ ) { _object = object_; }
	unsigned int object() const { return _object; }
	bool has_object( int type_ ) const { return _object & type_; }
	void has_object( int type_, bool has_ )
	{
		if ( has_ )
			_object |= type_;
		else
			_object &= ~type_;
	}
public:
	// extra data for color change 'object' (made public)
	// NOTE: should be wrapped, but it's not worth the effort.
	Fl_Color bg_color;
	Fl_Color ground_color;
	Fl_Color sky_color;
private:
	int _ground_level;
	int _sky_level;
	unsigned int _object;
};

//-------------------------------------------------------------------------------
class Terrain : public vector<TerrainPoint>
//-------------------------------------------------------------------------------
{
	typedef vector<TerrainPoint> Inherited;
public:
	enum Flags
	{
	   NO_SCROLLIN_ZONE = 1,
	   NO_SCROLLOUT_ZONE = 2
	};
	Terrain() :
		Inherited()
	{
		init();
	}
	void clear()
	{
		Inherited::clear();
		init();
	}
	bool hasColorChange() const
	{
		for ( size_t i = 0; i < size(); i++ )
		{
			if ( at(i).has_object( O_COLOR_CHANGE ) )
				return true;
		}
		return false;
	}
	bool hasSky()
	{
		check();
		return has_sky;
	}
	void init()
	{
		bg_color = FL_BLUE;
		ground_color = FL_GREEN;
		sky_color = FL_DARK_GREEN;
		alt_bg_colors.clear();
		alt_ground_colors.clear();
		alt_sky_colors.clear();
		ls_outline_width = 0;
		outline_color_sky = FL_BLACK;
		outline_color_ground = FL_BLACK;
		flags = 0;
		first_check = true;
		has_sky = false;
		min_sky = INT_MAX;
		min_ground = INT_MAX;
		max_sky = 0;
		max_ground = 0;
	}
	void check()
	{
		if ( !first_check )
			return;
		first_check = empty();
		has_sky = false;
		has_sky = false;
		min_sky = INT_MAX;
		min_ground = INT_MAX;
		max_sky = 0;
		max_ground = 0;
		for ( size_t i = 0; i < size(); i++ )	// loop through whole level
		{
			int g = at(i).ground_level();
			int s = at(i).sky_level();
			if ( s > 0 )
				has_sky = true;
			if ( s < min_sky )
				min_sky = s;
			if ( s > max_sky )
				max_sky = s;
			if ( g < min_ground )
				min_ground = s;
			if ( g > max_ground )
				max_ground = g;
		}
		if ( max_sky < 0 )
			max_sky = 0;
		if ( max_ground < 0 )
			max_ground = 0;
	}
public:
	Fl_Color bg_color;
	Fl_Color ground_color;
	Fl_Color sky_color;
	vector<Fl_Color> alt_bg_colors;
	vector<Fl_Color> alt_ground_colors;
	vector<Fl_Color> alt_sky_colors;
	unsigned  ls_outline_width;
	Fl_Color outline_color_sky;
	Fl_Color outline_color_ground;
	unsigned long flags;
	bool first_check;
	bool has_sky;
	int min_sky;
	int min_ground;
	int max_sky;
	int max_ground;
};

//-------------------------------------------------------------------------------
class Spaceship : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Spaceship( int x_, int y_, int W_, int H_, bool alt_ = false, bool effects_ = false ) :
		Inherited( O_SHIP, x_, y_, alt_ ? "pene.gif" : "spaceship.gif" ),
		_W( W_ ),
		_H( H_ ),
		_effects( effects_ ),
		_missilePos( missilePos() ),
		_bombPos( bombPos() ),
		_accel( 0 ),
		_decel( 0 )
	{
		if ( effects_ )
			start();
	}
	void left()
	{
		if ( _x > 0 )
		{
			_decel = 2;
			_x -= _DX;
		}
	}
	void right()
	{
		if ( _x < _W / 2 )
		{
			_accel = 3;
			_x += _DX;
		}
	}
	void up()
	{
		if ( _y > _h / 2 )
			_y -= _DX;
	}
	void down()
	{
		if ( _y < _H )
			_y += _DX;
	}
	virtual void draw()
	{
		Inherited::draw();
		if ( _effects && ( _accel || _decel ) && !G_paused )
		{
			fl_color( _accel > _decel ? FL_GRAY : FL_DARK_MAGENTA );
			fl_line_style( FL_DASH, 1 );
			int y0 = y() + 20;
			int l = 20;
			int x0 = x() + Random::pRand() % 3;
			while ( y0 < y() + h() - 10 )
			{
				fl_xyline( x0, y0, x0 + l );
				y0 += 8;
				x0 += 2;
				l += 8;
			}
			fl_line_style( 0 );
		}
	}
	const Point& missilePoint() const { return _missilePos; }
	const Point& bombPoint() const { return _bombPos; }
protected:
	virtual void update()
	{
		if ( G_paused ) return;
		if ( _accel > 0 )
			_accel--;
		if ( _decel > 0 )
			_decel--;
	}
	Point missilePos()
	{
		Point p( w(), h() / 2 );
		// search rightmost row
		for ( int x = w() - 1; x >= 0; x-- )
		{
			int ty = 0;
			while ( ty < h() && isTransparent( x, ty ) ) ty++;
			if ( ty >= h() ) continue;
			// rightmost row found, search for center
			int by = h() - 1;
			while ( by >= 0 && isTransparent( x, by ) ) by--;
			p.x = x;
			p.y = ty + ( by - ty ) / 2;
			break;
		}
		return p;
	}
	Point bombPos()
	{
		Point p( w() / 2, h() );
		// search bottom line
		for ( int y = h() - 1; y >= 0; y-- )
		{
			int lx = 0;
			while ( lx < w() && isTransparent( lx, y ) ) lx++;
			if ( lx >= w() ) continue;
			// bottom line found, search for center
			int rx = w() - 1;
			while ( rx >= 0 && isTransparent( rx, y ) ) rx--;
			p.x = lx + ( rx - lx ) / 2;
			p.y = y;
			break;
		}
		return p;
	}
private:
	int _W, _H;
	bool _effects;
	Point _missilePos;
	Point _bombPos;
	int _accel;
	int _decel;
};

//-------------------------------------------------------------------------------
struct DemoDataItem
//-------------------------------------------------------------------------------
{
	DemoDataItem() :
		sx( 0 ), sy( 0 ), bomb( false ), missile( false ) {}
	DemoDataItem( int sx_, int sy_, bool bomb_, bool missile_ ) :
		sx( sx_ ), sy( sy_ ), bomb( bomb_ ), missile( missile_ ) {}
	int sx;
	int sy;
	bool bomb;
	bool missile;
};

//-------------------------------------------------------------------------------
class DemoData
//-------------------------------------------------------------------------------
{
public:
	DemoData() : _seed( 1 ), _alternate_ship( false ), _user_completed( false ) {}
	void setShip( unsigned index_, int sx_, int sy_ )
	{
		while ( _data.size() <= index_ )
			_data.push_back( DemoDataItem() );
		_data[ index_ ].sx = sx_;
		_data[ index_ ].sy = sy_;
	}
	void setBomb( unsigned index_ )
	{
		while ( _data.size() <= index_ )
			_data.push_back( DemoDataItem() );
		_data[ index_ ].bomb = true;
	}
	void setMissile( unsigned index_ )
	{
		while ( _data.size() <= index_ )
			_data.push_back( DemoDataItem() );
		_data[ index_ ].missile = true;
	}
	void set( unsigned index_, int sx_, int sy_, bool bomb_, bool missile_ )
	{
		DemoDataItem item( sx_, sy_, bomb_, missile_ );
		while ( _data.size() <= index_ )
			_data.push_back( item );
	}
	void seed( unsigned seed_ )
	{
		_seed = seed_;
	}
	void alternate_ship( unsigned alternate_ship_ )
	{
		_alternate_ship = alternate_ship_;
	}
	void user_completed( unsigned user_completed_ )
	{
		_user_completed = user_completed_;
	}
	bool getShip( unsigned index_, int &sx_, int &sy_ ) const
	{
		if ( _data.size() > index_ )
		{
			sx_ = _data[ index_ ].sx;
			sy_ = _data[ index_ ].sy;
			return true;
		}
		return false;
	}
	bool getBomb( unsigned index_, bool& bomb_ ) const
	{
		if ( _data.size() > index_ )
		{
			bomb_ = _data[ index_ ].bomb;
			return true;
		}
		return false;
	}
	bool getMissile( unsigned index_, bool& missile_ ) const
	{
		if ( _data.size() > index_ )
		{
			missile_ = _data[ index_ ].missile;
			return true;
		}
		return false;
	}
	bool get( unsigned index_, int &sx_, int &sy_, bool& bomb_, bool& missile_ ) const
	{
		if ( _data.size() > index_ )
		{
			sx_ = _data[ index_ ].sx;
			sy_ = _data[ index_ ].sy;
			bomb_ = _data[ index_ ].bomb;
			missile_ = _data[ index_ ].missile;
			return true;
		}
		return false;
	}
	unsigned seed() const
	{
		return _seed;
	}
	bool alternate_ship() const
	{
		return _alternate_ship;
	}
	bool user_completed() const
	{
		return _user_completed;
	}
	void clear()
	{
		_seed = 1;
		_data.clear();
	}
	size_t size() const
	{
		return _data.size();
	}
private:
	unsigned int _seed;
	bool _alternate_ship;
	bool _user_completed;
	vector<DemoDataItem> _data;
};

//-------------------------------------------------------------------------------
class FltWin : public Fl_Double_Window
//-------------------------------------------------------------------------------
{
#define DEFAULT_USER "N.N."
	typedef Fl_Double_Window Inherited;
public:
	enum State
	{
	   NEXT_STATE = -1,
	   START = 0,
	   TITLE,
	   DEMO,
	   LEVEL,
	   LEVEL_DONE,
	   LEVEL_FAIL,
	   PAUSED,
	   SCORE
	};
	FltWin( int argc_ = 0, const char *argv_[] = 0 );
	int run();
	bool trainMode() const { return _trainMode; }
	bool isFullscreen() const { return fullscreen_active(); }
	bool gimmicks() const { return _gimmicks; }
private:
	void add_score( unsigned score_ );
	void position_spaceship();
	void addScrollinZone();
	void addScrolloutZone();

	State changeState( State toState_ = NEXT_STATE );

	bool iniValue( size_t level_, const string& id_,
	               int &value_, int min_, int max_, int default_ );
	bool iniValue( size_t level_, const string& id_,
	               string& value_, size_t max_, const string& default_ );
	void init_parameter();
	bool loadLevel( int level_, string& levelFileName_ );
	bool validDemoData( unsigned level_ = 0 ) const;
	unsigned pickRandomDemoLevel( unsigned minLevel_ = 0, unsigned maxLevel_ = 0 );
	string demoFileName( unsigned  level_ = 0 ) const;
	bool loadDemoData();
	bool saveDemoData();
	bool collision( const Object& o_, int w_, int h_ );

	void create_spaceship();
	void create_objects();
	void delete_objects();

	void check_bomb_hits();
	void check_drop_hits();
	void check_missile_hits();
	void check_rocket_hits();
	void check_hits();

	bool correctDX();

#ifndef NO_PREBUILD_LANDSCAPE
	Fl_Image *terrain_as_image();
#endif
	bool create_terrain();
	void create_level();

	void draw_badies();
	void draw_bombs();
	void draw_cumuluses();
	void draw_drops();
	void draw_missiles();
	void draw_phasers();
	void draw_radars();
	void draw_rockets();
	void draw_objects( bool pre_ );

	int drawText( int x_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const;
	int drawTable( int w_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const;
	void draw_score();
	void draw_scores();
	void draw_title();
	void draw_shaded_landscape( int xoff_, int W_ );
	void draw_landscape( int xoff_, int W_ );
	bool draw_decoration();
	void draw();

	string firstTimeSetup();

	void update_badies();
	void update_bombs();
	void update_cumuluses();
	void update_drops();
	void update_missiles();
	void update_phasers();
	void update_radars();
	void update_rockets();
	void update_objects();

	bool dropBomb();
	bool fireMissile();
	int handle( int e_ );
	void keyClick() const;

	void onActionKey();
	void onCollision();
	void onPaused();
	void onContinued();
	void onDemo();
	void onLostFocus();
	void onGotFocus();
	void onNextScreen( bool fromBegin_ = false );
	void onTitleScreen();
	void onStateChange( State from_state_ );
	void onUpdate();
	void onUpdateDemo();

	void setTitle();
	void setUser();
	void resetUser();
	bool toggleFullscreen();
	bool setFullscreen( bool fullscreen_ );
	void toggleShip( bool prev_ = false );
	void toggleUser( bool prev_ = false );

	bool paused() const { return _state == PAUSED; }
	void bombUnlock();
	static void cb_bomb_unlock( void *d_ );
	static void cb_demo( void *d_ );
	static void cb_paused( void *d_ );
	static void cb_update( void *d_ );
	State state() const { return _state; }
	bool alternate_ship() const { return _state == DEMO ? _demoData.alternate_ship() : _alternate_ship; }
	bool user_completed() const { return _state == DEMO ? _demoData.user_completed() : _user.completed; }

	void setPaused( bool paused_ );
	void toggleBgSound() const;
	void toggleSound() const;
	void togglePaused();
	void setBgSoundFile();
	void startBgSound() const;
protected:
	Terrain T;
	Terrain TBG;
	vector<Missile *> Missiles;
	vector<Bomb *> Bombs;
	vector<Rocket *> Rockets;
	vector<Phaser *> Phasers;
	vector<Radar *> Radars;
	vector<Drop *> Drops;
	vector<Bady *> Badies;
	vector<Cumulus *> Cumuluses;

	int _rocket_start_prob;
	int _rocket_radar_start_prob;
	int _rocket_min_start_speed;
	int _rocket_max_start_speed;
	int _rocket_max_start_dist;
	int _rocket_min_start_dist;
	int _rocket_var_start_dist;

	int _drop_start_prob;
	int _drop_min_start_speed;
	int _drop_max_start_speed;
	int _drop_max_start_dist;
	int _drop_min_start_dist;
	int _drop_var_start_dist;

	int _bady_min_start_speed;
	int _bady_max_start_speed;

	int _bady_min_hits;
	int _bady_max_hits;

	int _cumulus_min_start_speed;
	int _cumulus_max_start_speed;
private:
	State _state;
	int _xoff;
	int _ship_delay;
	int _ship_voff;
	int _ship_y;
	bool _left, _right, _up, _down;
	bool _alternate_ship;
	Spaceship *_spaceship;
	Rocket _rocket;
	Phaser _phaser;
	Radar _radar;
	Drop _drop;
	Bady _bady;
	Cumulus _cumulus;
	bool _bomb_lock;
	bool _collision;	// level ended with collision
	bool _done;			// level ended by finishing
	unsigned _frame;
	unsigned _hiscore;
	string _hiscore_user;
	unsigned _first_level;
	unsigned _level;
	bool _completed;
	unsigned _done_level;
	unsigned _level_repeat;
	bool _cheatMode;
	bool _trainMode;	// started with level specification
	bool _mouseMode;
	bool _gimmicks;
	int _effects;
	bool _correct_speed;
	bool _no_demo;
	bool _no_position;
	string _levelFile;
	string _input;
	Cfg *_cfg;
	unsigned _speed_right;
	bool _internal_levels;
	bool _enable_boss_key; // ESC
	bool _focus_out;
	bool _no_random;
	bool _hide_cursor;
	string _title;
	string _level_name;
	vector<string> _ini;	// ini section of level file
	AnimText *_anim_text;
	AnimText *_title_anim;
	string _bgsound;
	DemoData _demoData;
	Fl_Image *_terrain;
	User _user;
	static Waiter _waiter;
};

/*static*/ Waiter FltWin::_waiter;

//-------------------------------------------------------------------------------
// class FltWin : public Fl_Double_Window
//-------------------------------------------------------------------------------
FltWin::FltWin( int argc_/* = 0*/, const char *argv_[]/* = 0*/ ) :
	Inherited( SCREEN_W, SCREEN_H, "FL-Trator v"VERSION ),
	_state( START ),
	_xoff( 0 ),
	_left( false), _right( false ), _up( false ), _down( false ),
	_alternate_ship( false ),
	_spaceship( 0 ),
	_bomb_lock( false),
	_collision( false ),
	_done( false ),
	_frame( 0 ),
	_hiscore( 0 ),
	_first_level( 1 ),
	_level( 0 ),
	_completed( false ),
	_done_level( 0 ),
	_level_repeat( 0 ),
	_cheatMode( false ),
	_trainMode( false ),
	_mouseMode( false ),
	_gimmicks( true ),
	_effects( 0 ),
	_correct_speed( false ),
	_no_demo( false ),
	_no_position( false ),
	_cfg( 0 ),
	_speed_right( 0 ),
	_internal_levels( false ),
	_enable_boss_key( false ),
	_focus_out( true ),
	_no_random( false ),
	_hide_cursor( false ),
	_title( label() ),
	_anim_text( 0 ),
	_title_anim( 0 ),
	_terrain( 0 )
{
	end();
	_DX = DX;
	int argc( argc_ );
	unsigned argi( 1 );
	bool reset_user( false );
	string unknown_option;
	string arg;
	bool usage( false );
	bool info( false );
	bool fullscreen( false );

	string defaultArgs;
	string cfgName( "fltrator" );
	if ( argc_ <= 1 )
	{
		_cfg = new Cfg( "CG", cfgName.c_str() );
		if ( argc_ <= 1 )
		{
			char *value = 0;
			int ret = _cfg->get( "defaultArgs", value, "" );
			if ( !ret || Fl::get_key( FL_Control_L ) )
			{
				defaultArgs = firstTimeSetup();
				_cfg->flush();
			}
			else
			{
				defaultArgs = value;
			}
			free( value );
		}
		delete _cfg;
		_cfg = 0;
	}

	while ( defaultArgs.size() || argc-- > 1 )
	{
		if ( unknown_option.size() )
			break;
		string arg( defaultArgs.size() ? defaultArgs : argv_[argi++] );
		defaultArgs.erase();
		if ( "--help" == arg || "-h" == arg )
		{
			usage = true;
			break;
		}
		if ( "--info" == arg )
		{
			info = true;
			break;
		}
		if ( "--version" == arg )
		{
			cout << VERSION << endl;
			exit( 0 );
		}
		int level = atoi( arg.c_str() );
		if ( level > 0 && level <= (int)MAX_LEVEL )
		{
			_first_level = level;
			_trainMode = true;
		}
		else if ( arg.find( "-A" ) == 0 )
		{
			Audio::instance()->cmd( arg.substr( 2 ) );
		}
		else if ( arg.find( "-U" ) == 0 )
		{
			_user.name = arg.substr( 2, 10 );
		}
		else if ( arg.find( "-R" ) == 0 )
		{
#ifdef WIN32
			setup( atoi( arg.substr( 2 ).c_str() ), false, false );
#else
			setup( atoi( arg.substr( 2 ).c_str() ), false, true );
#endif
		}
		else if ( arg[0] == '-' )
		{
			for ( size_t i = 1; i < arg.size(); i++ )
			{
				switch ( arg[i] )
				{
					case 'a':
						_alternate_ship = true;
						break;
					case 'b':
						Audio::instance()->bg_disable();
						break;
					case 'c':
						_hide_cursor = true;
						break;
					case 'C':
						_correct_speed = true;
						break;
					case 'd':
						_no_demo = true;
						break;
					case 'e':
						_enable_boss_key = true;
						break;
					case 'f':
						fullscreen = true;
						break;
					case 'l':
						G_leftHanded = true;
						break;
					case 'F':
						setup( -1, false, false );
						_gimmicks = true;
						_effects++;
						break;
					case 'i':
						_internal_levels = true;
						break;
					case 'm':
						_mouseMode = true;
						break;
					case 'n':
						reset_user = true;
						break;
					case 'o':
						_no_position = true;
						break;
					case 'p':
						_focus_out = false;
						break;
					case 'r':
						_no_random = true;
						break;
					case 's':
						Audio::instance()->disable();
						break;
					case 'x':
						_gimmicks = false;
						break;
					case 'S':
						setup( -1, true );
						_gimmicks = false;
						_effects = 0;
						break;
					default:
						unknown_option += arg[i];
						break;
				}
			}
		}
		else
		{
			_levelFile = arg;
			cout << "level file: '" << _levelFile.c_str() << "'" << endl;
		}
	}
	if ( unknown_option.size() )
	{
		cout << "Invalid option: '" << unknown_option.c_str() << "'" << endl << endl;
		usage = true;
	}
	if ( usage )
	{
		cout << "Usage:" << endl;
		cout << "  " << argv_[0] << " [level] [levelfile] [options]" << endl << endl;
		cout << "\tlevel      [1, 10] startlevel" << endl;
		cout << "\tlevelfile  name of levelfile (for testing a new level)" << endl;
		cout << "Options:" << endl;
		cout << "  -a\tuser alternate (penetrator like) ship image" << endl;
		cout << "  -b\tdisable background sound" << endl;
		cout << "  -c\thide mouse cursor in window" << endl;
		cout << "  -e\tenable Esc as boss key" << endl;
		cout << "  -d\tdisable auto demo" << endl;
		cout << "  -f\tenable full screen mode" << endl;
		cout << "  -h\tdisplay this help" << endl;
		cout << "  -i\tplay internal (auto-generated) levels" << endl;
		cout << "  -l\tleft handed play" << endl;
		cout << "  -n\treset user (only if startet with -U)" << endl;
		cout << "  -m\tuse mouse for input" << endl;
		cout << "  -o\tdo not position window" << endl;
		cout << "  -p\tdisable pause on focus out" << endl;
		cout << "  -r\tdisable ramdomness" << endl;
		cout << "  -s\tstart with sound disabled" << endl;
		cout << "  -x\tdisable gimmick effects" << endl;
		cout << "  -A\"playcmd\"\tspecify audio play command" << endl;
		cout << "   \te.g. -A\"cmd=playsound -q %f; bgcmd=playsound -q %f %p; ext=wav\"" << endl;
		cout << "  -C\tuse speed correction measurement" << endl;
		cout << "  -F{F}\trun with settings for fast computer (turns on most {all} features)" << endl;
		cout << "  -Rvalue\tset frame rate to 'value' fps [20,25,40,50,100,200]" << endl;
		cout << "  -S\trun with settings for slow computer (turns off gimmicks/features)" << endl;
		cout << "  -Uusername\tstart as user 'username'" << endl;
		cout << endl;
		cout << "  --help\tprint out this text and exit" << endl;
		cout << "  --info\tprint out some runtime information and exit" << endl;
		cout << "  --version\tprint out version  and exit" << endl;
		exit( 0 );
	}
	if ( info )
	{
		cout << "fltkWaitDelay = " << _waiter.fltkWaitDelay() << endl;
		cout << "USE_FLTK_RUN  = " <<  _USE_FLTK_RUN << endl;
		cout << "DX            = " << DX << endl;
		cout << "FRAMES        = " <<  FRAMES << endl;
		cout << "FPS           = " <<  FPS << endl;
		cout << "Audio::cmd    = " << Audio::instance()->cmd() << endl;
		cout << "Audio::bg_cmd = " << Audio::instance()->cmd( true ) << endl;
#if FLTK_HAS_NEW_FUNCTIONS
		cout << "FLTK_HAS_NEW_FUNCTIONS" << endl;
#endif
#ifdef NO_PREBUILD_LANDSCAPE
		cout << "NO_PREBUILD_LANDSCAPE" << endl;
#endif
		exit( 0 );
	}

	if ( _trainMode )
		_enable_boss_key = true; 	// otherwise no exit!
	else
		_levelFile.erase();

	create_spaceship();

	// set spaceship image as icon
#if FLTK_HAS_NEW_FUNCTIONS
	Fl_RGB_Image icon( (Fl_Pixmap *)_spaceship->image() );
	Fl_Window::default_icon( &icon );
#endif

	if ( _internal_levels )
		cfgName += "_internal";	// don't mix internal levels with real levels
	_cfg = new Cfg( "CG", cfgName.c_str() );
	if ( _user.name.empty() && !_trainMode )
		_user.name = _cfg->last_user();

	if ( _user.name.empty() && !_trainMode )
		_user.name = DEFAULT_USER;

	if ( !_trainMode && !reset_user )
		setUser();

	_level = _first_level;
	_hiscore = _cfg->hiscore();
	_hiscore_user = _cfg->best().name;
	wavPath.ext( Audio::instance()->ext() );

	Fl::visual( FL_DOUBLE | FL_RGB );

	setFullscreen( fullscreen );
	show();
}

void FltWin::onStateChange( State from_state_ )
//-------------------------------------------------------------------------------
{
	switch ( _state )
	{
		case TITLE:
			if ( !G_paused && _gimmicks && Fl::focus() == this )
				Audio::instance()->play( "menu" );
			Audio::instance()->stop_bg();
			onTitleScreen();
			break;
		case SCORE:
			Fl::remove_timeout( cb_demo, this );
			break;
		case LEVEL:
			onNextScreen( from_state_ != PAUSED );
			break;
		case DEMO:
			setPaused( false );
			onDemo();
			break;
		case LEVEL_FAIL:
			Audio::instance()->play( "ship_x" );
			Audio::instance()->stop_bg();
		case LEVEL_DONE:
		{
			if ( _state == LEVEL_DONE )
			{
				Audio::instance()->play ( "done" );
				Audio::instance()->stop_bg();
				_done_level = _level;
				_first_level = _done_level + 1;
				if ( _first_level > MAX_LEVEL )
					_first_level = 1;
				if ( !_trainMode )
				{
					_user.completed += _done_level == MAX_LEVEL;
					_user.level = _done_level;
					_cfg->writeUser( _user );
					_cfg->flush();
				}
			}
			double TO = 2.0;
			if ( _state == LEVEL_FAIL && _level_repeat + 1 > MAX_LEVEL_REPEAT )
				TO *= 2;
			if ( _state == LEVEL_DONE && _level == MAX_LEVEL )
				TO = 60.0;
			Fl::add_timeout( TO, cb_paused, this );
			changeState( PAUSED );
			break;
		}
		default:
			break;
	}
}

FltWin::State FltWin::changeState( State toState_/* = NEXT_STATE*/ )
//-------------------------------------------------------------------------------
{
	State state = _state;
	if ( toState_ == NEXT_STATE )
	{
		switch ( _state )
		{
			case START:
			case SCORE:
			case DEMO:
				setPaused( false );
				_state = _trainMode ? LEVEL : TITLE;
				break;

			case PAUSED:
				if ( G_paused )
				{
					Fl::add_timeout( 0.5, cb_paused, this );
					return _state;
				}
			case LEVEL_DONE:
			case LEVEL_FAIL:
//			case PAUSED:
				_state = LEVEL;
				break;

			case TITLE:
				_state = LEVEL;
				break;

			default:
				break;
		}
	}
	else
	{
		if ( toState_ == TITLE && _trainMode )
			toState_ = LEVEL;
		if ( _state != toState_ )
			_state = toState_;
	}
	if ( _state != state )
		onStateChange( state );
	return _state;
}

void FltWin::add_score( unsigned score_ )
//-------------------------------------------------------------------------------
{
	if ( _state != DEMO )
		_user.score += score_;
}

void FltWin::position_spaceship()
//-------------------------------------------------------------------------------
{
//	int X = _state == DEMO ? w() / 2 : _spaceship->w() + 10;
	int X = _spaceship->w() + 10;
	int ground = T[X].ground_level();
	int sky = T[X].sky_level();
	int Y = ground + ( h() - ground - sky ) / 2;
//	printf( "position_spaceship(): X=%d Y=%d h()-Y =%d sky=%d\n", X, Y, h() - Y, sky );
	_spaceship->cx( X );
	_spaceship->cy( h() - Y );
}

void FltWin::addScrollinZone()
//-------------------------------------------------------------------------------
{
	T.insert( T.begin(), T[0] );	// TODO: this makes size() an odd number
	T[0].object( 0 ); // ensure there are no objects in scrollin zone!
	for ( int i = 0; i < w() / 2; i++ )
		T.insert( T.begin(), T[0] );
}

void FltWin::addScrolloutZone()
//-------------------------------------------------------------------------------
{
	T.push_back( T.back() );
	T.back().object( 0 ); // ensure there are no objects in scrollout zone!
	int xLastObject = T.size() - 1;
	while ( xLastObject && ( !T[ xLastObject ].object() ||
	        T[ xLastObject].object() == O_COLOR_CHANGE ) )
		--xLastObject;
	if ( xLastObject )
		xLastObject += _bady.w() / 2;	// right edge of broadest enemy object
	size_t minTerrainWidth = xLastObject + w() + 1;
	for ( int i = 0; i < w() / 2 - 1; i++ )
	{
		if ( T.size() >= minTerrainWidth )
			break;
		T.push_back( T.back() );
	}
}

bool FltWin::collision( const Object& o_, int w_, int h_ )
//-------------------------------------------------------------------------------
{
	int xoff = 0;
	int yoff = 0;
	int X = o_.x();
	int Y = o_.y();
	int W = o_.w();
	int H = o_.h();

	if ( X < 0 )
	{
		xoff = -X;
		X = 0;
		W -= xoff;
	}
	if ( Y < 0 )
	{
		yoff = -Y;
		Y = 0;
		H -= yoff;
	}

	W = X - xoff + W < w_ ? W : w_ - ( X - xoff );
	H = Y - yoff + H < h_ ? H : h_ - ( Y - yoff );

	const uchar *screen = fl_read_image( 0, X, Y, W, H );

	const int d = 3;
	bool collided = false;

	// background color r/g/b
	unsigned char  R, G, B;
	Fl::get_color( T.bg_color, R, G, B );
//	printf( "check for BG-Color %X/%X/%X\n", R, G, B );

#ifdef DEBUG_COLLISION
	int xc = 0;
	int yc = 0;
#endif
	for ( int y = 0; y < H; y++ ) // lines
	{
		long index = y * W * d; // line start offset
		for ( int x = 0; x < W; x++ )
		{
			unsigned char r = *(screen + index++);
			unsigned char g = *(screen + index++);
			unsigned char b = *(screen + index++);

			if ( !o_.isTransparent( x + xoff, y + yoff ) &&
			     !( r == R && g == G && b == B ) )
			{
				collided = true;
#ifdef DEBUG_COLLISION
				xc = x;
				yc = y;
#endif
				break;
			}
		}
		if ( collided ) break;
	}
	delete[] screen;
#if DEBUG_COLLISION
	if ( collided )
	{
		printf( "collision at %d + %d / %d + %d !\n", o_.x(), xc, o_.y(), yc  );
		printf( "object %dx%d xoff=%d yoff=%d\n", o_.w(), o_.h(), xoff, yoff );
		printf( "X=%d, Y=%d, W=%d, H=%d\n", X, Y, W, H );
	}
#endif
	return collided;
} // collision

bool FltWin::iniValue( size_t level_, const string& id_,
                       int &value_, int min_, int max_, int default_ )
//-------------------------------------------------------------------------------
{
	bool fromFile( false );
	value_ = default_;

	if ( _internal_levels )
		return fromFile;

	if ( !_ini.size() )
		return fromFile;

	string line;
	for ( size_t i = 0; i < _ini.size(); i++ )
	{
		line = _ini[ i ];
		size_t pos;
		pos = line.find( id_ );
		if ( pos != string::npos )
		{
			pos += id_.size();
			pos = line.find( "=", pos );
			if ( pos != string::npos )
			{
				int value = atoi( line.substr( pos + 1 ).c_str() );
				value_ = rangedValue( value, min_, max_ );
//				printf( "%s = %d\n", id_.c_str(), value_ );
				fromFile = true;
				break;
			}
		}
	}
	return fromFile;
}

bool FltWin::iniValue( size_t level_, const string& id_,
                       string& value_, size_t max_, const string& default_ )
//-------------------------------------------------------------------------------
{
	bool fromFile( false );
	value_ = default_;

	if ( _internal_levels )
		return fromFile;

	if ( !_ini.size() )
		return fromFile;

	string line;
	for ( size_t i = 0; i < _ini.size(); i++ )
	{
		line = _ini[ i ];
		size_t pos;
		pos = line.find( id_ );
		if ( pos != string::npos )
		{
			pos += id_.size();
			pos = line.find( "=", pos );
			if ( pos != string::npos )
			{
				value_ = line.substr( pos + 1 );
				if ( value_.size() > max_ )
					value_.erase( max_ );
//				printf( "%s = '%s'\n", id_.c_str(), value_.c_str() );
				fromFile = true;
				break;
			}
		}
	}
	return fromFile;
}

void FltWin::init_parameter()
//-------------------------------------------------------------------------------
{
	ostringstream defLevelName;
	defLevelName << ( _internal_levels ? "Internal level" : "Level" )  << " " << _level;
	iniValue( _level, "level_name", _level_name, 35, defLevelName.str() );

	iniValue( _level, "rocket_start_prob", _rocket_start_prob, 0, 100,
	          (int)( 45. + 35. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ) ); // 45% - 80%
	iniValue( _level, "rocket_radar_start_prob", _rocket_radar_start_prob, 0, 100,
	          (int)( 60. + 40. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ) ); // 60% - 100%
	if ( iniValue( _level, "rocket_start_speed", _rocket_min_start_speed, 1, 10,
	               4 + _level / 2 ) )
	{
		_rocket_max_start_speed = _rocket_min_start_speed;
	}
	else
	{
		iniValue( _level, "rocket_min_start_speed", _rocket_min_start_speed, 1, 10,
		          4 + _level / 2 - 1 );
		iniValue( _level, "rocket_max_start_speed", _rocket_max_start_speed, 1, 10,
		          _rocket_min_start_speed + 2 );
	}
	iniValue( _level, "rocket_min_start_dist", _rocket_min_start_dist, 0, 50, 50 );
	iniValue( _level, "rocket_max_start_dist", _rocket_max_start_dist, 50, 400, 400 );
	iniValue( _level, "rocket_var_start_dist", _rocket_var_start_dist, 0, 200, 200 );
#ifdef DEBUG
	printf("rocket_min_start_speed: %d\n", _rocket_min_start_speed);
	printf("rocket_max_start_speed: %d\n", _rocket_max_start_speed);
	printf("rocket_start_prob     : %d\n", _rocket_start_prob);
	printf("rocket_min_start_dist : %d\n", _rocket_min_start_dist);
	printf("rocket_max_start_dist : %d\n", _rocket_max_start_dist);
	printf("rocket_var_start_dist : %d\n", _rocket_var_start_dist);
#endif

	iniValue( _level, "drop_start_prob", _drop_start_prob, 0, 100,
	          (int)( 45. + 30. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ) ); // 45% - 75%
	if ( iniValue( _level, "drop_start_speed", _drop_min_start_speed, 1, 10,
	               4 + _level / 2) )
	{
		_drop_max_start_speed = _drop_min_start_speed;
	}
	else
	{
		iniValue( _level, "drop_min_start_speed", _drop_min_start_speed, 1, 10,
		          4 + _level / 2 - 1 );
		iniValue( _level, "drop_max_start_speed", _drop_max_start_speed, 1, 10,
		          _drop_min_start_speed + 1 );
	}
	iniValue( _level, "drop_min_start_dist", _drop_min_start_dist, 0, 50, 50 );
	iniValue( _level, "drop_max_start_dist", _drop_max_start_dist, 50, 400, 400 );
	iniValue( _level, "drop_var_start_dist", _drop_var_start_dist, 0, 200, 200 );
#ifdef DEBUG
	printf("drop_min_start_speed: %d\n", _drop_min_start_speed);
	printf("drop_max_start_speed: %d\n", _drop_max_start_speed);
	printf("drop_start_prob     : %d\n", _drop_start_prob);
	printf("drop_min_start_dist : %d\n", _drop_min_start_dist);
	printf("drop_max_start_dist : %d\n", _drop_max_start_dist);
	printf("drop_var_start_dist : %d\n", _drop_var_start_dist);
#endif

	if ( iniValue( _level, "bady_start_speed", _bady_min_start_speed, 1, 10,
	               2 ) )
	{
		_bady_max_start_speed = _bady_min_start_speed;
	}
	else
	{
		iniValue( _level, "bady_min_start_speed", _bady_min_start_speed, 1, 10,
		          2 - 1 );
		iniValue( _level, "bady_max_start_speed", _bady_max_start_speed, 1, 10,
		          2 );
	}
#ifdef DEBUG
	printf( "_bady_min_start_speed: %d\n", _bady_min_start_speed );
	printf( "_bady_max_start_speed: %d\n", _bady_max_start_speed );
#endif

	if ( iniValue( _level, "bady_hits", _bady_min_hits, 1, 5,
	               5 ) )
	{
		_bady_max_hits = _bady_min_hits;
	}
	else
	{
		iniValue( _level, "bady_min_hits", _bady_min_hits, 1, 5,
		          5 );
		iniValue( _level, "bady_max_hits", _bady_max_hits, 1, 5,
		          5 );
	}
#ifdef DEBUG
	printf( "_bady_min_hits: %d\n", _bady_min_hits );
	printf( "_bady_max_hits: %d\n", _bady_max_hits );
#endif

	if ( iniValue( _level, "cumulus_start_speed", _cumulus_min_start_speed, 1, 10,
	               2 ) )
	{
		_cumulus_max_start_speed = _cumulus_min_start_speed;
	}
	else
	{
		iniValue( _level, "cumulus_min_start_speed", _cumulus_min_start_speed, 1, 10,
		          2 - 1 );
		iniValue( _level, "cumulus_max_start_speed", _cumulus_max_start_speed, 1, 10,
		          2 + 1 );
	}
#ifdef DEBUG
	printf( "_cumulus_min_start_speed: %d\n", _cumulus_min_start_speed );
	printf( "_cumulus_max_start_speed: %d\n", _cumulus_max_start_speed );
#endif
}

string FltWin::demoFileName( unsigned level_/* = 0*/ ) const
//-------------------------------------------------------------------------------
{
	ostringstream os;
	os << ( _internal_levels ? "di" : "d" ) << "_" << ( level_ ? level_ : _level );
	if ( 1 != DX )
		os << "_" << DX;
	os << ".txt";
	return demoPath( os.str() );
}

bool FltWin::validDemoData( unsigned level_/* = 0*/ ) const
//-------------------------------------------------------------------------------
{
	ifstream f( demoFileName( level_ ).c_str() );
	if ( !f.is_open() )
		return false;
	unsigned int seed;
	bool alternate_ship;
	unsigned int dx;

	f >> seed;
	f >> alternate_ship;
	f >> dx;
	seed = seed;
	alternate_ship = alternate_ship;

	if ( DX != dx )
		return false;
	return true;
}

unsigned FltWin::pickRandomDemoLevel( unsigned minLevel_/* = 0*/, unsigned maxLevel_/* = 0*/ )
//-------------------------------------------------------------------------------
{
//	printf( "pickRandomDemoLevel [%u, %u]", minLevel_, maxLevel_ );
	unsigned minLevel( minLevel_ ? minLevel_ : 1 );
	unsigned maxLevel( maxLevel_ ? maxLevel_ : MAX_LEVEL );
//	printf( " ==> [%u, %u]\n", minLevel, maxLevel );
	vector<unsigned> possibleLevels;
	for ( unsigned level = minLevel; level <= maxLevel; level++ )
	{
		if ( validDemoData( level ) )
		{
//			printf( "   found valid data for level %u\n", level );
			possibleLevels.push_back( level );
		}
	}
	if ( possibleLevels.size() )
		return possibleLevels[ Random::pRand() % possibleLevels.size() ];
	return minLevel;	// return a valid level even if there's no demo data
}

bool FltWin::loadDemoData()
//-------------------------------------------------------------------------------
{
	_demoData.clear();
	_demoData.alternate_ship( _alternate_ship );
	ifstream f( demoFileName().c_str() );
	if ( !f.is_open() )
		return false;
//	cout << "using demo data " <<  demoFileName().c_str() << endl;
	unsigned int seed;
	unsigned int dx;
	f >> seed;
	_demoData.seed( seed );
	bool alternate_ship;
	f >> alternate_ship;
	f >> dx;
	if ( DX != dx )
		return false;
	bool user_completed;
	f >> user_completed;
	_demoData.alternate_ship( alternate_ship );
	_demoData.user_completed( user_completed );
	int index = 0;
	while ( f.good() )
	{
		int sx, sy;
		bool bomb, missile;
		f >> sx;
		f >> sy;
		f >> bomb;
		f >> missile;
		if ( !f.good() ) break;
		while ( f.good() )
		{
			_demoData.set( index, sx, sy, bomb, missile );
			index++;
			int c = (f >> std::ws).peek(); // (peek one character skipping whitespace)
			if ( '*' != c ) break;
			f.get();
		}
	}
	return true;
}

bool FltWin::saveDemoData()
//-------------------------------------------------------------------------------
{
	ofstream f( demoFileName().c_str() );
	if ( !f.is_open() )
		return false;
//	cout << "save to demo data " << demoFileName().c_str() << endl;
	f << _demoData.seed() << endl;
	f << _alternate_ship << endl;
	f << DX << endl;
	f << _user.completed << endl;
	int index = 0;
	int osx( 0 ), osy( 0 );
	bool obomb( false ), omissile( false );
	while ( f.good() )
	{
		int sx, sy;
		bool bomb, missile;
		if ( !_demoData.get( index, sx, sy, bomb, missile ) )
			break;
		if ( index && sx == osx && sy == osy && obomb == bomb && omissile == missile )
			f << '*';	// data repeat
		else
			f << sx << " " << sy << " " << bomb << " " << missile << endl;
		osx = sx; osy = sy; obomb = bomb; omissile = missile;
		index++;
	}
	return true;
}

static bool readColors( ifstream& f_, Fl_Color& c_, vector<Fl_Color>& alt_ )
//-------------------------------------------------------------------------------
{
	string line;
	getline( f_, line );
	if ( line.empty() )
		getline( f_, line );
	stringstream is;
	is << line;
	is >> c_;
	while ( !is.fail() )
	{
		Fl_Color c;
		is >> c;
		if ( !is.fail() )
			alt_.push_back( c );
	}
	return f_.good();
}

bool FltWin::loadLevel( int level_, string& levelFileName_ )
//-------------------------------------------------------------------------------
{
	string levelFileName( levelFileName_ );
	if ( levelFileName_.empty() )
	{
		ostringstream os;
		os << "L_" << level_ << ".txt";
		levelFileName = levelPath( os.str() );
		levelFileName_ = levelFileName;
	}
	ifstream f( levelFileName.c_str() );
	if ( !f.is_open() )
		return false;
//	printf( "reading level %d from %s\n", level_, levelFileName.c_str() );
	unsigned long version;
	f >> version;
	f >> T.flags;
	if ( version )
	{
		// new format
		f >> T.ls_outline_width;
		f >> T.outline_color_ground;
		f >> T.outline_color_sky;
	}

	readColors( f, T.bg_color, T.alt_bg_colors );
	readColors( f, T.ground_color, T.alt_ground_colors );
	readColors( f, T.sky_color, T.alt_sky_colors );

	while ( f.good() )	// access data is ignored!
	{
		int s, g;
		int obj;
		f >> s;
		f >> g;
		f >> obj;
		if ( !f.good() ) break;
		T.push_back( TerrainPoint( g, s, obj ) );
		if ( obj & O_COLOR_CHANGE )
		{
			// fetch extra data for color change object
			Fl_Color bg_color, ground_color, sky_color;
			f >> bg_color;
			f >> ground_color;
			f >> sky_color;
			if ( f.good() )
			{
				T.back().bg_color = bg_color;
				T.back().ground_color = ground_color;
				T.back().sky_color = sky_color;
			}
		}
	}
	f.clear(); 	// reset for reading of ini section
	string line;
	while ( getline( f, line ) )
	{
		_ini.push_back( line );
	}
	return true;
}

#ifndef NO_PREBUILD_LANDSCAPE
Fl_Image *FltWin::terrain_as_image()
//-------------------------------------------------------------------------------
{
	int W = T.size();
	int H = h();
	Fl_Image_Surface *img_surf = new Fl_Image_Surface( W, H );
	assert( img_surf );
	img_surf->set_current();

	// TODO: use same code as in draw()

	// draw bg
	fl_rectf( 0, 0, W, h(), T.bg_color );

	// draw landscape
	draw_landscape( 0, W );

	Fl_RGB_Image *image = img_surf->image();
	delete img_surf;
	Fl_Display_Device::display_device()->set_current(); // direct graphics requests back to the display
	return image;
}
#endif

static void fixColorChange( Terrain &T_, bool correct_speed_ )
//-------------------------------------------------------------------------------
{
	if ( DX == 1 && !correct_speed_ )
		return;
	int dx( correct_speed_ ? 10 : DX );
	for ( size_t i = 0; i < T_.size(); i++ )
	{
		if ( T_[ i ].object() & O_COLOR_CHANGE )
		{
			bool change( false );
			size_t e = ( ( i + dx ) / dx ) * dx;
//			printf( "fix range [%lu, %lu]\n", (unsigned long)i, (unsigned long)e );
			if ( e > T_.size() )
				e = T_.size() - 1;
			for ( size_t j = i + 1; j <= e; j++ )
			{
				if ( T_[ j ].object() & O_COLOR_CHANGE )
				{
					change = true;
//					printf( "interrupted by another cc object at %lu\n", (unsigned long)j );
					i = j - 1;
					break;
				}
				T_[ j ].object( T_[ j ].object() | O_COLOR_CHANGE );
				T_[ j ].sky_color = T_[ i ].sky_color;
				T_[ j ].ground_color = T_[ i ].ground_color;
				T_[ j ].bg_color = T_[ i ].bg_color;
			}
			if ( !change )
				i = e;
		}
	}
}

bool FltWin::create_terrain()
//-------------------------------------------------------------------------------
{
	assert( _level > 0 && _level <= MAX_LEVEL );
	static unsigned int seed = 0;
	static unsigned lastCachedTerrainLevel = 0;
	static Terrain TC;

	if ( _level != lastCachedTerrainLevel )
	{
		_ini.clear();
		T.clear();
		seed = 0;
	}
	else
	{
		T = TC;
	}

	// set level randomness
	if ( _state == DEMO )
	{
		seed = _demoData.seed();
	}
	else
	{
		if ( _no_random )
			seed = _level * 10000;	// always the same sequences/objects
		else
		{
			if ( !seed )
				seed = time( 0 );	// random sequences/objects
		}
		if ( _state == LEVEL )
		{
			_demoData.clear();
			_demoData.seed( seed );
		}
	}
//	cout << "seed #" << _level << ": " << seed << endl;
	Random::Srand( seed );

	_cheatMode = getenv( "FLTRATOR_CHEATMODE" );
	_user.score = _cfg->readUser( _user.name ).score;
	wavPath.level( _level );
	imgPath.level( _level );

	string levelFile( _levelFile );
	bool loaded( false );
	errno = 0;
	if ( _internal_levels )
	{
		// internal level (always create, to keep random generator in sync)
		T.clear();
		create_level();
		loaded = (int)T.size() > w();
	}
	else if ( T.empty() )
	{
		loaded = loadLevel( _level, levelFile );	// try to load level from landscape file
		if ( loaded && user_completed() )
		{
			// TODO: just a test to use different colors dep. on completed state
			if ( T.alt_bg_colors.size() )
			{
				T.bg_color = T.alt_bg_colors[0];
			}
		}
	}
	if ( (int)T.size() < w() )
	{
		string err( T.empty() && errno ? strerror( errno ) : "File is not a valid level file" );
		cerr << "Failed to load level file " << levelFile << ": " << err.c_str() << endl;
		return false;
	}
	else if ( !_internal_levels && loaded )
	{
		if ( !( T.flags & Terrain::NO_SCROLLIN_ZONE ) )
			addScrollinZone();
		if ( !( T.flags & Terrain::NO_SCROLLOUT_ZONE ) )
			addScrolloutZone();
	}
#ifndef NO_PREBUILD_LANDSCAPE
	if ( _level != lastCachedTerrainLevel || !_terrain )
	{
		delete _terrain;
		// terrains with color change cannot be prebuild as image!
		_terrain = T.hasColorChange() ? 0 : terrain_as_image();
	}
#endif
	if ( lastCachedTerrainLevel != _level )
	{
		lastCachedTerrainLevel = _level;
		fixColorChange( T, _correct_speed );
		TC = T;
	}
	TBG.clear();
	T.check();
	if ( _effects )
	{
		// currently levels without sky draw mountains in background
		if ( !T.hasSky() )
		{
			for ( size_t i = 0; i < T.size(); i++ )
				TBG.push_back( T[T.size() - i - 1] );
			TBG.flags = 1;	// use as marker for "has mountain plane"
		}
		// and levels with dark sky draw a starfield...
		unsigned char r, g, b;
		Fl::get_color( T.bg_color, r, g, b );
		int n = ( r < 0x50 )+ ( g < 0x50 ) + ( b < 0x50 );
		// ... but only if they have a dark or blue background color
		if ( n > 2 || ( ( n >= 2 ) && b >= 0x50 ) )
		{
			if ( TBG.empty() )
			{
				for ( size_t i = 0; i < T.size(); i++ )
					TBG.push_back( TerrainPoint( 0, 0 ) );
			}
			else
			{
				for ( size_t i = 0; i < TBG.size(); i++ )
					TBG[i].sky_level( 0 );
			}
			for ( size_t i = 0; i < TBG.size() / 10; i++ )
			{
				int x = Random::pRand() % TBG.size();
				int range = h() - 10;
				int y = Random::pRand() % range + 5;
				TBG[x].sky_level( y );	// store y in sky-level
			}
			TBG.flags |= 2;	// use as marker for "has starfield plane"
		}
	}

	// initialise the objects parameters (eventuall read from level file)
	init_parameter();

	setTitle();

	return true;
}

void FltWin::create_level()
//-------------------------------------------------------------------------------
{
	struct terrain_data
	{
		bool sky;
		bool edgy;
		Fl_Color ground_color;
		Fl_Color bg_color;
		Fl_Color sky_color;
		int outline_width;
		Fl_Color outline_color_ground;
		Fl_Color outline_color_sky;
		int rockets;
		int radars;
		int drops;
		int badies;
		int cumulus;
		int phasers;
	};
	static struct terrain_data level_colors[] = {
		{ /*1*/false, false, FL_GREEN, FL_BLUE, FL_GREEN, 0, FL_BLACK, FL_BLACK,
			30, 20, 0, 0, 0, 0 },
		{ /*2*/true, false, fl_rgb_color( 241, 132, 0 ), fl_rgb_color( 97, 47, 4 ), fl_rgb_color( 119, 13, 6 ), 3, FL_BLACK, FL_BLACK,
			20, 20, 25, 0, 0, 0 },
		{ /*3*/false, false, FL_RED, FL_GRAY, FL_RED, 2, FL_WHITE, FL_WHITE,
			30, 15, 0, 2, 0, 0 },
		{ /*4*/true, false, FL_DARK_GREEN, FL_DARK_CYAN, FL_DARK_YELLOW, 1, FL_BLACK, FL_YELLOW,
			30, 10, 20, 1, 0, 0 },
		{ /*5*/false, true, FL_BLACK, FL_MAGENTA, FL_BLACK, 4, FL_RED, FL_RED,
			30, 20, 0, 3, 0, 0 },
		{ /*6*/false, false, FL_DARK_RED, FL_DARK_GREEN, FL_DARK_RED, 0, FL_YELLOW, FL_YELLOW,
			30, 20, 0, 2, 1, 0 },
		{ /*7*/true, true, FL_BLACK, FL_BLACK, FL_BLACK, 4, FL_GRAY, FL_CYAN,
			20, 15, 20, 1, 0, 0 },
		{ /*8*/false, false, FL_WHITE, FL_DARK_BLUE, FL_WHITE, 4, FL_RED, FL_CYAN,
			30, 20, 0, 4, 2, 0 },
		{ /*9*/true, false, FL_DARK_BLUE, FL_DARK_BLUE, FL_DARK_BLUE, 2, FL_BLUE, FL_CYAN,
			30, 15, 20, 0, 1, 2 },
		{ /*10*/true, false, FL_BLACK, fl_lighter( FL_YELLOW ), FL_DARK_RED, 2, FL_BLACK, FL_WHITE,
			30, 10, 20, 2, 2, 4 }
	};

#define ANZ_LEVEL_COLOR_SETS (sizeof(level_colors) / sizeof(level_colors[0]) )
	assert( ANZ_LEVEL_COLOR_SETS >= MAX_LEVEL );

	int set = ( _level - 1 ) % ANZ_LEVEL_COLOR_SETS;
	bool sky =level_colors[set].sky;
	T.ground_color = level_colors[set].ground_color;
	T.bg_color = level_colors[set].bg_color;
	T.sky_color = level_colors[set].sky_color;
	bool edgy = level_colors[set].edgy;
	T.ls_outline_width = level_colors[set].outline_width;
	T.outline_color_ground = level_colors[set].outline_color_ground;
	T.outline_color_sky = level_colors[set].outline_color_sky;
	int rockets = level_colors[set].rockets;
	int radars = level_colors[set].radars;
	int drops = level_colors[set].drops;
	int badies = level_colors[set].badies;
	int cumulus = level_colors[set].cumulus;
	int phasers = level_colors[set].phasers;

//	printf( "create level(): _level: %u _level_repeat: %u, sky = %d\n",
//		_level, _level_repeat, sky );

	int range = ( h() * ( sky ? 3 : 4 ) ) / 5; // use 3/5 (with sky) or 4/5 ( w/o sky) of height
	int bott = h() / 5;	// default ground level
	static const size_t INTERNAL_TERRAIN_SIZE( 9 * SCREEN_W );
	while ( T.size() < INTERNAL_TERRAIN_SIZE )
	{
		int r = T.empty() ? range / 2 : range;	// don't start with a high ground
		int peak = Random::Rand() % ( r / 2 )  + r / 2;
		int j;

		int peak_dist = ( Random::Rand() % ( peak * 2 ) ) + peak / 4 ;
		int bott1 = T.size() ? T.back().ground_level() : bott;
		if ( !edgy )
		{
			for ( j = 0; j < peak_dist; j++ )
			{
				int dy =  ( (double)j / peak_dist ) * (double)( peak - bott1 );
				T.push_back( TerrainPoint( dy + bott1 ) );
			}
		}
		else
			bott1 = peak;

		int flat = Random::Rand() % ( w() / 2 ) + ( edgy * w() / 5 );

		if ( !edgy )
		{
			if ( Random::Rand() % 3 == 0 )
				flat = Random::Rand() % 10;
		}

		for ( j = 0; j < flat; j++ )
			T.push_back( edgy ? bott1 : T.back() );

		int bott2 = bott + ( Random::Rand() % bott ) - bott / 2;
		if ( bott2 > peak - 20 )
			bott2 = peak - 20;

		if ( !edgy )
		{
			for ( j = peak_dist - 1; j >= 0; j-- )
			{
				int dy =  ( (double)j / peak_dist ) * (double)( peak - bott2 );
				T.push_back( TerrainPoint( dy + bott2 ) );
			}
		}
	}

	if ( sky )
	{
		range = h() / 5;
		int top = h() / 12;
		for ( size_t i = 0; i < T.size(); i++ )
		{
			int ground = T[i].ground_level();
			int free = h() - ground;
			int sky = (int)( (double)top * (double)free / (double)h() );
			sky += _level * 10;
			if ( h() - ground - sky < _spaceship->h() + 20 )
				sky = h() - ground - _spaceship->h() - 20;
			if ( sky  < -1 || sky >= h() )
			{
//				printf( "sky: %d free: %d ground: %d top: %d:\n", sky, free, ground, top );
				assert( sky  >= -1 && sky < h() );
			}
			T[i].sky_level( sky );
		}
	}
	//
	// Setup object positions
	//
	int min_dist = 100 - _level * 5;
	int max_dist = 200 - _level * 6;
	if ( min_dist < _rocket.w() )
		min_dist = _rocket.w();
	if ( max_dist < _rocket.w() * 3 )
		max_dist = _rocket.w() * 3;


//	printf( "min_dist: %d\n", min_dist );
//	printf( "max_dist: %d\n", max_dist );

	int last_x = Random::Rand() % max_dist + min_dist;
	int o = 0;
	int retry = 3;	// retries if no suitable place for object here
	string objects;
	while ( last_x < (int)T.size() )
	{
		if ( objects.empty() )
		{
			// (re-)build shuffled object pool
			objects = string( rockets, 'r' ) + string( radars, 'a' ) +
				string( drops, 'd' ) + string( badies, 'b' ) +
				string( cumulus, 'c' ) + string( phasers, 'p' );
			for ( size_t i = 0; i < objects.size() * 10; i++ )
			{
				objects.insert( objects.begin() + Random::pRand() % objects.size(), objects[0] );
				objects.erase( 0, 1 );
			}
		}

		int g = T[last_x].ground_level();
		if ( h() - g > ( 100 - (int)_level * 10 ) )
		{
			bool flat( true );
			for ( int x = last_x -_rocket.w() / 2; x < last_x + _rocket.w() / 2; x++ )
			{
				if ( T[x].ground_level() > g + 3 || T[x].ground_level() < g - 3 )
				{
					flat = false;
					break;
				}
			}
//			printf( "last_x: %d flat: %d\n", last_x, flat );
			// pick an object if needed
			if ( !o )
			{
				int index = Random::pRand() % objects.size();
				o = objects[ index ];
				objects.erase( index, 1 );
			}
			switch ( o )
			{
				case 'r':
					if ( ( edgy && flat ) || !edgy )
						T[last_x].object( O_ROCKET );
					break;
				case 'a':
					if ( flat )
						T[last_x].object( O_RADAR );
					break;
				case 'b':
					T[last_x].object( O_BADY );
					break;
				case 'd':
					T[last_x].object( O_DROP );
					break;
				case 'c':
					T[last_x].object( O_CUMULUS );
					break;
				case 'p':
					if ( flat )
						T[last_x].object( O_PHASER );
					break;
			}
		}
		else
		{
//			printf( "too low for objects\n" );
		}

		if ( !T[last_x].object() && retry )
		{
			//  object not set (not flat), try a little more right
			last_x += 10;
			retry--;
		}
		else
		{
			if ( o == 'd' || !retry )
				last_x += Random::Rand() % ( min_dist / 2 ) + _rocket.w();
			else
				last_x += Random::Rand() % max_dist + min_dist;
			o = 0;
			retry = 3;
		}
	}
	addScrollinZone();
	addScrolloutZone();
}

void FltWin::draw_badies()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Badies.size(); i++ )
	{
		Badies[i]->draw();
	}
}

void FltWin::draw_bombs()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Bombs.size(); i++ )
	{
		Bombs[i]->draw();
	}
}

void FltWin::draw_cumuluses()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Cumuluses.size(); i++ )
	{
		Cumuluses[i]->draw();
	}
}

void FltWin::draw_drops()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Drops.size(); i++ )
	{
		Drops[i]->draw();
	}
}

void FltWin::draw_missiles()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Missiles.size(); i++ )
	{
		Missiles[i]->draw();
	}
}

void FltWin::draw_phasers()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Phasers.size(); i++ )
	{
		Phasers[i]->draw();
	}
}

void FltWin::draw_radars()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Radars.size(); i++ )
	{
		Radars[i]->draw();
	}
}

void FltWin::draw_rockets()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Rockets.size(); i++ )
	{
		Rockets[i]->draw();
	}
}

void FltWin::draw_objects( bool pre_ )
//-------------------------------------------------------------------------------
{
	if ( pre_ )
	{
		draw_bombs();
		draw_phasers();
		draw_radars();
		draw_drops();
		draw_badies();
		draw_rockets();
		draw_missiles();
	}
	else
	{
		draw_cumuluses();
		if ( _anim_text )
			_anim_text->draw();
	}
}

int FltWin::drawText( int x_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const
//-------------------------------------------------------------------------------
{
	char buf[200];
	va_list argp;
	va_start( argp, c_ );
	vsnprintf( buf, sizeof( buf ), text_, argp );
	va_end( argp );
	fl_font( FL_HELVETICA_BOLD_ITALIC, sz_ );
	Fl_Color cc = fl_contrast( FL_WHITE, c_ );
	fl_color( cc );
	int x = x_;
	if ( x_ < 0 )
	{
		// centered
		int dx = 0, dy = 0, W = 0, H = 0;
		fl_text_extents( buf, dx, dy, W, H );
		x = ( w() - W ) / 2;
	}

	fl_draw( buf, strlen( buf ), x + 2, y_ + 2 );
	fl_color( c_ );
	fl_draw( buf, strlen( buf ), x, y_ );
	return x;
}

int FltWin::drawTable( int w_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const
//-------------------------------------------------------------------------------
{
	char buf[200];
	va_list argp;
	va_start( argp, c_ );
	vsnprintf( buf, sizeof( buf ), text_, argp );
	va_end( argp );
	fl_font( FL_HELVETICA_BOLD_ITALIC, sz_ );
	Fl_Color cc = fl_contrast( FL_WHITE, c_ );
	fl_color( cc );

	int xl = 0, yl = 0, wl = 0, hl = 0;
	int xr = 0, yr = 0, wr = 0, hr = 0;
	const char *l = buf;
	const char *r = 0;
	char *del = strchr( buf, '\t' );
	if ( del )
	{
		*del++ = 0;
		r = del;
	}
	fl_text_extents( l, xl, yl, wl, hl );
	if ( r )
		fl_text_extents( r, xr, yr, wr, hr );

	int x =  ( w() - w_ ) / 2;

	fl_draw( l, strlen( l ), x + 2, y_ + 2 );
	if ( r )
		fl_draw( r, strlen( r ), x + w_ - wr + 2, y_ + 2 );
	static const int DOT_SIZE = 5;
	int lw = ( x + w_ - wr - 2 * DOT_SIZE ) - ( x + wl + 2 * DOT_SIZE );
	lw = ( ( lw + DOT_SIZE / 2 ) / DOT_SIZE + 1 ) * DOT_SIZE;
	int lx = x + wl + 2 * DOT_SIZE;
	for ( int i = 0; i < lw / ( DOT_SIZE * 2 ); i++ )
		fl_rectf( lx + 2 + i * DOT_SIZE * 2, y_ - 3, DOT_SIZE, DOT_SIZE );
	fl_color( c_ );
	fl_draw( l, strlen( l ), x, y_ );
	if ( r )
		fl_draw( r, strlen( r ), x + w_ - wr, y_ );
	for ( int i = 0; i < lw / ( DOT_SIZE * 2 ); i++ )
		fl_rectf( lx + i * DOT_SIZE * 2, y_ - 5, DOT_SIZE, DOT_SIZE );

	return x;
}

void FltWin::draw_score()
//-------------------------------------------------------------------------------
{
	static Fl_Image *_lifes[3] = { 0, 0, 0 };
	if ( _effects && !_lifes[0] )
	{
		_lifes[0] = new Fl_GIF_Image( imgPath.get( "lifes1.gif" ).c_str() );
		_lifes[1] = new Fl_GIF_Image( imgPath.get( "lifes2.gif" ).c_str() );
		_lifes[2] = new Fl_GIF_Image( imgPath.get( "lifes3.gif" ).c_str() );
	}
	if ( _state == DEMO )
	{
		drawText( -1, h() - 30, "*** DEMO - Hit space to cancel ***", 30, FL_WHITE );
		return;
	}
	if ( _trainMode )
	{
		drawText( w() - 110, 20, "TRAINER", 20, FL_RED );
	}

	fl_font( FL_HELVETICA, 30 );
	fl_color( fl_contrast( FL_WHITE, T.ground_color ) );

	char buf[50];
	int n = snprintf( buf, sizeof( buf ), "Hiscore: %06u", _hiscore );
	fl_draw( buf, n, w() - 280, h() - 30 );

	if ( !_effects )
	{
		n = snprintf( buf, sizeof( buf ), "L %u/%u Score: %06u", _level,
		              MAX_LEVEL_REPEAT - _level_repeat, _user.score );
		fl_draw( buf, n, 20, h() - 30 );
		n = snprintf( buf, sizeof( buf ), "%d %%",
	              (int)( (float)_xoff / (float)( T.size() - w() ) * 100. ) );
		fl_draw( buf, n, w() / 2 - 20 , h() - 30 );
	}
	else
	{
		n = snprintf( buf, sizeof( buf ), "%u%s       Score: %06u",
	                 _level, ( _level < 10 ? " " : "" ), _user.score );
		fl_draw( buf, n, 20, h() - 30 );
		int lifes = MAX_LEVEL_REPEAT - _level_repeat;
		if ( lifes )
		{
			_lifes[lifes - 1]->draw( 60, h() - 52 );
		}
		int proc = (int)( (float)_xoff / (float)( T.size() - w() ) * 100. );
		int X = w() / 2 - 16;
		int Y = h() - 49;
		int W = 100;	// 100% = 100px
		int H = 18;
		fl_rect( X - 1, Y -1, W + 2, H + 2 );
		fl_color( T.ground_color != T.bg_color ? T.bg_color :
		          fl_contrast( T.bg_color, T.ground_color ) );
		fl_rectf( X, Y, proc, H );
	}

	if ( G_paused )
	{
		drawText( -1, 50, "** PAUSED **", 50, FL_WHITE );
	}
	else if ( _done )
	{
		if ( _level == MAX_LEVEL && ( !_trainMode || _cheatMode ) )
		{
			if ( !_completed )
			{
				_completed = true;
				Audio::instance()->enable();	// turn sound on
				free( _terrain );	// switch to direct drawing for grayout!
				_terrain = 0;
				Audio::instance()->play( "win" );
				(_anim_text = new AnimText( 0, 10, w(), "** MISSION COMPLETE! **",
					FL_WHITE, FL_RED, 50, 40, false ))->start();
			}
			if ( _anim_text )
				_anim_text->draw();
			drawText( -1, 150, "You bravely mastered all challenges", 30, FL_WHITE );
			drawText( -1, 190, "and deserve to be called a REAL HERO.", 30, FL_WHITE );
			drawText( -1, 230, "Well done! Take some rest now, or ...", 30, FL_WHITE );
			drawText( -1, 400, "** hit space to restart **", 40, FL_YELLOW );
			// fireworks...
			for ( size_t i = 0; i < Phasers.size(); i++ )
				Phasers[i]->disable();
			T.bg_color = FL_GRAY;
			T.sky_color = fl_darker( FL_GRAY );
			T.ground_color = fl_darker( FL_GRAY );

			int r = 0;
			int dx = w() / 10;
			while ( Rockets.size() < 11 )
			{
				int x = Rockets.size() ? Rockets.back()->x() + dx : dx;
				if ( x > w() - dx )
					x = dx;
				int y = h() + r * 20 + _rocket.h();
				Rockets.push_back( new Rocket( x, y ) );
				Rockets.back()->start();
				r++;
			}
		}
		else
		{
			drawText( -1, 50, "LEVEL %u FINISHED!", 50, FL_WHITE, _level );
		}
	}
	else if ( _collision )
	{
		if ( _level_repeat + 1 > MAX_LEVEL_REPEAT )
		{
			drawText( -1, 50, "START AGAIN!", 50, FL_WHITE );
		}
	}
}

void FltWin::draw_scores()
//-------------------------------------------------------------------------------
{
	fl_rectf( 0, 0, w(), h(), FL_BLACK );
	drawText( -1, 120, "CONGRATULATION!", 70, FL_RED );
	drawText( -1, 200, "You topped the hiscore:", 40, FL_RED );
	int x = drawText( -1, 290, "old hiscore .......... %06u", 30, FL_WHITE, _hiscore );
	int Y = 330;
	if ( _hiscore )
	{
		string bestname( _hiscore_user );
		drawText( x, Y, "held by ................ %s", 30, FL_WHITE,
		          bestname.empty() ?  "???" : bestname.c_str() );
		Y += 40;
	}
	drawText( x, Y, "new hiscore ......... %06u", 30, FL_WHITE, _user.score );

	if ( _input.size() > 10 )
		_input.erase( 10 );
	string name( _input );

	string filler( 10 - name.size(), '_' );
	string temp( name + filler );
	string inpfield;
	for ( size_t i = 0; i < temp.size(); i++ )
	{
		inpfield.push_back( temp[i] );
		inpfield.push_back( ' ' );
	}

	if ( _user.name == DEFAULT_USER )
		drawText( -1, 420, "Enter your name:   %s", 32, FL_GREEN, inpfield.c_str() );
	else
		drawText( -1, 420, "Score goes to:   %s", 32, FL_GREEN, _user.name.c_str() );
	drawText( -1, h() - 50, "** hit space to continue **", 40, FL_YELLOW );
}

void FltWin::draw_title()
//-------------------------------------------------------------------------------
{
	static Fl_Image *bgImage = 0;
	static Fl_Image *bgImage2 = 0;
	static int _flip = 0;
	static int reveal_height = 0;

	if ( _frame <= 1 )
	{
		_flip = 0;
		delete bgImage;
		bgImage = 0;
		delete bgImage2;
		bgImage2 = 0;
		reveal_height = 0;
	}

	if ( !G_paused )
	{
		// speccy loading stripes
		Fl_Color c[] = { FL_GREEN, FL_BLUE, FL_YELLOW };
		int ci = Random::pRand() % 3;
		for ( int y = 0; y < h(); y += 4 )
		{
			fl_color( c[ci] );
			if ( y <= 20 || y >= h() - 20 )
			{
				fl_line( 0, y , w(), y );
				fl_line( 0, y + 1 , w(), y + 1 );
				fl_line( 0, y + 2 , w(), y + 2 );
				fl_line( 0, y + 3 , w(), y + 3 );
			}
			else
			{
				fl_line( 0, y, 40, y ); fl_line( w() - 40, y, w(), y );
				fl_line( 0, y + 1, 40, y + 1 ); fl_line ( w() - 40, y + 1, w(), y + 1 );
				fl_line( 0, y + 2, 40, y + 2 ); fl_line ( w() - 40, y + 2, w(), y + 2 );
				fl_line( 0, y + 3, 40, y + 3 ); fl_line ( w() - 40, y + 3, w(), y + 3 );
			}
			ci++;
			ci %= 3;
		}
	}

	if ( G_paused )
	{
		fl_rectf( 0, 0, w(), h(), fl_rgb_color( 0, 0, 0 ) );
		reveal_height = h();
	}
	else
		fl_rectf( 40, 20, w() - 80, h() - 40, fl_rgb_color( 32, 32, 32 ) );
	if ( _spaceship && _spaceship->image() && !bgImage )
	{
#if FLTK_HAS_NEW_FUNCTIONS
		Fl_Image::RGB_scaling( FL_RGB_SCALING_BILINEAR );
		Fl_RGB_Image *rgb = new Fl_RGB_Image( (Fl_Pixmap *)_spaceship->image() );
		bgImage = rgb->copy( w() - 120, h() - 150 );
		delete rgb;
		rgb = new Fl_RGB_Image( (Fl_Pixmap *)_rocket.image() );
		bgImage2 = rgb->copy( w() / 3, h() - 150 );
		delete rgb;
#else
		bgImage = _spaceship->image()->copy( w() - 120, h() - 150 );
		bgImage2 = _rocket.image()->copy( w() / 3, h() - 150 );
#endif
	}
	if (!_title_anim)
		(_title_anim = new AnimText( 0, 45, w(), "FL-TRATOR",
		                             FL_RED, FL_WHITE, 90, 80, false ))->start();
	if ( !G_paused && _cfg->non_zero_scores() && _flip++ % (FPS * 8) > ( FPS * 5) )
	{
		if ( bgImage2 )
			bgImage2->draw( 60, 120 );
		size_t n = _cfg->non_zero_scores();
		if ( n > 6 )
			n = 6;
		int y = 330 - n / 2 * 40;
		drawText( -1, y, "Hiscores", 35, FL_GREEN );
		y += 50;
		for ( size_t i = 0; i < n; i++ )
		{
			string name( _cfg->users()[i].name );
			drawTable( 360, y, "%05u\t%s", 30,
			              _cfg->users()[i].completed ? FL_YELLOW : FL_WHITE,
			              _cfg->users()[i].score, name.empty() ? DEFAULT_USER : name.c_str() );
			y += 40;
		}
	}
	else
	{
		if ( bgImage )
			bgImage->draw( 60, 40 );
		drawText( -1, 210, "%s (%u)", 30, FL_GREEN,
		          _user.name.empty() ? "N.N." : _user.name.c_str(), _first_level );
		drawTable( 360, 260, "%c/%c\tup/down", 30, FL_WHITE, KEY_UP, KEY_DOWN );
		drawTable( 360, 310, "%c/%c\tleft/right", 30, FL_WHITE, KEY_LEFT, KEY_RIGHT );
		drawTable( 360, 350, "%c\tfire missile", 30, FL_WHITE, KEY_RIGHT );
		drawTable( 360, 390, "space\tdrop bomb", 30, FL_WHITE );
		drawTable( 360, 430, "7-9\thold game", 30, FL_WHITE );
	}
	if ( G_paused )
		drawText( -1, h() - 50, "** PAUSED **", 40, FL_YELLOW );
	else
	{
		drawText( -1, h() - 50, "** hit space to start **", 40, FL_YELLOW );
		drawText( -1, h() - 26, "o/p: toggle user     q/a: toggle ship     s: %s sound",
			10, FL_GRAY, Audio::instance()->enabled() ? "disable" : "enable" );
		drawText( w() - 90, h() - 26, "fps=%d", 8, FL_CYAN, FPS );
	}
	drawText( 45, 34, "v"VERSION, 8, FL_CYAN );

	if (_title_anim)
		_title_anim->draw();

	if ( _gimmicks && reveal_height < h() - 40 )
	{
		fl_rectf( 40, 20 + reveal_height, w() - 80, h() - reveal_height - 40, FL_BLACK );
		reveal_height += 6 * _DX;
	}
}

static int grad( int value_, int max_, int s_, int e_ )
//-------------------------------------------------------------------------------
{
	return s_ + ( e_ - s_ ) * value_ / max_;
}

typedef struct rgb_color { uchar r;	uchar g;	uchar b; } RGB_COLOR;

static void grad_rect( int x_, int y_, int w_, int h_, int H_, bool rev_,
	RGB_COLOR& c1_, RGB_COLOR& c2_ )
//-------------------------------------------------------------------------------
{
	int H = h_;
	int offs = rev_ ? H_ - h_ : 0;
	// gradient from color c1 (top) ==> color c2 (bottom)
	for ( int h = 0; h < H; h++ )
	{
		unsigned char r = grad( h + offs, H_, c1_.r, c2_.r );
		unsigned char g = grad( h + offs, H_, c1_.g, c2_.g );
		unsigned char b = grad( h + offs, H_, c1_.b, c2_.b );
		fl_color( fl_rgb_color( r, g, b ) );
		fl_xyline( x_, y_ + h, x_ + w_ );
	}
}

void FltWin::draw_shaded_landscape( int xoff_, int W_ )
//-------------------------------------------------------------------------------
{
	T.check();
	int s = T.max_sky;
	int g = T.max_ground;
#ifdef DEBUG
	if ( h() - g < s )
	{
		int overlap = s - h() + g;
		printf( "ground and sky overlap by %d px!\n", overlap );
	}
#endif

	if ( s > 0 )
	{
		int W( W_ );
		int X( 0 );
		int D = 100;
		Fl_Color c( T.sky_color );
		Fl_Color cd = fl_darker( c );
		if ( cd == c )
			c = fl_lighter( c );
		else
			c = fl_darker( fl_darker( cd ) );
		RGB_COLOR c1;
		RGB_COLOR c2;
		Fl::get_color( c, c1.r, c1.g, c1.b );
		Fl::get_color( T.sky_color, c2.r, c2.g, c2.b );
		while ( W > 0 )
		{
			if ( W < D )
				D = W;
			int S( 0 );
			for ( int i = 0; i < D; i++ )
			{
//				if ( xoff_ + X + i >= (int)T.size() )
//					break;
				if ( T[xoff_ + X + i].sky_level() > S )
					S = T[xoff_ + X + i].sky_level();
			}
			grad_rect( X, 0, D, S, s, false, c1, c2 );
			W -= D;
			X += D;
		}
	}
	if ( g > 0 )
	{
		int W( W_ );
		int X( 0 );
		int D = 100;
		Fl_Color c( T.ground_color );
		c = fl_lighter( fl_lighter( fl_lighter( c ) ) );
		RGB_COLOR c1;
		RGB_COLOR c2;
		Fl::get_color( c, c1.r, c1.g, c1.b );
		Fl::get_color( T.ground_color, c2.r, c2.g, c2.b );
		while ( W > 0 )
		{
			if ( W < D )
				D = W;
			int G( 0 );
			for ( int i = 0; i < D; i++ )
			{
//				if ( xoff_ + X + i >= (int)T.size() )
//					break;
				if ( T[xoff_ + X + i].ground_level() > G )
					G = T[xoff_ + X + i].ground_level();
			}
			grad_rect( X, h() - G, D, G, g, true, c1, c2 );
			W -= D;
			X += D;
		}
	}

	// background
	fl_color( T.bg_color );
	for ( int i = 0; i < W_; i++ )
	{
		fl_yxline( i, T[xoff_ + i].sky_level(), h() - T[xoff_ + i].ground_level() );
	}
}

void FltWin::draw_landscape( int xoff_, int W_ )
//-------------------------------------------------------------------------------
{
	// draw landscape
	if ( _effects )
		return draw_shaded_landscape( xoff_, W_ );

#if 0
	// the most simple initial version
	for ( int i = 0; i < W_; i++ )
	{
		fl_color( T.sky_color );
		fl_yxline( i, -1, T[xoff_ + i].sky_level() );
		fl_color( T.ground_color );
		fl_yxline( i, h() - T[xoff_ + i].ground_level(), h() );
	}
#else
	int x = 0;
	fl_color( T.sky_color );
	while ( x < W_ )
	{
		int xo = x;
		int s = T[xoff_ + x].sky_level();
		while ( x < W_ && T[xoff_ + x + 1].sky_level() == s )
			x++;
		fl_rectf( xo, 0, x - xo + 1, s );
		x++;
	}

	/*int*/ x = 0;
	fl_color( T.ground_color );
	while ( x < W_ )
	{
		int xo = x;
		int g = T[xoff_ + x].ground_level();
		while ( x < W_ && T[xoff_ + x + 1].ground_level() == g )
			x++;
		fl_rectf( xo, h() - g, x - xo + 1, g );
		x++;
	}
#endif
	// draw outline
	// NOTE: WIN32 must set line style AFTER setting drawing color
	if ( T.ls_outline_width )
	{
		fl_color( T.outline_color_sky );
		fl_line_style( FL_SOLID, T.ls_outline_width );
		for ( int i = 1; i < W_ - 1; i++ )
		{
			if ( T[xoff_ + i].sky_level() >= 0 )
			{
				fl_line( i - 1, T[xoff_ + i - 1].sky_level(),
				         i + 1, T[xoff_ + i + 1].sky_level() );
			}
		}

		fl_color( T.outline_color_ground );
#ifdef WIN32
		fl_line_style( FL_SOLID, T.ls_outline_width );
#endif
		for ( int i = 1; i < W_ - 1; i++ )
		{
			fl_line( i - 1, h() - T[xoff_ + i - 1].ground_level(),
			         i + 1, h() - T[xoff_ + i + 1].ground_level() );
		}
		fl_line_style( 0 );
	}
}

bool FltWin::draw_decoration()
//-------------------------------------------------------------------------------
{
	bool redraw( false );
	if ( _effects > 1 )	// only if turned on additionally
	{
		// shaded bg
		int sky_min = T.min_sky;
		int ground_min = T.min_ground;
		int H = h() - sky_min - ground_min;
		assert( H > 0 );
		int y = 0;
		int W = SCREEN_W;
		Fl_Color c = fl_lighter( T.bg_color );
		while ( y < H )
		{
			int x = 0;
			// Note: fl_color_average() does just the same as grad() - use it?
			fl_color( fl_color_average( T.bg_color, c, float( y ) / H ) );
			while ( x < W )
			{
				while ( x < W &&
				        ( T[_xoff + x].sky_level() > y + sky_min ||
				        h() - T[_xoff + x].ground_level() < y + sky_min ) )
					x++;
				int x0 = x;
				while ( x < W &&
				        ( T[_xoff + x].sky_level() <= y + sky_min &&
				        h() - T[_xoff + x].ground_level() >= y + sky_min ) )
					x++;
				if ( x > x0 )
					fl_xyline( x0, y + sky_min , x - 1 );
			}
			y++;
		}
		redraw = true;
	}
	if ( TBG.flags & 2 )
	{
		// test starfield
		int xoff = _xoff / 4;	// scrollfactor 1/4
		fl_color( FL_YELLOW );
		for ( size_t x = 0; x < SCREEN_W; x++ )
		{
			if ( _xoff + x >= T.size() ) break;
			int sy = TBG[xoff + x].sky_level();
			if ( sy > T[_xoff + x].ground_level() &&
			    h() - sy > T[_xoff + x].sky_level() )
			{
				// draw with a "twinkle" effect
				( Random::pRand() % 10 || G_paused ) ?
					fl_point( x, h() - sy ) : fl_pie( x, h() - sy, 2, 2, 0., 360. );
			}
		}
		redraw = true;
	}
	if ( TBG.flags & 1 )
	{
		// test for "parallax scrolling" background plane
		int xoff = _xoff / 3;	// scrollfactor 1/3
		fl_color( fl_lighter( T.bg_color ) );
		for ( size_t i = 0; i < SCREEN_W; i++ )
		{
			if ( _xoff + i >= T.size() ) break;
			// TODO: take account of outline_width if drawn (currently not)
			int g2 = h() - T[_xoff + i].ground_level()/* - T.ls_outline_width*/;
			int g1 = h() - TBG[(xoff + i + SCREEN_W )].ground_level() * 2 / 3;
			if ( g2 > g1 )
				fl_yxline( i, g1 , g2 );
		}
		redraw = true;
	}
	return redraw;
}

void FltWin::draw()
//-------------------------------------------------------------------------------
{
	_frame++;

	if ( _state == TITLE )
		return draw_title();
	if ( _state == SCORE )
		return draw_scores();
	if ( G_paused && _frame % 10 != 0 )	// don't hog the CPU in paused level mode
		return;
	if ( T.empty() )	// safety check
		return Inherited::draw();

	static int reveal_width = 0;
	if ( _frame <= 1 )
	{
		reveal_width = 0;
	}
	if ( _terrain )
	{
		// "blit" in pre-built image
		_terrain->draw( 0, 0, w(), h(), _xoff );
	}
	else
	{
		// handle color change
		if ( T[_xoff].object() & O_COLOR_CHANGE )
		{
			T.sky_color = T[_xoff].sky_color;
			T.ground_color = T[_xoff].ground_color;
			T.bg_color = T[_xoff].bg_color;
		}

		// draw bg
		fl_rectf( 0, 0, w(), h(), T.bg_color );

		// draw landscape
		draw_landscape( _xoff, w() );
	}

	draw_objects( true );	// objects for collision check

	if ( !G_paused && !paused() && !_done && _state != DEMO && !_collision )
	{
		_collision |= collision( *_spaceship, w(), h() );
		if ( _collision )
			onCollision();
	}

	if ( draw_decoration() )
	{
		// must redraw objects
		draw_objects( true );
	}

	if ( !paused() || _frame % (FPS / 2) < FPS / 4 || _collision )
			_spaceship->draw();

	draw_objects( false );	// objects AFTER collision check (=without collision)

	draw_score();

	if ( _collision )
		_spaceship->draw_collision();

	if ( G_paused )
		reveal_width = w();
	if ( _gimmicks && reveal_width < w() )
	{
		fl_rectf( reveal_width, 0, w() - reveal_width, h(), FL_BLACK );
		reveal_width += 6 * _DX;
	}
}

void FltWin::check_bomb_hits()
//-------------------------------------------------------------------------------
{
	vector <Bomb *>::iterator b = Bombs.begin();
	for ( ; b != Bombs.end(); )
	{
		vector<Rocket *>::iterator r = Rockets.begin();
		for ( ; r != Rockets.end(); )
		{
			if ( !(*r)->exploding() &&
			     (*b)->rect().intersects( (*r)->rect() ) )
			{
				// rocket hit by bomb
				Audio::instance()->play( "bomb_x" );
				(*r)->explode();
				add_score( 50 );

				// bomb also is gone...
				delete *b;
				b = Bombs.erase(b);
				break;
			}
			++r;
		}
		if ( b == Bombs.end() ) break;

		vector<Phaser *>::iterator pa = Phasers.begin();
		for ( ; pa != Phasers.end(); )
		{
			if ( !(*pa)->exploding() &&
			     (*b)->rect().intersects( (*pa)->rect() ) )
			{
				// phaser hit by bomb
				Audio::instance()->play( "bomb_x" );
				(*pa)->explode();
				add_score( 50 );

				// bomb also is gone...
				delete *b;
				b = Bombs.erase(b);
				break;
			}
			++pa;
		}
		if ( b == Bombs.end() ) break;

		vector<Radar *>::iterator ra = Radars.begin();
		for ( ; ra != Radars.end(); )
		{
			if ( !(*ra)->exploding() &&
			     (*b)->rect().intersects( (*ra)->rect() ) )
			{
				// radar hit by bomb
				Audio::instance()->play( "bomb_x" );
				(*ra)->explode();
				add_score( 50 );

				// bomb also is gone...
				delete *b;
				b = Bombs.erase(b);
				break;
			}
			++ra;
		}
		if ( b == Bombs.end() ) break;


		++b;
	}
}

void FltWin::check_drop_hits()
//-------------------------------------------------------------------------------
{
	vector<Drop *>::iterator d = Drops.begin();
	for ( ; d != Drops.end(); )
	{
		if ( !(*d)->dropped() ||
		     !(*d)->rect().intersects( _spaceship->rect() ) )
		{
			++d;
			continue;
		}
		if ( (*d)->collision( *_spaceship ) )
		{
			if ( (*d)->x() > _spaceship->cx() + _spaceship->w() / 4 )	// front of ship
			{
//				printf( "drop deflected\n" );
				(*d)->x( (*d)->x() + 10 );
				++d;
				continue;
			}
//			printf( "drop fully hit spaceship\n" );
			delete *d;
			d = Drops.erase(d);
			_collision = true;
			onCollision();
			continue;
		}
		else
			++d;
	}
}

void FltWin::check_missile_hits()
//-------------------------------------------------------------------------------
{
	vector<Missile *>::iterator m = Missiles.begin();
	for ( ; m != Missiles.end(); )
	{
		vector<Rocket *>::iterator r = Rockets.begin();
		for ( ; r != Rockets.end(); )
		{
			if ( !(*r)->exploding() &&
			     (*m)->rect().intersects( (*r)->rect() ) )
			{
				// rocket hit by missile
				Audio::instance()->play( "missile_hit" );
				(*r)->explode();
				add_score( 20 );

				// missile also is gone...
				delete *m;
				m = Missiles.erase(m);
				break;
			}
			++r;
		}
		if ( m == Missiles.end() ) break;

		vector<Phaser *>::iterator pa = Phasers.begin();
		for ( ; pa != Phasers.end(); )
		{
			if ( !(*pa)->exploding() &&
			     (*m)->rect().intersects( (*pa)->rect() ) )
			{
				// phaser hit by missile
				Audio::instance()->play( "bomb_x" );
				(*pa)->explode();
				add_score( 40 );

				// missile also is gone...
				delete *m;
				m = Missiles.erase(m);
				break;
			}
			++pa;
		}
		if ( m == Missiles.end() ) break;

		vector<Radar *>::iterator ra = Radars.begin();
		for ( ; ra != Radars.end(); )
		{
			if ( !(*ra)->exploding() &&
			     (*m)->rect().intersects( (*ra)->rect() ) )
			{
				// radar hit by missile
				if ( (*ra)->hit() )	// takes 2 missile hits to succeed!
				{
					Audio::instance()->play( "missile_hit" );
					(*ra)->explode();
					add_score( 40 );
				}
				// missile also is gone...
				delete *m;
				m = Missiles.erase(m);
				break;
			}
			++ra;
		}
		if ( m == Missiles.end() ) break;

		vector<Bady *>::iterator b = Badies.begin();
		for ( ; b != Badies.end(); )
		{
			if ( (*m)->rect().inside( (*b)->rect()) )
			{
				// bady hit by missile
				if ( (*b)->hit() )
				{
					Audio::instance()->play( "bady_hit" );
					delete *b;
					b = Badies.erase(b);
					add_score( 100 );
				}
				// missile is also gone...
				delete *m;
				m = Missiles.erase(m);
				break;
			}
			++b;
		}
		if ( m == Missiles.end() ) break;

		vector<Drop *>::iterator d = Drops.begin();
		for ( ; d != Drops.end(); )
		{
			if ( (*m)->rect().intersects( (*d)->rect()) )
			{
				// drop hit by missile
				(*d)->hit();
				Audio::instance()->play( "drop_hit" );
				delete *d;
				d = Drops.erase(d);
				add_score( 5 );
				break;
			}
			++d;
		}
		if ( m == Missiles.end() ) break;

		++m;
	}
}

void FltWin::check_rocket_hits()
//-------------------------------------------------------------------------------
{
	vector<Rocket *>::iterator r = Rockets.begin();
	for ( ; r != Rockets.end(); )
	{
		if ( !(*r)->exploding() &&
		  	(*r)->lifted() && (*r)->rect().intersects( _spaceship->rect() ) )
		{
			if ( (*r)->collision( *_spaceship ) )
			{
				// rocket hit by spaceship
				(*r)->explode( 0.5 );
				_collision = true;
				onCollision();
			}
		}
		++r;
	}
}

void FltWin::check_hits()
//-------------------------------------------------------------------------------
{
	check_missile_hits();
	check_bomb_hits();
	if ( !_done )
		check_rocket_hits();
	check_drop_hits();
}

void FltWin::create_spaceship()
//-------------------------------------------------------------------------------
{
	delete _spaceship;
	_spaceship = new Spaceship( w() / 2, 40, w(), h(), alternate_ship(),
		_effects != 0 );	// before create_terrain()!
}

void FltWin::create_objects()
//-------------------------------------------------------------------------------
{
	// Only consider scrolled part!
	// Plus 150 pixel (=half width of broadest object) in advance in order to
	// make appearance of new objects smooth.
	for ( int i = (_xoff == 0 ? 0 : w() - _DX); i < w() + 150 ; i++ )
	{
		if ( _xoff + i >= (int)T.size() ) break;
		unsigned int o = T[_xoff + i].object();
		if ( !o ) continue;
		if ( o & O_ROCKET && i - _rocket.w() / 2 < w() )
		{
			Rocket *r = new Rocket( i, h() - T[_xoff + i].ground_level() - _rocket.h() / 2 );
			int start_dist = _rocket_max_start_dist - _level * 20 -
			                 Random::Rand() % _rocket_var_start_dist;
			if ( start_dist < _rocket_min_start_dist )
			{
				start_dist = _rocket_min_start_dist;
			}
			r->data1( start_dist );
			Rockets.push_back( r );
			o &= ~O_ROCKET;
#ifdef DEBUG
			printf( "#rockets: %lu\n", (unsigned long)Rockets.size() );
#endif
		}
		if ( o & O_RADAR && i - _radar.w() / 2 < w() )
		{
			Radar *r = new Radar( i, h() - T[_xoff + i].ground_level() - _radar.h() / 2 );
			Radars.push_back( r );
			o &= ~O_RADAR;
#ifdef DEBUG
			printf( "#radars: %lu\n", (unsigned long)Radars.size() );
#endif
		}
		if ( o & O_PHASER && i - _phaser.w() / 2 < w())
		{
			Phaser *p = new Phaser( i, h() - T[_xoff + i].ground_level() - _phaser.h() / 2 );
			Phasers.push_back( p );
			o &= ~O_PHASER;
#ifdef DEBUG
			printf( "#phaser: %lu\n", (unsigned long)Phasers.size() );
#endif
			Phasers.back()->bg_color( T.bg_color );
			Phasers.back()->max_height( T[_xoff + i].sky_level() );
			Phasers.back()->start();
		}
		if ( o & O_DROP && i - _drop.w() / 2 < w() )
		{
			Drop *d = new Drop( i, T[_xoff + i].sky_level() + _drop.h() / 2 );
			int start_dist = _drop_max_start_dist - _level * 20 -
			                 Random::Rand() % _drop_var_start_dist;
			if ( start_dist < _drop_min_start_dist )
			{
				start_dist = _drop_min_start_dist;
			}
			d->data1( start_dist );
			Drops.push_back( d );
			o &= ~O_DROP;
#ifdef DEBUG
			printf( "#drops: %lu\n", (unsigned long)Drops.size() );
#endif
		}
		if ( o & O_BADY && i - _bady.w() / 2 < w() )
		{
			bool turn( Random::Rand() % 3 == 0 );
			int X = turn ?
			        h() - T[_xoff + i].ground_level() - _bady.h() / 2 :
			        T[_xoff + i].sky_level() + _bady.h() / 2;
			Bady *b = new Bady( i, X );
			o &= ~O_BADY;
			if ( turn )
				b->turn();
			b->stamina( rangedRandom( _bady_min_hits, _bady_max_hits ) );
			Badies.push_back( b );
#ifdef DEBUG
			printf( "#badies: %lu\n", (unsigned long)Badies.size() );
#endif
		}
		if ( o & O_CUMULUS && i - _cumulus.w() / 2 < w() )
		{
			bool turn( Random::Rand() % 3 == 0 );
			int X = turn ?
			        h() - T[_xoff + i].ground_level() - _cumulus.h() / 2 :
			        T[_xoff + i].sky_level() + _cumulus.h() / 2;
			Cumulus *c = new Cumulus( i, X );
			o &= ~O_CUMULUS;
			if ( turn )
				c->turn();
			Cumuluses.push_back( c );
#ifdef DEBUG
			printf( "#cumuluses: %lu\n", (unsigned long)Cumuluses.size() );
#endif
		}
		T[_xoff + i].object( o );
	}
}

void FltWin::delete_objects()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Rockets.size(); i++ )
		delete Rockets[i];
	Rockets.clear();
	for ( size_t i = 0; i < Phasers.size(); i++ )
		delete Phasers[i];
	Phasers.clear();
	for ( size_t i = 0; i < Radars.size(); i++ )
		delete Radars[i];
	Radars.clear();
	for ( size_t i = 0; i < Drops.size(); i++ )
		delete Drops[i];
	Drops.clear();
	for ( size_t i = 0; i < Badies.size(); i++ )
		delete Badies[i];
	Badies.clear();
	for ( size_t i = 0; i < Cumuluses.size(); i++ )
		delete Cumuluses[i];
	Cumuluses.clear();
}

void FltWin::update_badies()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Badies.size(); i++ )
	{
		if ( !paused() )
			Badies[i]->x( Badies[i]->x() - _DX );

		int top = T[_xoff + Badies[i]->x() + Badies[i]->w() / 2].sky_level();
		int bottom = h() - T[_xoff + Badies[i]->x() + Badies[i]->w() / 2].ground_level();
		bool gone = Badies[i]->x() < -Badies[i]->w();
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one bady from #%lu\n", (unsigned long)Badies.size() );
#endif
			Bady *b = Badies[i];
			Badies.erase( Badies.begin() + i );
			delete b;
			i--;
			continue;
		}
		else
		{
			// bady fall strategy: always start!
			if ( !Badies[i]->started() )
			{
				int speed = rangedValue( rangedRandom( _bady_min_start_speed, _bady_max_start_speed ), 1, 10 );
//				printf( "   start bady with speed %d!\n", speed );
				Badies[i]->start( speed );
				assert( Badies[i]->started() );
			}
			else
			{
				if ( ( Badies[i]->y() + Badies[i]->h() >= bottom &&
				       !Badies[i]->turned() ) ||
				      ( Badies[i]->y() <= top  &&
				        Badies[i]->turned() ) )
					Badies[i]->turn();
			}
		}
	}
}

void FltWin::update_bombs()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Bombs.size(); i++ )
	{
		bool gone = Bombs[i]->y() > h() ||
		            Bombs[i]->y() + Bombs[i]->h() > h() - T[_xoff + Bombs[i]->x()].ground_level();
		if ( gone )
		{
//			printf( " gone one bomb from #%d\n", Missiles.size() );
			Bomb *b = Bombs[i];
			Bombs.erase( Bombs.begin() + i );
			delete b;
			i--;
		}
	}
}

void FltWin::update_cumuluses()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Cumuluses.size(); i++ )
	{
		if ( !paused() )
			Cumuluses[i]->x( Cumuluses[i]->x() - _DX );

		int top = T[_xoff + Cumuluses[i]->x() + Cumuluses[i]->w() / 2].sky_level();
		int bottom = h() - T[_xoff + Cumuluses[i]->x() + Cumuluses[i]->w() / 2].ground_level();
		bool gone = Cumuluses[i]->x() < -Cumuluses[i]->w();
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one cumulus from #%lu\n", (unsigned long)Cumuluses.size() );
#endif
			Cumulus *c = Cumuluses[i];
			Cumuluses.erase( Cumuluses.begin() + i );
			delete c;
			i--;
			continue;
		}
		else
		{
			// cumulus fall strategy: always start!
			if ( !Cumuluses[i]->started() )
			{
				int speed = rangedValue( rangedRandom( _cumulus_min_start_speed, _cumulus_max_start_speed ), 1, 10 );
//				printf( "   start cunulus with speed %d!\n", speed );
				Cumuluses[i]->start( speed );
				assert( Cumuluses[i]->started() );
			}
			else
			{
				if ( ( Cumuluses[i]->y() + Cumuluses[i]->h() >= bottom &&
				       !Cumuluses[i]->turned() ) ||
				      ( Cumuluses[i]->y() <= top  &&
				        Cumuluses[i]->turned() ) )
					Cumuluses[i]->turn();
			}
		}
	}
}

void FltWin::update_drops()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Drops.size(); i++ )
	{
		if ( !paused() )
			Drops[i]->x( Drops[i]->x() - _DX );

		int bottom = h() - T[_xoff + Drops[i]->x()].ground_level();
		if ( 0 == bottom  ) bottom += Drops[i]->h();	// glide out completely if no ground
		bool gone = Drops[i]->y() + Drops[i]->h() / 2 > bottom || Drops[i]->x() < -Drops[i]->w();
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one drop from #%lu\n", (unsigned long)Drops.size() );
#endif
			Drop *d = Drops[i];
			Drops.erase( Drops.begin() + i );
			delete d;
			i--;
			continue;
		}
		else
		{
			// drop fall strategy...
			if ( Drops[i]->nostart() ) continue;
			if ( Drops[i]->dropped() ) continue;
			// if user has completed all 10 levels, objects start later (which is harder)...
			int dist = user_completed() ? Drops[i]->cx() - _spaceship->cx() // center distance S<->R
			                            : Drops[i]->x() - _spaceship->x();
			int start_dist = (int)Drops[i]->data1();	// 400 - _level * 20 - Random::Rand() % 200;
			if ( dist <= 0 || dist >= start_dist ) continue;
//			printf( "start drop #%ld, start_dist=%d?\n", i, start_dist );
			// really start drop?
			unsigned drop_prob = _drop_start_prob;
//			printf( "drop_prob: %u\n", drop_prob );
			if ( Random::Rand() % 100 < drop_prob )
			{
				int speed = rangedValue( rangedRandom( _drop_min_start_speed, _drop_max_start_speed ), 1, 10 );
//				printf( "   drop with speed %d!\n", speed );
				Drops[i]->start( speed );
				assert( Drops[i]->dropped() );
				continue;
			}
			Drops[i]->nostart( true );
		}
	}
}

void FltWin::update_missiles()
//-------------------------------------------------------------------------------
{
//	printf( "#%u missiles\n", Missiles.size() );
	for ( size_t i = 0; i < Missiles.size(); i++ )
	{
		bool gone = Missiles[i]->exhausted() ||
		            Missiles[i]->x() > w() ||
		            Missiles[i]->y() > h() - T[_xoff + Missiles[i]->x() + Missiles[i]->w()].ground_level() ||
		            Missiles[i]->y() < T[_xoff + Missiles[i]->x() + Missiles[i]->w()].sky_level();
		if ( gone )
		{
//			printf( " gone one missile from #%d\n", Missiles.size() );
			Missile *m = Missiles[i];
			Missiles.erase( Missiles.begin() + i );
			delete m;
			i--;
		}
	}
}

void FltWin::update_phasers()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Phasers.size(); i++ )
	{
		if ( !paused() )
			Phasers[i]->x( Phasers[i]->x() - _DX );

		bool gone = Phasers[i]->exploded() || Phasers[i]->x() < -Phasers[i]->w();
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one phaser from #%lu\n", (unsigned long)Phasers.size() );
#endif
			Phaser *p = Phasers[i];
			Phasers.erase( Phasers.begin() + i );
			delete p;
			i--;
			continue;
		}
	}
}

void FltWin::update_radars()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Radars.size(); i++ )
	{
		if ( !paused() )
			Radars[i]->x( Radars[i]->x() - _DX );

		bool gone = Radars[i]->exploded() || Radars[i]->x() < -Radars[i]->w();
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one radar from #%lu\n", (unsigned long)Radars.size() );
#endif
			Radar *r = Radars[i];
			Radars.erase( Radars.begin() + i );
			delete r;
			i--;
			continue;
		}
	}
}

void FltWin::update_rockets()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Rockets.size(); i++ )
	{
		if ( !paused() )
			Rockets[i]->x( Rockets[i]->x() - _DX );

		int top = T[_xoff + Rockets[i]->x()].sky_level();
		if ( -1 == top  ) top -= Rockets[i]->h();	// glide out completely if no sky
		bool gone = Rockets[i]->exploded() || Rockets[i]->x() < -Rockets[i]->w();
		// NOTE: when there's no sky, explosion is not visible, but still occurs,
		//       so checking exploded() suffices as end state for all rockets.
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one rocket from #%lu\n", (unsigned long)Rockets.size() );
#endif
			Rocket *r = Rockets[i];
			Rockets.erase( Rockets.begin() + i );
			delete r;
			i--;
			continue;
		}
		else if ( Rockets[i]->y() <= top && !Rockets[i]->exploding() )
		{
			Rockets[i]->crash();
		}
		else if ( !Rockets[i]->exploding() )
		{
			// rocket fire strategy...
			if ( Rockets[i]->nostart() ) continue;
			if ( Rockets[i]->lifted() ) continue;
			// if user has completed all 10 levels, objects start later (which is harder)...
			int dist = user_completed() ? Rockets[i]->cx() - _spaceship->cx() // center distance S<->R
			                            : Rockets[i]->x() - _spaceship->x();
			int start_dist = (int)Rockets[i]->data1();	// 400 - _level * 20 - Random::Rand() % 200;
			if ( dist <= 0 || dist >= start_dist ) continue;
//			printf( "start rocket #%ld, start_dist=%d?\n", i, start_dist );
			// really start rocket?
			unsigned lift_prob = _rocket_start_prob;
//			printf( "lift prob: %u\n", lift_prob );
			// if a radar is within start_dist then the
			// probability gets much higher
			int x = Rockets[i]->x();
			for ( size_t ra = 0; ra < Radars.size(); ra++ )
			{
				if ( Radars[ra]->defunct() ) continue;
				if ( Radars[ra]->x() >= x ) continue;
				if ( Radars[ra]->x() <= x - start_dist ) continue;
				// there's a working radar within start_dist
				lift_prob = _rocket_radar_start_prob;
//				printf( "raise lift_prob to %f\n", lift_prob );
				break;
			}
			if ( Random::Rand() % 100 < lift_prob )
			{
				int speed = rangedValue( rangedRandom( _rocket_min_start_speed, _rocket_max_start_speed ), 1, 10 );
//				printf( "   lift with speed %d!\n", speed );
				Rockets[i]->start( speed );
				assert( Rockets[i]->lifted() );
				continue;
			}
			Rockets[i]->nostart( true );
		}
	}
}

void FltWin::update_objects()
//-------------------------------------------------------------------------------
{
	update_missiles();
	update_bombs();
	update_rockets();
	update_phasers();
	update_radars();
	update_drops();
	update_badies();
	update_cumuluses();
}

void FltWin::setTitle()
//-------------------------------------------------------------------------------
{
	ostringstream os;
	if ( _state == LEVEL || _state == DEMO || _trainMode)
	{
		if ( _state == DEMO )
			os << "DEMO ";
		os << "Level " << _level;
		if ( _internal_levels )
			os << " (internal)";
		else if ( _level_name.size() )
			os << ": " << _level_name;
		os << " - ";
	}
	os << _title;
	if ( G_paused && _state != START )
		os << " (paused)";
	copy_label( os.str().c_str() );
}

void FltWin::setUser()
//-------------------------------------------------------------------------------
{
	_user = _cfg->readUser( _user.name );
	_first_level = _user.level + 1;
	if ( _first_level > MAX_LEVEL )
		_first_level = 1;
	_level = _first_level;
//	cout << "user " << _user.name << " completed: " << _user.completed << endl;
}

void FltWin::resetUser()
//-------------------------------------------------------------------------------
{
	_user.score = 0;
	_user.completed = 0;
	_first_level = 1;
	_level = 1;
	_cfg->writeUser( _user );	// but no flush() - must play one level to make permanent!
	_cfg->load();
	keyClick();
	onTitleScreen();	// immediate display + reset demo timer
}

void FltWin::setBgSoundFile()
//-------------------------------------------------------------------------------
{
	_bgsound.erase();
	if ( Audio::hasBgSound() )
	{
		string bgfile = wavPath.get( "bgsound" );
		if ( access( bgfile.c_str(), R_OK ) == 0 )
		{
			_bgsound = bgfile;
		}
		else
		{
			// pick a random bg sound from folder 'bgsound'
			dirent **ls;
			Fl_File_Sort_F *sort = fl_casealphasort;
			string bgdir( mkPath( "bgsound" ) );
			int num_files = fl_filename_list( bgdir.c_str(), &ls, sort );
			vector<string> bgSoundList;
			for ( int i = 0; i < num_files; i++ )
			{
				string pattern = "*." + Audio::instance()->ext();
				if ( fl_filename_match( ls[i]->d_name, pattern.c_str() ) )
				{
					bgSoundList.push_back( bgdir + ls[i]->d_name );
				}
			}
			fl_filename_free_list( &ls, num_files );
			if ( bgSoundList.size() )
			{
				_bgsound = bgSoundList[Random::pRand() % bgSoundList.size()] ;
			}
		}
	}
}

void FltWin::setPaused( bool paused_ )
//-------------------------------------------------------------------------------
{
	if ( G_paused != paused_ )
	{
		G_paused = paused_;
		if ( G_paused )
			onPaused();
		else
			onContinued();
	}
}

void FltWin::startBgSound() const
//-------------------------------------------------------------------------------
{
	if ( _bgsound.size() )
		Audio::instance()->play( _bgsound.c_str(), true );
}

void FltWin::keyClick() const
//-------------------------------------------------------------------------------
{
	Audio::instance()->play( "drop" );
}

void FltWin::toggleBgSound() const
//-------------------------------------------------------------------------------
{
	if ( Audio::hasBgSound() )
	{
		Audio::instance()->bg_disable( !Audio::instance()->bg_disabled() );
		if ( Audio::instance()->bg_enabled() && _state == LEVEL )
			startBgSound();
		else if ( Audio::instance()->bg_disabled() && _state == LEVEL )
			Audio::instance()->stop_bg();
	}
	else
	{
		Audio::instance()->disable( !Audio::instance()->disabled() );
	}
}

bool FltWin::setFullscreen( bool fullscreen_ )
//-------------------------------------------------------------------------------
{
	static int ox = 0;
	static int oy = 0;
	int X, Y, W, H;
	bool isFull = fullscreen_active();

	if ( fullscreen_ )
	{
		// switch to fullscreen
		if ( !isFull )
		{
			ox = x();
			oy = y();
			hide();
			if ( setScreenResolution( SCREEN_W, SCREEN_H ) )
			{
				Fl::check();	// X11: neccessary for FLTK to register screen res. change!
				fullscreen();
			}
			else
			{
				// failure to change resolution, check if screen fits anyway
				Fl::screen_xywh( X, Y, W, H );
				if ( W == (int)SCREEN_W && H == (int)SCREEN_H )
				{
					// screen has just the right resolution, so fullscreen can be allowed
					fullscreen();
				}
				else
				{
					cerr << "Failed to set resolution to " << SCREEN_W << "x"<< SCREEN_H <<
						" ( is: " << W << "x" << H << ")" << endl;
				}
			}
			show();
			size( SCREEN_W, SCREEN_H );	// WIN32: without window becomes size of screen!
		}
	}
	else
	{
		// switch to normal screen
		if ( isFull )
		{
			hide();
			restoreScreenResolution();
			Fl::check();	// X11: neccessary for FLTK to register screen res. change!
			fullscreen_off();
			show();
		}
		if ( !_no_position )
		{
			if ( ox == 0 && oy == 0 )
			{
				// try to center on current screen
				Fl::screen_xywh( X, Y, W, H );
				ox = ( W - w() ) / 2;
				oy = ( H - h() ) / 2;
			}
			resize( ox, oy, SCREEN_W, SCREEN_H );
		}
	}
	if ( _hide_cursor )
	{
		// hide mouse cursor
		cursor( FL_CURSOR_NONE, (Fl_Color)0, (Fl_Color)0 );
		default_cursor( FL_CURSOR_NONE, (Fl_Color)0, (Fl_Color)0 );
	}
	else if ( _mouseMode )
	{
		cursor( FL_CURSOR_HAND );
		default_cursor( FL_CURSOR_HAND );
	}
	return fullscreen_active();
}

bool FltWin::toggleFullscreen()
//-------------------------------------------------------------------------------
{
	return setFullscreen( !fullscreen_active() );
}

void FltWin::toggleSound() const
//-------------------------------------------------------------------------------
{
	Audio::instance()->disable( !Audio::instance()->disabled() );
}

void FltWin::togglePaused()
//-------------------------------------------------------------------------------
{
	if ( G_paused )
	{
		G_paused = false;
		onContinued();
	}
	else
	{
		G_paused = true;
		onPaused();
	}
}

void FltWin::toggleShip( bool prev_/* = false */ )
//-------------------------------------------------------------------------------
{
	_alternate_ship = !_alternate_ship;
	keyClick();
	create_spaceship();
	onTitleScreen();	// immediate display + reset demo timer
}

void FltWin::toggleUser( bool prev_/* = false */ )
//-------------------------------------------------------------------------------
{
	int found = -1;
	for ( size_t i = 0; i < _cfg->users().size(); i++ )
	{
		if ( _user.name == _cfg->users()[i].name )
		{
			found = (int)i;
			break;
		}
	}
	if ( found >= 0 )
	{
		prev_ ? found-- : found++;
		if ( found < 0 )
			found = _cfg->users().size() - 1;
		if ( (size_t)found >= _cfg->users().size() )
			found = 0;
		_user.name = _cfg->users()[found].name;
		setUser();

		keyClick();
		onTitleScreen();	// immediate display + reset demo timer
	}
}

bool FltWin::dropBomb()
//-------------------------------------------------------------------------------
{
	if	(!_bomb_lock && !paused() && Bombs.size() < 5 )
	{
		Bomb *b = new Bomb( _spaceship->x() + _spaceship->bombPoint().x,
		                    _spaceship->y() + _spaceship->bombPoint().y +
		                    ( alternate_ship() ? 14 : 10 ));	// alternate ship needs more offset!
		Bombs.push_back( b );
		Bombs.back()->start( _speed_right );
		_bomb_lock = true;
		if ( _state == LEVEL )
			_demoData.setBomb( _xoff );
		Fl::add_timeout( 0.5, cb_bomb_unlock, this );
		return true;
	}
	return false;
}

bool FltWin::fireMissile()
//-------------------------------------------------------------------------------
{
	if ( Missiles.size() < 5 && !paused() )
	{
		Missile *m = new Missile( _spaceship->x() + _spaceship->missilePoint().x + 20,
		                          _spaceship->y() + _spaceship->missilePoint().y );
		Missiles.push_back( m );
		Missiles.back()->start();
		if ( _state == LEVEL )
			_demoData.setMissile( _xoff );
		return true;
	}
	return false;
}

void FltWin::bombUnlock()
//-------------------------------------------------------------------------------
{
	_bomb_lock = false;
}

/*static*/
void FltWin::cb_bomb_unlock( void *d_ )
//-------------------------------------------------------------------------------
{
	((FltWin *)d_)->bombUnlock();
}

/*static*/
void FltWin::cb_demo( void *d_ )
//-------------------------------------------------------------------------------
{
	((FltWin *)d_)->changeState( DEMO );
}

/*static*/
void FltWin::cb_paused( void *d_ )
//-------------------------------------------------------------------------------
{
	((FltWin *)d_)->changeState();
}

/*static*/
void FltWin::cb_update( void *d_ )
//-------------------------------------------------------------------------------
{
	FltWin *f = (FltWin *)d_;
	f->state() == FltWin::DEMO ? f->onUpdateDemo() : f->onUpdate();
	if ( _USE_FLTK_RUN )
		Fl::repeat_timeout( FRAMES, cb_update, d_ );
}

void FltWin::onActionKey()
//-------------------------------------------------------------------------------
{
//	printf( "onActionKey\n" );
	Fl::remove_timeout( cb_demo, this );
	Fl::remove_timeout( cb_paused, this );
	if ( _state == DEMO )
	{
		_done = true;	// ensure ship gets reloaded in onNextScreen()
		return;
	}
	if ( _state == SCORE )
	{
		if ( _input.empty() )
			_input = DEFAULT_USER;
		if ( _user.name == DEFAULT_USER && _input != DEFAULT_USER )
			_cfg->removeUser( _user.name );	// remove default user
		if ( _user.name == DEFAULT_USER )
			_user.name = _input;
//		printf( "_user.name: '%s'\n", _user.name.c_str() );
//		printf( "_input: '%s'\n", _input.c_str() );
//		printf( "_score: '%u'\n", _user.score );
		_cfg->writeUser( _user );
		_cfg->flush();
		_hiscore = _cfg->hiscore();
		_hiscore_user = _cfg->best().name;
		_first_level = _done_level + 1;
		if ( _first_level > MAX_LEVEL )
			_first_level = 1;
//		printf( "score saved.\n" );
	}

	changeState();
}

void FltWin::onCollision()
//-------------------------------------------------------------------------------
{
	if ( _done )
		_collision = false;
	else if ( _cheatMode )
	{
		if ( _xoff % 20 == 0 )
			Audio::instance()->play( "boink" );
		_collision = false;
	}
}

void FltWin::onPaused()
//-------------------------------------------------------------------------------
{
	Audio::instance()->stop_bg();
	setTitle();
}

void FltWin::onContinued()
//-------------------------------------------------------------------------------
{
	if ( _state == LEVEL )
	{
		startBgSound();
	}
	setTitle();
}

void FltWin::onGotFocus()
//-------------------------------------------------------------------------------
{
	if ( !_focus_out )
		return;

//	printf(" onGotFocus()\n" );
	if ( _state == TITLE || _state == DEMO )
	{
		setPaused( false );
		_state = START;
		changeState( TITLE );
	}
	setTitle();
}

void FltWin::onLostFocus()
//-------------------------------------------------------------------------------
{
	if ( !_focus_out )
		return;

//	printf(" onLostFocus()\n" );
	_left = _right = _up = _down = false;	// invalidate keys
	setPaused( true );	// (this also stops bg sound)
	if ( _state == TITLE || _state == DEMO )
	{
		_state = START;	// re-change also to TITLE
		changeState( TITLE );
	}
	setTitle();
}

void FltWin::onDemo()
//-------------------------------------------------------------------------------
{
//	printf( "starting demo...\n" );
	_state = DEMO;
	onNextScreen( true );
}

void FltWin::onNextScreen( bool fromBegin_/* = false*/ )
//-------------------------------------------------------------------------------
{
	delete _spaceship;
	_spaceship = 0;
	delete _anim_text;
	_anim_text = 0;
	delete _title_anim;
	_title_anim = 0;

	delete_objects();

	_xoff = 0;
	_frame = 0;
	_collision = false;
	_completed = false;

	bool show_scores = false;
	if ( _state == LEVEL && _demoData.size() && _demoData.size() >= T.size() - w() &&
		!_trainMode && !_no_demo )
		saveDemoData();

	if ( fromBegin_ )
	{
		Random::Srand( time( 0 ) );	// otherwise determined because of demo seed!
		_level = _state == DEMO ? pickRandomDemoLevel( 1,
		                          max( 3, _user.completed ? (int)MAX_LEVEL : (int)_level ) )
		                        : _first_level;
		_level_repeat = 0;
		_user.score = _cfg->readUser( _user.name).score;
	}
	else
	{
		show_scores = _cfg->readUser( _user.name ).score > (int)_hiscore && !_done;

		if ( _done || _state == DEMO )
		{
			_level++;
			_level_repeat = 0;

			if ( _level > MAX_LEVEL || _state == DEMO/*new: demo stops after one level*/ )
			{
				_level = _first_level; // restart with level 1
				_user.score = _cfg->readUser( _user.name ).score;
				changeState( TITLE );
			}
		}
		else
		{
			_level_repeat++;
			if ( _level_repeat > MAX_LEVEL_REPEAT )
			{
				_level = _first_level;
				_level_repeat = 0;
				changeState( TITLE );
			}
		}
	}

	if ( _state == DEMO )
		loadDemoData();

	create_spaceship();	// after loadDemoData()!

	if ( !create_terrain() )
	{
		hide();
		return;
	}

	if ( _demoData.size() && _demoData.size() < T.size() - w() )
	{
		// clear incomplete demo data
		_demoData.clear();
	}

	if ( _state == DEMO && !_demoData.size() )
	{
		changeState( TITLE );
	}

	position_spaceship();

	if ( !_level_repeat )
	{
		(_anim_text = new AnimText( 0, 20, w(), _level_name.c_str() ))->start();
		if ( _state == LEVEL )
			setBgSoundFile();
	}

	_done = false;
	setPaused( false );
	_left = _right = _up = _down = false;

	show_scores &= _level == _first_level && _level_repeat == 0;
	if ( show_scores )
	{
		_input = _user.name == DEFAULT_USER ? "" : _user.name;
		changeState( SCORE );
	}
	else if ( _state == LEVEL )
	{
		startBgSound();
	}
}

void FltWin::onTitleScreen()
//-------------------------------------------------------------------------------
{
	if ( _trainMode )
		return;
	_state = TITLE;
	_frame = 0;
	Fl::remove_timeout( cb_demo, this );
	if ( !G_paused && !_no_demo )
	{
		Fl::add_timeout( 20.0, cb_demo, this );
	}
}

void FltWin::onUpdateDemo()
//-------------------------------------------------------------------------------
{
	create_objects();

	update_objects();

	check_hits();

	int cx = 0;
	int cy = 0;
	bool bomb( false );
	bool missile( false );
	if ( !_demoData.get( _xoff, cx, cy, bomb, missile ) )
		_done = true;
	else
	{
		// use Spaceshipo::right()/left() methods for accel/decel feedback in demo
		while ( _spaceship->cx() < cx )
			_spaceship->right();
		while ( _spaceship->cx() > cx )
			_spaceship->left();
//		_spaceship->cx( cx );
		_spaceship->cy( cy );
		if ( bomb )
			dropBomb();
		if ( missile )
			fireMissile();
	}

	_collision = false;

	redraw();

	if ( _done )
	{
		onNextScreen();	// ... but for now, just skip pause in demo mode
		return;	// do not increment _xoff now!
	}

	_xoff += _DX;
	if ( _xoff + w() >= (int)T.size() - 1 )
		_done = true;
}

bool FltWin::correctDX()
//-------------------------------------------------------------------------------
{
	_DX = DX;
	if ( _waiter.elapsedMilliSeconds() )
	{
		_DX = ( ( _waiter.elapsedMilliSeconds() + 2 ) / 5 );
		if ( _DX != DX  )
		{
#ifdef DEBUG
			printf( "_DX = %d (%u)\n", _DX, _waiter.elapsedMilliSeconds() );
#endif
			// set limit
			if ( _DX > 10 )
				_DX = 10;
		}
		return true;
	}
	return false;
}

void FltWin::onUpdate()
//-------------------------------------------------------------------------------
{
	if ( _mouseMode )
	{
		static int cnt =0 ;
		cnt += _DX;
		if ( cnt >= 10 )
		{
			cnt = 0;
			handle( TIMER_CALLBACK );
		}
	}

	if ( _state == TITLE || _state == SCORE || G_paused )
	{
		redraw();
		return;
	}
	create_objects();

	update_objects();

	check_hits();

	if ( paused() )
	{
		Audio::instance()->check( true );	// reliably stop bg-sound
		redraw();
		return;
	}

	if ( _xoff % SCORE_STEP == 0 )
	{
		add_score( 1 );
		Audio::instance()->check();	// test bg-sound expired
	}

	if ( _left && _xoff + w() + w() / 2 < (int)T.size() ) // no retreat at end!
		_spaceship->left();
	if ( _right )
	{
		int ox = _spaceship->x();
		_spaceship->right();
		if ( ox != _spaceship->x() )
			_speed_right++;
	}
	if ( _up )
		_spaceship->up();
	if ( _down  )
		_spaceship->down();
	_demoData.setShip( _xoff, _spaceship->cx(), _spaceship->cy() );

	redraw();
	if ( _collision )
		changeState( LEVEL_FAIL );
	else if ( _done )
		changeState( LEVEL_DONE );

	_xoff += _DX;
	if ( _xoff + w() >= (int)T.size() - 1 )
		_done = true;
}

int FltWin::handle( int e_ )
//-------------------------------------------------------------------------------
{
#define F10_KEY 0xffc7
	static bool ignore_space = false;
	static int repeated_right = -1;

	if ( _mouseMode )
	{
		static int x0 = 0;
		static int y0 = 0;
		static bool dragging = false;
		static bool maybe_missile = false;
		static bool bomb = false;
		static bool missile = false;
		static int wheel = 0;

		int x = Fl::event_x();
		int y = Fl::event_y();
		switch ( e_ )
		{
			case TIMER_CALLBACK:
				if ( missile )
					fireMissile();
				if ( bomb )
					dropBomb();
				if ( wheel )
				{
					wheel--;
					if ( !wheel )
						{ _up = false; _down = false; }
				}
				missile = false;
				bomb = false;
				return 1;
				break;
			case FL_LEAVE:
				dragging = false;
				_left = _right = _up = _down = false;
				break;
			case FL_DRAG:
			{
				int dx = x - x0;
				int dy = y - y0;
				if ( !dragging && abs( dx ) <= 3 && abs( dy ) <= 3 )
					break;
				dragging = true;
				if ( dx == 0 && dy == 0 )
					break;
				missile = false;
				maybe_missile = false;
				bomb = false;
				if ( dy && abs( dx ) >= abs( dy ) * 4 )
					{ dy = 0; }
				else if ( dx && abs( dy ) >= abs( dx ) * 4 )
					{ dx = 0; }
				if ( dx < 0 )
					{ _left = true; _right = false; }
				if ( dx > 0 )
					{ _right = true; _left = false; }
				if ( dy < 0 )
					{ _up = true; _down = false; }
				if ( dy > 0 )
					{ _down = true; _up = false; }
				if ( dx == 0 )
					{ _left = _right = false; }
				if ( dy == 0 )
					{ _up = _down = false; }
				x0 = x;
				y0 = y;
				break;
			}
			case FL_MOUSEWHEEL:
				if ( _state == TITLE )
				{
					( Fl::event_dy() > 0 ) ? toggleUser() : toggleShip();
					break;
				}
				wheel++;
				if ( Fl::event_dy() < 0 )
					{ _up = true; _down = false; }
				else if ( Fl::event_dy() > 0 )
					{ _down = true; _up = false; }
				break;
			case FL_PUSH:
				setPaused( false );
				dragging = false;
				x0 = x;
				y0 = y;
				maybe_missile = Fl::event_button1() || ( x > w() - w() / 4 );
				bomb = Fl::event_button3() || ( x < w() / 4 && y > h() / 4 );
				if ( bomb )
					maybe_missile = false;
				if ( !bomb )
					_left = _right = _up = _down = false;
				break;
			case FL_RELEASE:
				if ( _state == TITLE || _state == SCORE || _state == DEMO || _done )
				{
					if ( x < 40 && y < 40 )
					{
						hide();
						return 1;
					}
					else
					{
						setPaused( false );
						keyClick();
						onActionKey();
					}
					break;
				}
				if ( !dragging )
					missile = maybe_missile;
				if ( maybe_missile)	//???
					_speed_right = 0;	//???
				if ( !bomb )
					_left = _right = _up = _down = false;
				dragging = false;
				maybe_missile = false;
				bomb = false;
				break;
		}
	}	// _mouseMode

	if ( FL_FOCUS == e_ )
	{
		Fl::focus( this );
		onGotFocus();
		return Inherited::handle( e_ );
	}
	if ( FL_UNFOCUS == e_ )
	{
		onLostFocus();
		return Inherited::handle( e_ );
	}
	int c = Fl::event_key();
//	printf( "event %d: key = 0x%x\n", e_, c );
	if ( ( e_ == FL_KEYDOWN || e_ == FL_KEYUP ) && c == FL_Escape )
		// get rid of escape key closing window (except in boss mode and title screen)
		return ( _enable_boss_key || _state == TITLE || fullscreen_active() ) ?
			Inherited::handle( e_ ) : 1;

	if ( e_ == FL_KEYUP && _state != SCORE && 's' == c )
	{
		if ( _state == TITLE )
			keyClick();
		toggleSound();
		if ( _state == TITLE )
		{
			keyClick();
			onTitleScreen();	// update display
		}
	}
	if ( e_ == FL_KEYUP && _state != SCORE && 'b' == c )
	{
		toggleBgSound();
	}
	if ( e_ == FL_KEYUP && F10_KEY == c && ( _state != LEVEL || G_paused ) )
	{
		toggleFullscreen();
	}
	if ( _state == TITLE || _state == SCORE || _state == DEMO || _done )
	{
		if ( _state == SCORE && e_ == FL_KEYUP && c >= '0' && c < 'z' )
		{
			keyClick();
			_input += c;
		}
		if ( _state == SCORE && e_ == FL_KEYUP && c == FL_BackSpace && _input.size() )
		{
			keyClick();
			_input.erase( _input.size() - 1 );
		}
		if ( _state == TITLE && e_ == FL_KEYUP )
		{
			if ( KEY_LEFT == c )
				toggleUser( true );
			else if ( KEY_RIGHT == c )
				toggleUser();
			else if ( KEY_UP == c )
				toggleShip( true );
			else if ( KEY_DOWN == c )
				toggleShip();
			else if ( 'r' == c )
				resetUser();
			else if ( 'd' == c )
				cb_demo( this );
			else if ( '-' == Fl::event_text()[0] )
				setup( FPS - 1, false, _USE_FLTK_RUN );
			else if ( '+' == Fl::event_text()[0] )
			{
				setup( -FPS - 1, false, _USE_FLTK_RUN );
			}
		}
		if ( e_ == FL_KEYDOWN && ' ' == c )
			{
				setPaused( false );
				ignore_space = true;
				keyClick();
				onActionKey();
			}
		return Inherited::handle( e_ );
	}
	if ( e_ == FL_KEYDOWN )
	{
		if ( F10_KEY != c )
			setPaused( false );
		if ( KEY_LEFT == c )
			_left = true;
		else if ( KEY_RIGHT == c )
		{
			repeated_right++;
			_right = true;
		}
		else if ( KEY_UP == c )
			_up = true;
		else if ( KEY_DOWN == c )
			_down = true;
		return 1;
	}
	if ( e_ == FL_KEYUP )
	{
		if ( KEY_LEFT == c )
			_left = false;
		else if ( KEY_RIGHT == c )
		{
			_right = false;
			_speed_right = 0;
			if ( repeated_right <= 0 )
				fireMissile();
			repeated_right = -1;
		}
		else if ( ' ' == c )
		{
			if ( !ignore_space )
				dropBomb();
			ignore_space = false;
			return 1;
		}
		else if ( KEY_UP == c )
			_up = false;
		else if ( KEY_DOWN == c )
			_down = false;
		else if ( '7' <= c && '9' >= c )
		{
			if ( !_done && !_collision )
			{
				togglePaused();
			}
		}
		return 1;
	}
	return Inherited::handle( e_ );
}

int FltWin::run()
//-------------------------------------------------------------------------------
{
	if ( !Fl::first_window() )
		return 0;

	changeState();

	if ( _USE_FLTK_RUN )
	{
//		printf( "Using Fl::run()\n" );
		Fl::add_timeout( FRAMES, cb_update, this );
		return Fl::run();
	}

	while ( Fl::first_window() )
	{
		_waiter.wait();
		_correct_speed && correctDX();
		_state == DEMO ? onUpdateDemo() : onUpdate();
	}
	return 0;
}

string FltWin::firstTimeSetup()
//-------------------------------------------------------------------------------
{
	string cmd;
	char title[] = "FLTrator first time setup";
	fl_message_title( title );
	int fast_slow = fl_choice( "Do you have a fast computer?\n\n"
	                           "If you have any recent hardware\n"
	                           "you should answer 'yes'.",
		"ASK ME LATER",  "NO", "YES" );
	if ( fast_slow == 2 )
	{
		fl_message_title( title );
		int speed = fl_choice( "*How* fast is your computer?\n\n"
		                       "'Very fast' enables all features,\n"
		                       "'Fast' most features.\n"
		                       "'Normal' disables drawing effects\n\n"
		                       "NOTE: You don't need a gaming pc\n"
		                       "to answer with 'very fast'..",
			"NORMAL", "FAST", "VERY FAST" );
		switch ( speed )
		{
			case 0: // NORMAL
				break;
			case 1: // FAST
				cmd = "-F";
				break;
			case 2: // VERY FAST
				cmd = "-FF";
		}
	}
	else if ( fast_slow == 1 )
	{
		fl_message_title( title );
		int speed = fl_choice( "*How* slow is your computer?\n\n"
		                       "'Slow' lowers the frame rate,\n"
		                       "'Very slow' enables frame correction,\n"
		                       "'Creepy slow' will disable sound too.",
			"SLOW", "VERY SLOW", "CREEPY SLOW" );
		switch ( speed )
		{
			case 0: // SLOW
				cmd = "-S";
				break;
			case 1: // VERY SLOW
				cmd = "-SCb";
				break;
			case 2: // CREEPY SLOW
				cmd = "-SCsbe";
		}
	}
	if ( fast_slow )
	{
		_cfg->set( "defaultArgs", cmd.c_str() );
		fl_message_title( title );
		fl_message( "Configuration saved.\n\n"
		            "To re-run next time start with\n"
		            "'Control-Left' key pressed." );
	}
	return cmd;
}

#include "Fl_Fireworks.H"
//-------------------------------------------------------------------------------
class Fireworks : public Fl_Fireworks
//-------------------------------------------------------------------------------
{
typedef Fl_Fireworks Inherited;
public:
	Fireworks( Fl_Window& win_, bool fullscreen_ = false ) :
		Fl_Fireworks( win_, fullscreen_ ? 2000 : 1000 ),
		_text( 0 )
	{
		callback( cb_fireworks );
		string yearStr;
		time_t t = time( NULL );
		struct tm *tmp;
		tmp = localtime( &t );
		if ( t )
		{
			char buf[20];
			if ( strftime( buf, sizeof(buf), "%Y", tmp ) )
				yearStr = buf;
		}
		string crYearStr( "2015" );
		if ( yearStr > crYearStr )
			crYearStr += ( "-" + yearStr );
		ostringstream welcome;
		welcome << "CG\n© " << crYearStr << "\n\nproudly presents...";
		(_text = new AnimText( 0, win_.h(), win_.w(), welcome.str().c_str(),
			FL_GRAY, FL_RED, 50, 40, false ))->start();
		while ( Fl::first_window() && win_.children() )
#ifndef WIN32
			Fl::wait( 0.0005 );
#else
			Fl::wait( 0.0 );
#endif
	}
	~Fireworks()
	{
		delete _text;
	}
	void draw()
	{
		Inherited::draw();
		_text->draw();
	}
	static void cb_fireworks( Fl_Widget *wgt_, long d_ )
	{
		Fireworks *f = (Fireworks *)wgt_;
		static int flip = 0;
		flip ^= 1;
		switch ( d_ )
		{
			case -1:
				if ( flip )
				{
					f->_text->y( f->_text->y() - 2 );
					if ( f->_text->y() < 30 )
						f->_text->y( f->h() );
				}
				break;
			case 1:
				Audio::instance()->play( "firework_start" );
				break;
			case 2:
			{
				static int flip = 0;
				flip ^= 1;
				Audio::instance()->play( "firework_explode" );
				if ( flip )
					f->_text->color( f->explodeColor() );
				else
					f->_text->bgColor( f->explodeColor() );
				break;
			}
		}
	}
private:
	AnimText *_text;
};

static void signalHandler( int sig_ )
//-------------------------------------------------------------------------------
{
	signal( sig_, SIG_DFL );
	if ( sig_ != SIGINT && sig_ != SIGTERM )
		cerr << endl << "Sorry, FLTrator crashed (signal " << sig_ << ")." << endl;
	Audio::instance()->shutdown();
	restoreScreenResolution();
	exit( -1 );
}

static void installSignalHandler()
//-------------------------------------------------------------------------------
{
	const int signals[] = { SIGABRT, SIGFPE, SIGILL, SIGINT, SIGSEGV, SIGTERM };
	for ( size_t i = 0; i < sizeof( signals ) / sizeof( signals[0] ); i++ )
		signal( signals[i], signalHandler );
}

#ifdef WIN32
#include "win32_console.H"
#endif
//-------------------------------------------------------------------------------
int main( int argc_, const char *argv_[] )
//-------------------------------------------------------------------------------
{
#ifdef WIN32
	Console console;	// output goes to command window (if started from there)
#endif
	installSignalHandler();

	fl_register_images();
	Fl::set_font( FL_HELVETICA, " Verdana" );
	Fl::set_font( FL_HELVETICA_BOLD_ITALIC, "PVerdana" );
	int seed = time( 0 );
	Random::pSrand( seed );

	FltWin fltrator( argc_, argv_ );

	if ( !fltrator.trainMode() && fltrator.gimmicks() )
		Fireworks fireworks( fltrator, fltrator.isFullscreen() );

	int ret = fltrator.run();
	Audio::instance()->shutdown();
	restoreScreenResolution();
	return ret;
}
