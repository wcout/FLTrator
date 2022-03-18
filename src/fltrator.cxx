//
// Copyright 2015-2022 Christian Grabner.
//
// This file is part of FLTrator.
//
// FLTrator is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// FLTrator is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY;  without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details:
// http://www.gnu.org/licenses/.
//
#ifndef VERSION
#define VERSION "v2.4"
#endif

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
#define FLTK_HAS_IMAGE_SCALING FL_MAJOR_VERSION > 1 || \
    (FL_MAJOR_VERSION == 1 && FL_MINOR_VERSION >= 4)

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
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>
#if ( defined APPLE ) || ( defined __linux__ ) || ( defined __MING32__ )
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#endif
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm> // max
#include <ctime>
#include <cstring>
#include <cassert>
#include <signal.h>
#include <climits>
#include <cmath>

#ifdef WIN32
#include <windows.h>
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
// macro to send error output to cerr AND logfile
#define PERR(x) { cerr << x << endl; LOG(x) }

static const unsigned MAX_LEVEL = 10;
static const unsigned MAX_LEVEL_REPEAT = 3;

static const unsigned SCREEN_NORMAL_W = 800;
static const unsigned SCREEN_NORMAL_H = 600;

static unsigned SCREEN_W = SCREEN_NORMAL_W;
static unsigned SCREEN_H = SCREEN_NORMAL_H;

static const int TIMER_CALLBACK = 9999;


#define nbrOfItems( a ) ( sizeof( a ) / sizeof( a[0] ) )

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

static const Fl_Color multicolors[] = {
	fl_rgb_color( 194, 120, 69 ),
	fl_rgb_color( 194, 43, 23 ),
	fl_rgb_color( 194, 66, 5 ),
	FL_YELLOW
};

static const Fl_Color drop_explosion_color[] = { fl_rgb_color( 66, 189, 216 ) };
static const Fl_Color radar_explosion[] = { fl_rgb_color( 153, 204, 255 ), fl_rgb_color( 204, 153, 255 ),
                                      fl_rgb_color( 255, 204, 0 ) };

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

static bool _HAVE_SLOW_CPU =
#ifdef HAVE_SLOW_CPU
	HAVE_SLOW_CPU
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
static double _DDX = DX;	// floating scroll offset

#ifdef NO_MULTIRES
#define SCALE_X 1
#define SCALE_Y 1
#define _YF     1
#else
static double SCALE_X = (double)SCREEN_W / 800;
static double SCALE_Y = (double)SCREEN_H / 600;
static double _YF = 1.;		// ratio screen ratio/original screen ratio
static int MAX_SCREEN_W = 1920;
static int MIN_SCREEN_W = 320;
static int MAX_SCREEN_H = 1200;
#endif

#define KEY_LEFT   KEYSET[G_leftHanded].left
#define KEY_RIGHT  KEYSET[G_leftHanded].right
#define KEY_UP     KEYSET[G_leftHanded].up
#define KEY_DOWN   KEYSET[G_leftHanded].down

using namespace std;

#include "random.H"
#include "fullscreen.H"
#include "joystick.cxx"
#include "fl_joystick.cxx"
#include "resize_image.cxx"
#include "Fl_Waiter.H"

//-------------------------------------------------------------------------------
enum ObjectType
//-------------------------------------------------------------------------------
{
	O_UNDEF = 0,
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
	O_SHIP = 1 << 18,
	O_EXPLOSION = 1 << 19
};

#ifndef NO_MULTIRES
static void flt_font( int f_, int s_ ) { fl_font( f_, lround( SCALE_X * s_ ) ); }

static bool setupScreenSize( int W_, int H_ )
//-------------------------------------------------------------------------------
{
	if ( W_ >= MIN_SCREEN_W && W_ <= MAX_SCREEN_W &&
	     H_ >= W_ / 2 && H_ <= MAX_SCREEN_H && W_ >= H_ )
	{
		SCREEN_W = W_;
		SCREEN_H = H_;
		SCALE_X = (double)SCREEN_W / 800;
		SCALE_Y = (double)SCREEN_H / 600;
		LOG( "SCALE_X: " << SCALE_X );
		LOG( "SCALE_Y: " << SCALE_Y );
		return true;
	}
	else
	{
		PERR( "Warning: Requested screen size " << W_ << " x " << H_ <<
		      " exceeds limits " << MIN_SCREEN_W << "/" << MAX_SCREEN_W <<
		      " x " << MAX_SCREEN_H );
		if ( W_ > 1920 )
		{
			PERR( "" );
			PERR( "   Using a larger screen size than 1920x1200 requires a" << endl <<
			      "   really powerful computer and you need to change the" << endl <<
			      "   allowed sizes in the file 'ini.txt' (in 'levels' dir)" );
		}
	}
	return false;
}

static bool setupScreenSize( const string& cmd_, int& W_, int& H_ )
//-------------------------------------------------------------------------------
{
	W_ = 0;
	H_ = 0;
	int W = 0;
	int H = 0;
	if ( cmd_.empty() )
	{
		int x, y;
		Fl::screen_work_area( x, y, W, H );
	}
	else if ( cmd_ == "f" )
	{
		int x, y;
		Fl::screen_xywh( x, y, W, H );
	}
	else
	{
		W = atol( cmd_.c_str() );
		size_t pos = cmd_.find_first_of( "xX,/" );
		if ( pos != string::npos )
			H = atol( cmd_.substr( pos + 1 ).c_str() );
		if ( !H )
			H = SCREEN_NORMAL_H * W / SCREEN_NORMAL_W;
	}
	if ( setupScreenSize( W, H ) )
	{
		W_ = W;
		H_ = H;
		LOG( "setupScreenSize " << W << "x" << H );
		return true;
	}
	return false;
}
#else
#define flt_font( f_, s_ ) fl_font( f_, s_ )
#endif

static void setup( int fps_, bool have_slow_cpu_, bool use_fltk_run_ )
//-------------------------------------------------------------------------------
{
	_HAVE_SLOW_CPU = have_slow_cpu_;
	_USE_FLTK_RUN = use_fltk_run_;
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
	while ( 1 )
	{
		FRAMES = 1. / FPS;
		DX = 200 / FPS;
		SCORE_STEP = (int)( 200. / (double)DX ) * DX;
		_DDX = SCALE_X * ( 200. / FPS );
		_DX = lround( _DDX );
		if ( _DX ) break;
		FPS /= 2;
	}
	SCORE_STEP = lround( SCALE_X * SCORE_STEP );
	LOG( "setup: FRAMES = " << FRAMES << " _DDX = " << _DDX << " SCORE_STEP = " << SCORE_STEP );
#ifndef NO_MULTIRES
	double ro = (double)SCREEN_NORMAL_W / SCREEN_NORMAL_H;
	double r =  (double)SCREEN_W / SCREEN_H;
	_YF = ro / r;
	LOG( "_YF = " << _YF );
#endif
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
			LOG( "use local home dir: " << home );
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
			{
				LOG( "use installed home dir: " << home );
			}
			else
			{
				fl_message( "%s", "Required resources not in place!\nAborting..." );
				exit( EXIT_FAILURE );
			}
		}
	}
	return home;
}

static string asString( long n_ )
//-------------------------------------------------------------------------------
{
	static ostringstream os;
	os.str( "" );
	os << n_;
	return os.str();
}

static string mkPath( const string& dir_, const string& sub_ = "",
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

static void grad_rect( int x_, int y_, int w_, int h_, int H_, bool rev_,
                       Fl_Color c1_, Fl_Color c2_ )
//-------------------------------------------------------------------------------
{
	int H = h_;
	int offs = rev_ ? H_ - h_ : 0;
	// gradient from color c1 (top) ==> color c2 (bottom)
	for ( int h = 0; h < H; h++ )
	{
		fl_color( fl_color_average( c2_, c1_, (double)(h + offs) / H_ ) );
		fl_xyline( x_, y_ + h, x_ + w_ );
	}
}

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
	void ext( const string& ext_ )
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

static string levelPath( const string& file_ = "" )
//-------------------------------------------------------------------------------
{
	return mkPath( "levels", "", file_ );
}

static string demoPath( const string& file_ = "" )
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

static double rangedValue( double value_, double min_, double max_ )
//-------------------------------------------------------------------------------
{
	double v( value_ );
	if ( min_ > max_ )
	{
		double t = min_;
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
	virtual std::ostream &printOn( std::ostream &os_ ) const;
private:
	static bool within( int x_, int y_, const Rect& r_ )
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

inline std::ostream &operator<<( std::ostream &os_, const Rect &r_ )
{ return r_.printOn( os_ ); }

/*virtual*/
ostream &Rect::printOn( ostream &os_ ) const
//--------------------------------------------------------------------------
{
	os_ << "Rect(" << x() << "/" << y() <<  "-" << w() << "x" << h() << ")";
	return os_;
} // printOn

//-------------------------------------------------------------------------------
class Cfg : public Fl_Preferences
//-------------------------------------------------------------------------------
{
	typedef Fl_Preferences Inherited;
public:
	struct User
	{
		User() : score( 0 ), level( 0 ), completed( 0 ), ship( -1 ) {}
		string name;
		int score;
		int level;
		int completed;
		int ship;
	};
	Cfg( const char *vendor_, const char *appl_ );
	void load();
	void writeUser( const string& user_,
	                int score_ = -1,
	                int level_ = -1,
	                int completed_ = 0,
	                int ship_ = -1 );
	const User& readUser( const string& user_ );
	const User& readUser( User& user_ );
	const User& writeUser( User& user_ );
	bool removeUser( const string& user_ );
	size_t non_zero_scores() const;
	const User& best() const;
	unsigned hiscore() const { return best().score; }
	const vector<User>& users() const { return _users; }
	bool existsUser( const string& user_ ) const;
	const string& last_user() const { return _last_user; }
	const string& pathName() const { return _pathName; }
private:
	vector<User> _users;
	string _last_user;
	string _pathName;
};
typedef Cfg::User User;

//-------------------------------------------------------------------------------
// class Cfg : public Fl_Preferences
//-------------------------------------------------------------------------------
Cfg::Cfg( const char *vendor_, const char *appl_ ) :
	Inherited( USER, vendor_, appl_ )
//-------------------------------------------------------------------------------
{
	// Hack to determine the filepath where the preferences
	// are stored. Currently Fl_Preferences has no method to
	// retrieve it other than to request a userdata directory
	// and deduce the location of the .prefs file from that.
	// (NOTE: This will create a currently unneeded sub-directory)
	// Really shouldn't use Fl_Preferences.
	char buf[ 1024 ];
	buf[0] = 0;
	getUserdataPath( buf, sizeof( buf ) );
	_pathName = buf;
	_pathName.erase( _pathName.size() - 1 );	// remove last '/'
	_pathName += ".prefs";
	LOG( "cfg: " << _pathName );
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
		user.get( "ship", e.ship, -1 );
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

bool Cfg::existsUser( const string& user_ ) const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < _users.size(); i++ )
		if ( _users[i].name == user_ )
			return true;
	return false;
}

void Cfg::writeUser( const string& user_,
	                  int score_/* = -1*/,
	                  int level_/* = -1*/,
	                  int completed_/* = 0*/,
                     int ship_/* =-1*/ )
//-------------------------------------------------------------------------------
{
	set( "last_user", user_.c_str() );
	Fl_Preferences user( this, user_.c_str() );
	if ( score_ >= 0 )
		user.set( "score", (int)score_ );
	if ( level_ >= 0 )
		user.set( "level", (int)level_ );
	user.set( "completed", completed_ );
	if ( ship_ >= 0 )
		user.set( "ship", ship_ );
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
	writeUser( user_.name, user_.score, user_.level, user_.completed, user_.ship );
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
	bool play( const char *file_, bool bg_ = false, bool repeat_ = true );
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
	void enable() { disable( false ); bg_disable( false ); }
	void bg_disable( bool disable_ = true ) { _bg_disabled = disable_; }
	void cmd( const string& cmd_ );
	string cmd( bool bg_ = false ) const { return bg_ ? _bgPlayCmd : _playCmd; }
	string ext() const { return _ext; }
	void noExplosions( bool no_explosions_ ) { _no_explosions = no_explosions_; }
	bool noExplosions() const { return _no_explosions; }
	void stop_bg();
	void check( bool killOnly = false );
	void shutdown();
private:
	Audio();
	~Audio();
	bool allowed( const string& f_ ) const;
	void stop( const string& pidfile_ );
	bool kill_sound( const string& pidfile_ );
	static bool terminate_player( pid_t pid_, const string& pidfile_ );
	string _playCmd;
	string _bgPlayCmd;
	string _ext;
	bool _disabled;
	bool _bg_disabled;
	int _id;
	string _bgpidfile;
	vector<string> _retry_bgpidfile;
	string _bgsound;
	bool _repeat;
	bool _no_explosions;
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
	_id( 0 ),
	_repeat( false ),
	_no_explosions( false )
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
		PERR( "Waiting to stop " << _retry_bgpidfile.size() << " bgsound(s)" );
#ifndef WIN32
//		NOTE: WIN32 has no sleep(sec) - it has Sleep(msec).
//		      But anyway this waiting makes only sense for aplay (Linux).
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
	// command 'type' with argument 'arg'
	if ( type.empty() || "cmd" == type )
		playCmd_ = arg;
	else if ( "bgcmd" == type )
		bgPlayCmd_ = arg;
	else if ( "ext" == type )
		ext_ = arg;
	else
		PERR( "Invalid audio command: '" << arg.c_str() << "'" );
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
	"aplay -q -N %f 2>/dev/null"
#endif
	;
	static const string DefaultBgCmd =
#ifdef WIN32
	"playsound %F %p"
#elif __APPLE__
	"play %f"
#else
	// linux
	"aplay -q -N --process-id-file %p %f 2>/dev/null"
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

bool Audio::allowed( const string& f_ ) const
//-------------------------------------------------------------------------------
{
	if ( _no_explosions && f_.find( "x_" ) == 0 )
		return false;
	return true;
}

bool Audio::play( const char *file_, bool bg_/* = false*/, bool repeat_/* = true*/ )
//-------------------------------------------------------------------------------
{
	int ret = 0;
	bool disabled( ( bg_ && _bg_disabled ) || ( !bg_ && _disabled ) );
	if ( !disabled && file_ && allowed( file_ ) )
	{
		string file( file_ );
		string cmd( bg_ ? _bgPlayCmd : _playCmd );
		bool runInBg( false );

		if ( bg_ )
		{
			stop_bg();
			_bgpidfile = bgPidFileName( ++_id );
			_bgsound = file;
			_repeat = repeat_;
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

		// execute command 'cmd'
#ifdef WIN32
		static STARTUPINFO si = { 0 };
		static DWORD dwCreationFlags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS;
		if ( !si.cb )
		{
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW | STARTF_FORCEOFFFEEDBACK;
			si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
			si.wShowWindow = SW_HIDE;

			if ( !getenv( "PLAYSOUND_STANDARD_PRIORITY" ) )
				dwCreationFlags |= ABOVE_NORMAL_PRIORITY_CLASS;
		}
		PROCESS_INFORMATION pi = { 0 };
		if ( ( ret = !CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
		                            dwCreationFlags, NULL, NULL, &si, &pi) ) == 0 )
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
#elif __APPLE__
		ret = system( cmd.c_str() );
#else
		// linux
		ret = system( cmd.c_str() );
#endif
	}
	return !disabled && !ret;
}

/*static*/
bool Audio::terminate_player( pid_t pid_, const string& pidfile_ )
//-------------------------------------------------------------------------------
{
	// kill player with pid 'pid_', pidfile 'pidfile_'
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
		// pidfile not yet created or already gone
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
			if ( _repeat )
			{
				// restart bgsound
				play( _bgsound.c_str(), true );
			}
			else
			{
				_bgpidfile.erase();
			}
		}
	}
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

//-------------------------------------------------------------------------------
class FltImage
//-------------------------------------------------------------------------------
{
public:
	struct ImageInfo
	{
		ImageInfo() : valid( false ), image( 0 ), imageForDrawing( 0 ),
		              orig_image( 0 ), origImageForDrawing( 0 ),
		              frames( 0 ), timeout( 0. ) {}
		bool valid;
		Fl_Shared_Image *image;
		Fl_RGB_Image *imageForDrawing;
		Fl_Shared_Image *orig_image;
		Fl_RGB_Image *origImageForDrawing;
		int frames;
		double timeout;
	};

	FltImage() :
		_image( 0 ),
		_imageForDrawing( 0 ),
		_orig_image( 0 ),
		_origImageForDrawing( 0 ),
		_animate_timeout( 0. ),
		_frames( 0 ),
		_ox( 0 ),
		_w( 0 ),
		_h( 0 ),
		_orig_w( 0 ),
		_orig_h( 0 )
	{
	}
	~FltImage()
	{
		// NOTE: we don't release images, so they stay cached
		//       within Fl_Shared_Image
//		if ( _image )
//			_image->release();
	}
	void nextFrame()
	{
		_ox += _w;
		if ( _ox >= _image->w() )
			_ox = 0;
	}
	void draw( int x_, int y_ )
	{
#if !defined(WIN32) && !defined(FLTK_USES_XRENDER)
		int X = x_;
		int Y = y_;
		int W = _w;
		int H = _h;
		int ox;
		int oy;
		// RGB image must be clipped to screen otherwise drawing artefacts!
#pragma message( "No Xrender support - fix clipping of RGB image with alpha" )
		clipImage( X, Y, W, H, ox, oy, SCREEN_W, SCREEN_H );
		drawImage()->draw( X, Y, W, H, _ox + ox, oy );
#else	// !defined(WIN32) && !defined(FLTK_USES_XRENDER)
		drawImage()->draw( x_, y_, _w, _h, _ox, 0 );
#endif
	}
	bool get( const char *image_, double scale_ = 1. )
	{
		ImageInfo ii = _icache[ image_ ];
		bool image_path_changed( false );
		if ( !ii.valid ) // image not yet cached?
		{
			// load image once and cache it
			Fl_Shared_Image *image = Fl_Shared_Image::get( image_ );
			if ( image && image->count() )
			{
				// check for "animated" image
				ii.frames = 1;
				const char *p = strstr( image_, "_" );
				if ( p && isdigit( p[1] ) )
				{
					// extract frames/delay from image name 'name_<frames>_<delay>.gif' ...
					p++;
					ii.frames = atoi( p );
					p = strstr( p, "_" );
					if ( p )
					{
						p++;
						ii.timeout = (double)atoi( p ) / 1000;
					}
				}
				else
				{
					// Try to read gif comment field for keywords 'frames', 'delay'.
					// NOTE: for now just search in the first 1024 bytes of the file...
					ifstream ifs( image_, ios::binary );
					char buf[1024];
					memset( buf, 0, sizeof( buf ) );
					ifs.read( buf, sizeof( buf ) );
					string s( buf, sizeof( buf ) );
					size_t pos = s.find( "frames=" );
					if ( pos != string::npos )
						ii.frames = atoi( &s.c_str()[pos + 7] );
					if ( ii.frames > 1 )
					{
						/*size_t*/ pos = s.find( "delay=" );
						if ( pos != string::npos )
							ii.timeout = (double)atoi( &s.c_str()[pos + 6] ) / 1000;
					}
				}
				if ( ii.frames < 0 )
					ii.frames = 1;
				if ( ii.timeout && ii.timeout < 0.05 )
					ii.timeout = 0.05;

				// Correct scaling for animated images
#if FLTK_HAS_NEW_FUNCTIONS
				Fl_Image::RGB_scaling( FL_RGB_SCALING_BILINEAR );
#endif
				ii.orig_image = image;
				ii.origImageForDrawing = 0;
				int w = image->w();
				int h = image->h();
				if ( ii.frames > 0 )
				{
					w /= ii.frames;
					_orig_w = w;
					w = lround( SCALE_X * w );
				}
				else
					w = lround( SCALE_X * w );
				w *= scale_;
				h = lround( SCALE_Y * h * scale_ );

				// Scale pixmap image
				image = Fl_Shared_Image::get( image_, w * ii.frames, h );
				assert( image );

				// NOTE: we don't release images, so they stay cached
				//       within Fl_Shared_Image
//				if ( _image )
//					_image->release();
				ii.image = image;
				LOG( "image '" << image_ << "' " << image->w() << "x" << image->h() << " cached" );
#if FLTK_HAS_NEW_FUNCTIONS
				Fl_Image *rgb = ii.orig_image;
				if ( ii.orig_image->count() > 2 )
				{
					// NOTE: Pixmap images used for drawing will only
					//       be converted once from Pixmap to RGB.
					rgb = new Fl_RGB_Image( (Fl_Pixmap *)ii.orig_image );
					assert( rgb );
					ii.origImageForDrawing = (Fl_RGB_Image *)rgb;
				}
				ii.imageForDrawing = (Fl_RGB_Image *)fl_copy_image( rgb, image->w(), image->h() );
#endif

				// save image information to cache
				ii.valid = true;
				_icache[ image_ ] = ii;
			}
		}
		string last_name = name();
		_image = ii.image;
		image_path_changed = name() != last_name;
		if ( image_path_changed )	// don't reset offset if same image was requested
			_ox = 0;
		_animate_timeout = ii.timeout;
		_frames = ii.frames;
		_imageForDrawing = ii.imageForDrawing;
		_orig_image = ii.orig_image;
		_origImageForDrawing = ii.origImageForDrawing;
		_h = _image ? _image->h() : 0;
		_w = _image ? _image->w() : 0;
		if ( _frames > 1 )
			_w = _image->w() / _frames;
		_orig_h = _orig_image ? _orig_image->h() : 0;
		_orig_w = _orig_image ? _orig_image->w() : 0;
		if ( _frames > 1 )
			_orig_w = _orig_image->w() / _frames;

		return image_path_changed;
	}
	bool isTransparent( size_t x_, size_t y_ ) const
	{
		// valid only for pixmaps!
		x_ += _ox; // for animated images use the current frame
#if FLTK_HAS_IMAGE_SCALING
		assert( _image->w() == _image->data_w() && _image->h() == _image->data_h() );
#endif
		assert( (int)x_ < _image->w() && (int)y_ < _image->h() );
		return _image->data()[y_ + 2][x_] == ' ';
	}
	double animate_timeout() const { return _animate_timeout; }
	Fl_Image *drawImage() const
	{
		return _imageForDrawing ? (Fl_Image *)_imageForDrawing : (Fl_Image *)_image;
	}
	Fl_Image *image() const { return (Fl_Image *)_image; }
	Fl_Image *origDrawImage() const
	{
		return _origImageForDrawing ? (Fl_Image *)_origImageForDrawing : (Fl_Image *)_orig_image;
	}
	string name() const { return _image ? _image->name() : ""; }
	int w() const { return _w; }
	int h() const { return _h; }
	int orig_w() const { return _orig_w; }
	int orig_h() const { return _orig_h; }
	static void uncache() { _icache.clear(); }
private:
	Fl_Shared_Image *_image;
	Fl_RGB_Image *_imageForDrawing;
	Fl_Shared_Image *_orig_image;
	Fl_RGB_Image *_origImageForDrawing;
	double _animate_timeout;
	int _frames;
	int _ox;
	int _w;
	int _h;
	int _orig_w;
	int _orig_h;
protected:
	static map<string, ImageInfo> _icache;
};

/*static*/
map<string, FltImage::ImageInfo> FltImage::_icache;

//-------------------------------------------------------------------------------
class Object
//-------------------------------------------------------------------------------
{
public:
	Object( ObjectType o_ = O_UNDEF, int x_ = 0, int y_ = 0, const char *image_ = 0, int w_ = 0, int h_ = 0 );
	virtual ~Object();
	void animate();
	// check for collision with other object
	bool collisionWithObject( const Object& o_ ) const;
	void stop_animate() { Fl::remove_timeout( cb_animate, this ); }
	bool hit();
	int hits() const { return _hits; }
	bool image( const char *image_, double scale_ = 1. );
	bool isTransparent( size_t x_, size_t y_ ) const { return _image.isTransparent( x_, y_ ); }
	bool started() const { return _state > 0; }
	virtual double timeout() const { return _timeout; }
	void timeout( double timeout_ ) { _timeout = timeout_; }
	virtual const char* start_sound() const { return 0; }
	virtual const char* image_started() const { return 0; }
	virtual void init() {}
	void start( size_t speed_ = 1 );
	virtual void update();
	virtual void draw();
	void draw_collision() const;
	bool nostart() const { return _nostart; }
	void nostart( bool nostart_) { _nostart = nostart_; }

	void x( int x_ ) { _x = x_ + _w / 2; _X = _x; }
	void y( int y_ ) { _y = y_ + _h / 2; _Y = _y; }
	int x() const { return _x - _w / 2; }	// left x
	int y() const { return _y - _h / 2; }	// top y

	void cx( int cx_ ) { _x = cx_; _X = cx_; }
	void cy( int cy_ ) { _y = cy_; _Y = cy_; }
	int cx() const { return _x; }	// center x
	int cy() const { return _y; }	// center y

	int w() const { return _w; }
	int h() const { return _h; }

	int state() const { return _state; }
	unsigned speed() const { return _speed; }
	void speed( unsigned speed_ ) { _speed = speed_; }
	const Rect rect() const { return Rect( x(), y(), w(), h() ); }
	Fl_Image *image() const { return _image.drawImage(); }
	Fl_Image *origImage() const { return _image.origDrawImage(); }
	const FltImage& flt_image() const { return _image; }
	string name() const { return _image.name(); }
	ObjectType o() const { return _o; }
	bool isType( ObjectType o_ ) const { return _o == o_; }
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
	double animate_timeout() const { return _image.animate_timeout(); }
	void dxRange( int dxRange_ ) { _dxRange = dxRange_; }
	int dxRange() const { return _dxRange; }
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
	double _X, _Y;
	unsigned _speed;
	int _dxRange;
	unsigned _state;
	long _data1;
	long _data2;
private:
	bool _nostart;
	bool _exploding;	// state exploding
	bool _exploded;
	bool _hit;	// state 'hit' for explosion drawing
	double _timeout;
	FltImage _image;
	int _hits;
};

//-------------------------------------------------------------------------------
// class Object
//-------------------------------------------------------------------------------
Object::Object( ObjectType o_/* = O_UNDEF*/, int x_/* = 0*/ , int y_/* = 0*/,
                const char *image_/* = 0*/, int w_/* = 0*/, int h_/* = 0*/ ) :
	_o( o_ ),
	_x( x_ ),
	_y( y_ ),
	_w( w_ ),
	_h( h_ ),
	_X( x_ ),
	_Y( y_ ),
	_speed( 1 ),
	_dxRange( 0 ),
	_state( 0 ),
	_data1( 0 ),
	_data2( 0 ),
	_nostart( false ),
	_exploding( false ),
	_exploded( false ),
	_hit( false ),
	_timeout( 0.05 ),
	_hits( 0 )
//-------------------------------------------------------------------------------
{
	if ( image_ )
		this->image( image_ );
}

/*virtual*/
Object::~Object()
//-------------------------------------------------------------------------------
{
	Fl::remove_timeout( cb_animate, this );
	Fl::remove_timeout( cb_update, this );
	Fl::remove_timeout( cb_explosion_end, this );
}

void Object::animate()
//-------------------------------------------------------------------------------
{
	if ( G_paused ) return;
	_image.nextFrame();
}

bool Object::collisionWithObject( const Object& o_ ) const
//-------------------------------------------------------------------------------
{
	if ( rect().intersects( o_.rect() ) )
	{
		// if rectangles intersect, check for touch
		const Rect ir( rect().intersection_rect( o_.rect() ) );
		const Rect r_this( rect().relative_rect( ir ) );
		const Rect r_that( o_.rect().relative_rect( ir ) );
		for ( int y = 0; y < r_this.h(); y++ )
		{
			for ( int x = 0; x < r_this.w(); x++ )
			{
				if ( !isTransparent( r_this.x() + x, r_this.y() + y ) &&
				     !o_.isTransparent( r_that.x() + x, r_that.y() + y ) )
					return true;
			}
		}
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

bool Object::image( const char *image_, double scale_/* = 1.*/ )
//-------------------------------------------------------------------------------
{
	assert( image_ );
	bool changed =_image.get( imgPath.get( image_ ).c_str(), scale_ );
	if ( changed )
	{
		Fl::remove_timeout( cb_animate, this );
		_w = _image.w();
		_h = _image.h();
		if ( _image.animate_timeout() )
			Fl::add_timeout( _image.animate_timeout(), cb_animate, this );
	}
	return changed;
}

void Object::start( size_t speed_/* = 1*/ )
//-------------------------------------------------------------------------------
{
	_speed = speed_;
	init();	// give derived object's a chance to initialise something before start
	const char *img = image_started();
	if ( img )
		image( img );
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

/*virtual*/
void Object::draw()
//-------------------------------------------------------------------------------
{
	if ( !_exploded )
	{
		_image.draw( x(), y() );
	}
	if ( _exploding || _hit )
		draw_collision();
}

void Object::draw_collision() const
//-------------------------------------------------------------------------------
{
	int sz = ( w() > h() ? w() : h() ) / 10;
	++sz &= ~1;
	int pts = w() * h() / sz / sz * 20;
	for ( int i = 0; i < pts; i++ )
	{
		unsigned X = Random::pRand() % w();
		unsigned Y = Random::pRand() % h();
		if ( !isTransparent( X, Y ) )
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
	Fl::repeat_timeout( ((Object *)d_)->animate_timeout(), cb_animate, d_ );
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
	virtual const char *start_sound() const { return "x_launch"; }
	virtual const char* image_started() const { return "rocket_launched.gif"; }
	virtual void update()
	{
		if ( G_paused ) return;
		if ( !exploding() && !exploded() )
		{
			size_t delta = ( 1 + _state / 10 ) * _speed;
			if ( delta > 12 )
				delta = 12;
			_y -= ceil( SCALE_Y * delta );
			if ( dxRange() )
				_x += ceil( SCALE_X * rangedRandom( -dxRange(), dxRange() ) );
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
		Inherited( O_RADAR, x_, y_, "radar.gif" )
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
		Inherited( O_DROP, x_, y_, "drop.gif" ),
		_dx( 0 )
	{
	}
	virtual void init()
	{
		if ( dxRange() )
			_dx = rangedRandom( -dxRange(), dxRange() );
	}
	bool dropped() const { return started(); }
	virtual const char *start_sound() const { return "drop"; }
	virtual void update()
	{
		if ( G_paused ) return;
		size_t delta = ( 1 + _state / 10 ) * _speed;
		if ( delta > 12 )
			delta = 12;
		_y += ceil( SCALE_Y * delta );
		_x += ceil( SCALE_X * _dx );
		Inherited::update();
	}
private:
	int _dx;
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
	void turn()	{ _up = !_up; }
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
		if (_up )
			_y -= ceil( SCALE_Y * delta );
		else
			_y += ceil( SCALE_Y * delta );
		if ( dxRange() )
			_x += lround( SCALE_X * rangedRandom( -dxRange(), dxRange() ) );
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
		if (_up )
			_y -= ceil( SCALE_Y * delta );
		else
			_y += ceil( SCALE_Y * delta );
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
	Missile( int x_, int y_, Fl_Color color_ = FL_WHITE ) :
		Object( O_MISSILE, x_, y_, 0, ceil( 20. * SCALE_X ), ceil( 3. * SCALE_Y ) ),
		_ox( x_ ),
		_color( color_ )
	{
		update();
	}
	virtual const char *start_sound() const { return "x_missile"; }
	virtual void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		_x += lround( SCALE_X * 15 );
	}
	int dx() const { return _x - _ox; }
	bool exhausted() const { return dx() > lround( SCALE_X * 450 ); }
	virtual void draw()
	{
		Fl_Color c( _color );
		if ( dx() > lround( SCALE_X * 250 ) )
			c = fl_darker( c );
		if ( dx() > lround( SCALE_X * 350 ) )
			c = fl_darker( c );
		fl_rectf( x(), y(), w(), h(), c );
	}
private:
	int _ox;
	Fl_Color _color;
};

//-------------------------------------------------------------------------------
class Bomb : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;
public:
	Bomb( int x_, int y_ ) :
		Inherited( O_BOMB, x_, y_, "bomb.gif" ),
		_dy( lround( SCALE_Y * 10 ) )
	{
		update();
	}
	virtual const char *start_sound() const { return "x_bomb_f"; }
	virtual void update()
	{
		if ( G_paused ) return;
		_y += _dy;
		if ( _state < 5 )
			_x += lround( SCALE_X * 16 );
		else if ( _state > 15 )
			_x -= lround( SCALE_X * 5 );
		_x -= lround( SCALE_X * 3 );
		_x -= ( lround( SCALE_X * (_speed / 30 ) ) );
		if ( _state % 2)
			_dy += lround( SCALE_Y * 1 );
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
		_disabled( false ),
		_dx( 0 )
	{
		_state = Random::Rand() % 400;
	}
	virtual void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		int state = _state % 40;
		if ( _disabled )
			state = 0;
		if ( state == 0 )
		{
			image( "phaser.gif" );
			if ( dxRange() )
				_dx = rangedRandom( -dxRange(), dxRange() );
		}
		else if ( state == 20 )
			image( "phaser_active.gif" );
		else if ( state == 36 )
			Audio::instance()->play( "phaser" );
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
		if ( state >= 36 )
		{
			fl_color( fl_contrast( FL_BLUE, _bg_color ) );
			fl_line_style( FL_SOLID, lround( SCALE_Y * 3 ) );
			fl_line( cx(), y(), cx() + lround( SCALE_X * _dx ), _max_height );
			fl_line_style( 0 );
		}
	}
private:
	int _max_height;
	Fl_Color _bg_color;
	bool _disabled;
	int _dx;
};

//-------------------------------------------------------------------------------
class Explosion : public Object
//-------------------------------------------------------------------------------
{
	typedef Object Inherited;

	class Particle
	{
	public:
		Particle( double end_r_, double angle_,
		          double speed_, Fl_Color color_ = FL_WHITE,
		          unsigned len_ = 10, bool multicolor_ = false ) :
			start_r( 1. ), end_r( end_r_ ), angle( angle_ ),
			speed( speed_ * 4 ), color( color_ ), len( len_ ), multicolor( multicolor_ ),
			valid( false ), dot( false )
		{
			end_r -= ( Random::pRand() % (int)end_r / 4 );
			r = start_r + Random::pRand() % std::min( len, 5 );
			dot = len <= 5;
			len = ceil( SCALE_Y * len );
			speed *= SCALE_Y;
		}
		double start_r;
		double end_r;
		double angle;
		double speed;
		Fl_Color color;
		int len;
		bool multicolor;
		double r;
		double beg_x, beg_y;
		double end_x, end_y;
		Fl_Color curr_color;
		bool valid;
		bool dot;
	};

public:
	enum ExplosionType
	{
		FALLOUT           = ( 1 << 16 ),
		MC                = ( 1 << 17 ),
		SPLASH            = ( 1 << 18 ),

		STRIKE            = 1,
		DOT               = 2,
		MC_STRIKE         = MC | STRIKE,
		MC_DOT            = MC | DOT,
		SPLASH_STRIKE     = SPLASH | SPLASH,
		FALLOUT_STRIKE    = FALLOUT | STRIKE,
		MC_FALLOUT_STRIKE = MC | FALLOUT | STRIKE,
		MC_FALLOUT_DOT    = MC | FALLOUT | DOT
	};
	Explosion( int x_, int y_, ExplosionType type_ = STRIKE,
		        double strength_ = 1., const Fl_Color * colors_ = 0, int nColors_ = 0 ) :
		Object( O_EXPLOSION, x_, y_, 0, 200, 200 ),
		_type( type_ ),
		_radius( ceil( SCALE_Y * w() * strength_ ) ),
		_colors( colors_ ? colors_ : multicolors ),
		_nColors( colors_ ? nColors_ : nbrOfItems( multicolors ) ),
		_done( false )
	{
		explode( true );
		update();
	}
	virtual const char *start_sound() const
		{ return ( _radius < w() ? "x_explode1" : "x_explode2" ); }
	virtual void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		if ( _state == 2 )	// second stage
			explode();
		vector<Particle> &p_ = _particles;

		_done = true;
		// update particle position/speed
		for ( size_t i = 0; i < p_.size(); i++ )
		{
			Particle& p = p_[i];
			if ( p.r < p.end_r && p.speed > 0.4 )
			{
				p.beg_x = p.r * cos( p.angle * M_PI / 180.0 ) + cx();
				p.beg_y = p.r * sin( p.angle * M_PI / 180.0 ) + cy();
				double f = ( p.end_r - p.r ) / ( p.end_r - p.start_r );
				int mask = 256. * f;
				if ( !mask )
					mask++;
				if ( p.multicolor )
					p.curr_color = _colors[ Random::pRand() % _nColors ];
				else
					p.curr_color = p.color;

				unsigned len = (double)p.len * f;
				p.end_x = ( p.r + len ) * cos( p.angle * M_PI / 180.0 ) + cx();
				p.end_y = ( p.r + len ) * sin( p.angle * M_PI / 180.0 ) + cy();

				p.valid = true;
				if ( p.r < p.end_r / 2 )
				{
					if ( p.speed > 0.4 )
						p.speed += p.dot ? -0.2 : 0.2;
				}
				else
				{
					if ( p.speed > 0.4 )
						p.speed -= p.dot ? -0.2 : 0.2;
				}
				p.r += p.speed;
				_done = false;
			}
			else
			{
				p_.erase( p_.begin() + i );
				i--;
			}
		}
	}
	virtual void draw()
	{
		// draw the particles
		for ( size_t i = 0; i < _particles.size(); i++ )
		{
			Particle& p = _particles[i];
			if ( !p.valid )
				continue;

			fl_color( p.curr_color );

			if ( p.dot )
			{
				fl_pie( p.beg_x, p.beg_y, p.len, p.len, 0., 360. );
			}
			else
			{
				fl_line_style( FL_SOLID, ceil( 3. * SCALE_Y ) );
				fl_line( p.beg_x, p.beg_y, p.end_x, p.end_y );
				fl_line_style( 0 );
			}
		}
	}
	void explode( bool init_ = false )
	{
		// create particles
		bool dot = ( ( _type & 0xffff ) == DOT );
		bool fallout = ( _type & FALLOUT );
		int r = lround( _radius / SCALE_Y );
		int particles = ( r * r ) / ( init_ ? 200 : 400 );
		for ( int i = 0; i < particles; i++ )
		{
			int speed = fallout ? Random::pRand() % 10 + 1 : Random::pRand() % 5 + 3;
			Fl_Color color = _colors[ 0 ];
			bool multicolor = ( _type & MC );
			_particles.push_back( Particle( _radius * ( dot + 1 ),
				( _type & SPLASH ) ? Random::pRand() % 180 + 180 : Random::pRand() % 360,
				speed,
				color,
				( dot ? Random::pRand() % 3 + 3 : speed * 10 ), multicolor ) );
		}
	}
	bool done() const { return _done; }
private:
	std::vector <Particle> _particles;
	ExplosionType _type;
	int _radius;
	const Fl_Color *_colors;
	int _nColors;
	bool _done;
};

//-------------------------------------------------------------------------------
class AnimText : public Object
//-------------------------------------------------------------------------------
{
// NOTE: This is just a quick impl. for showing "animated" text.
//       and should probably be made more flexible. Also the text
//       drawing could be combined with the FltWin::drawText().
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
		flt_font( FL_HELVETICA_BOLD_ITALIC, _sz );
		int W = 0;
		int H = 0;
		fl_measure( _text.c_str(), W, H, 1 );
		fl_color( _bgColor );
		fl_draw( _text.c_str(), _x + ceil( SCALE_Y * 2 ), _y + ceil( SCALE_Y * 2 ), _w, H, FL_ALIGN_CENTER, 0, 1 );
		fl_color( _color );
		fl_draw( _text.c_str(), _x, _y, _w, H, FL_ALIGN_CENTER, 0, 1 );
		if ( W > _w - 30 )
			_maxSize = _sz;
	}
	virtual void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		if ( _done ) return;
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
	virtual double timeout() const { return started() ? 0.02 : 0.1; }
	const char *text() const { return _text.c_str(); }
private:
	string _text;
	Fl_Color _color;
	Fl_Color _bgColor;
	int _maxSize;
	int _minSize;
	bool _once;
	int _sz;
	bool _up;
	int _hold;
	bool _done;
};

//-------------------------------------------------------------------------------
class ImageAnimation
//-------------------------------------------------------------------------------
{
public:
	struct Rect
	{
		Rect( int x_, int y_, int w_, int h_ ) :
			x( x_ ), y( y_ ), w( w_ ), h( h_ ) {}
		int x;
		int y;
		int w;
		int h;
	};

	ImageAnimation( const FltImage &image_,
		             double duration_ = 1.0,
	                bool hq_ = true,
	                int frames_ = 0 ) :
		_done( true ),
		_set( false ),
		_duration( duration_ > 0. ? duration_ : 1.0 ),
		_hq( hq_ ),
		_frames( frames_ > 0 ? frames_ : ( hq_ ? 30 : 15 ) ),
		_x0( 0 ),
		_y0( 0 ),
		_x1( 0 ),
		_y1( 0 ),
		_scalex0( 1. ),
		_scaley0( 1. ),
		_scalex1( 1. ),
		_scaley1( 1. ),
		_frame( 0 ),
		_x( 0 ),
		_y( 0 ),
		_image( 0 )
	{
		_src.get( image_.name().c_str() );
	}
	void setMoveFrom( int x_, int y_ )
	{
		_x0 = x_ + _src.w() / 2;
		_y0 = y_ + _src.h() / 2;
	}
	void setMoveTo( int x_, int y_ )
	{
		_x1 = x_ + _src.w() / 2;
		_y1 = y_ + _src.h() / 2;
	}
	void setSizeMoveFromTo( int x0_, int y0_, int x1_, int y1_,
	                        double scalex0_, double scaley0_,
	                        double scalex1_, double scaley1_ )
	{
		_x0 = x0_;
		_x1 = x1_;
		_y0 = y0_;
		_y1 = y1_;
		_scalex0 = scalex0_;
		_scaley0 = scaley0_;
		_scalex1 = scalex1_;
		_scaley1 = scaley1_;
		_set = true;
	}
	void setSizeMoveFromTo( int x0_, int y0_, int x1_, int y1_,
                           double scale0_, double scale1_ )
	{
		setSizeMoveFromTo( x0_, y0_, x1_, y1_, scale0_, scale0_, scale1_, scale1_ );
	}
	void setSizeMoveFromTo( const Rect& r0_, const Rect& r1_ )
	{
		double scalex0 = (double)r0_.w / _src.orig_w();
		double scaley0 = (double)r0_.h / _src.orig_h();
		double scalex1 = (double)r1_.w / _src.orig_w();
		double scaley1 = (double)r1_.h / _src.orig_h();
		setSizeMoveFromTo( r0_.x + r0_.w / 2, r0_.y + r0_.h / 2,
                         r1_.x + r1_.w / 2, r1_.y + r1_.h / 2,
                         scalex0, scaley0, scalex1, scaley1 );
	}
	~ImageAnimation()
	{
		stop();
		for ( size_t i = 0; i < _cache.size(); i++ )
			delete _cache[ i ];
	}
	void draw()
	{
		if ( _image && !_done )
			_image->draw( _x - _image->w() / 2, _y - _image->h() / 2 );
	}
	bool start()
	{
		stop();
		_frame = 0;
		_done = !_set;
		if ( !_done )
			Fl::add_timeout( _duration / _frames, cb_update, this );
		return _done;
	}
	bool stop()
	{
		Fl::remove_timeout( cb_update, this );
		_done = true;
		if ( _image )
			_image->uncache();
		_image = 0;
		return _done;
	}
	bool update()
	{
		if ( _frame >= _frames )
		{
			return stop();
		}
		else
		{
			int dx = _x1 - _x0;
			int dy = _y1 - _y0;
			_x = _x0 + ceil( ((double)dx / _frames) * _frame );
			_y = _y0 + ceil( ((double)dy / _frames) * _frame );
			if ( (int)_cache.size() <= _frame )	// not all frames cached yet
			{
				double scalex = ( ( _scalex1 - _scalex0 ) / _frames ) * _frame + _scalex0;
				double scaley = ( ( _scaley1 - _scaley0 ) / _frames ) * _frame + _scaley0;
#if FLTK_HAS_NEW_FUNCTIONS
				Fl_Image::RGB_scaling( _hq ? FL_RGB_SCALING_BILINEAR : FL_RGB_SCALING_NEAREST );
#endif
//				_image = _src.origDrawImage()->copy( _src.orig_w() * scalex, _src.orig_h() * scaley );
				_image = fl_copy_image( _src.origDrawImage(), _src.orig_w() * scalex, _src.orig_h() * scaley );
				_cache.push_back( _image );
			}
			else
			{
				_image = _cache[ _frame ];
			}
			_frame++;
		}
		return _done;
	}
	bool done() const { return _done; }
	const FltImage& src() const { return _src; }
private:
	static void cb_update( void *d_ )
	{
		ImageAnimation *this_ptr = (ImageAnimation *)d_;
		if ( !this_ptr->update() )
			Fl::repeat_timeout( this_ptr->_duration / this_ptr->_frames, cb_update, d_ );
	}
private:
	bool _done;
	bool _set;
	double _duration;
	bool _hq;
	int _frames;
	FltImage _src;
	int _x0;
	int _y0;
	int _x1;
	int _y1;
	double _scalex0;
	double _scaley0;
	double _scalex1;
	double _scaley1;
	int _frame;
	int _x;
	int _y;
	vector<Fl_Image*> _cache;
	Fl_Image *_image;
};

//-------------------------------------------------------------------------------
class TerrainPoint
//-------------------------------------------------------------------------------
{
public:
	TerrainPoint( int ground_level_, int sky_level_ = -1, int object_ = 0 ) :
		_ground_level( lround( (double)ground_level_ * SCALE_Y ) ),
		_sky_level( lround( (double)sky_level_ * SCALE_Y ) ),
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
				min_ground = g;
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
	unsigned ls_outline_width;
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
	Spaceship( int x_, int y_, int W_, int H_, int ship_ = 0, bool effects_ = false ) :
		Inherited( O_SHIP, x_, y_,	((string)"spaceship" + asString( ship_ ) + ".gif").c_str() ),
		_W( W_ ),
		_H( H_ ),
		_effects( effects_ ),
		_missilePos( missilePos() ),
		_bombPos( bombPos() ),
		_bombXOffset( lround( SCALE_Y * 10 ) ),
		_explosionColor( FL_CYAN ),
		_missileColor( FL_WHITE ),
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
			_X -= _DDX;
			_X = fmax( _X, 0 );
			_x = lround( _X );
		}
	}
	void right()
	{
		if ( _x < _W / 2 )
		{
			_accel = 3;
			_X += _DDX;
			_X = fmin( _X, _W / 2 );
			_x = lround( _X );
		}
	}
	void up()
	{
		if ( _Y > _h / 2 - 1 )
		{
			_Y -= _DDX * _YF;
			_Y = fmax( _Y, _h / 2 - 1 );
			_y = lround( _Y );
		}
	}
	void down()
	{
		if ( _y < _H )
		{
			_Y += _DDX * _YF;
			_Y = fmin( _Y, _H );
			_y = lround( _Y );
		}
	}
	virtual void draw()
	{
		Inherited::draw();
		if ( _effects && ( _accel || _decel ) && !G_paused )
		{
			fl_color( _accel > _decel ? FL_GRAY : FL_DARK_MAGENTA );
			fl_line_style( FL_DASH, lround( SCALE_Y * 1 ) );
			int y0 = y() + SCALE_Y * 20;
			int l = SCALE_X * 20;
			int x0 = x() + Random::pRand() % 3;
			while ( y0 < y() + h() - SCALE_Y * 10 )
			{
				fl_xyline( x0, y0, x0 + l );
				y0 += SCALE_Y * 8;
				x0 += SCALE_X * 2;
				l += SCALE_X * 8;
			}
			fl_line_style( 0 );
		}
	}
	const Point& missilePoint() const { return _missilePos; }
	const Point& bombPoint() const { return _bombPos; }
	int bombXOffset() const { return _bombXOffset; }
	void bombXOffset( int bombXOffset_ ) { _bombXOffset = bombXOffset_; }
	Fl_Color explosionColor() const { return _explosionColor; }
	void explosionColor( Fl_Color explosionColor_ ) { _explosionColor = explosionColor_; }
	Fl_Color missileColor() const { return _missileColor; }
	void missileColor( Fl_Color missileColor_ ) { _missileColor = missileColor_; }
protected:
	virtual void update()
	{
		if ( G_paused ) return;
		if ( _accel > 0 )
			_accel--;
		if ( _decel > 0 )
			_decel--;
	}
	Point missilePos() const
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
	Point bombPos() const
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
	int _bombXOffset;
	Fl_Color _explosionColor;
	Fl_Color _missileColor;
	int _accel;
	int _decel;
};

//-------------------------------------------------------------------------------
struct DemoDataItem
//-------------------------------------------------------------------------------
{
	DemoDataItem() :
		sx( 0 ), sy( 0 ), bomb( false ), missile( false ),
		seed( 0 ), seed2( 0 ), seed_set( false ) {}
	DemoDataItem( int sx_, int sy_, bool bomb_, bool missile_,
	              uint32_t seed_, uint32_t seed2_ ) :
		sx( sx_ ), sy( sy_ ), bomb( bomb_ ), missile( missile_ ),
		seed( seed_ ), seed2( seed2_ ), seed_set( true ) {}
	int sx;
	int sy;
	bool bomb;
	bool missile;
	uint32_t seed;
	uint32_t seed2;
	bool seed_set;
};

//-------------------------------------------------------------------------------
class DemoData
//-------------------------------------------------------------------------------
{
public:
	DemoData() : _seed( 1 ), _ship( 0 ), _user_completed( 0 ), _classic( false ) {}
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
	void setSeed( unsigned index_, uint32_t seed_, uint32_t seed2_ )
	{
		while ( _data.size() <= index_ )
			_data.push_back( DemoDataItem() );
		if ( _data[ index_ ].seed_set )
			return;
		_data[ index_ ].seed = seed_;
		_data[ index_ ].seed2 = seed2_;
		_data[ index_ ].seed_set = true;
	}
	void set( unsigned index_, int sx_, int sy_, bool bomb_, bool missile_,
	          uint32_t seed_, uint32_t seed2_ )
	{
		DemoDataItem item( sx_, sy_, bomb_, missile_, seed_, seed2_ );
		while ( _data.size() <= index_ )
			_data.push_back( item );
	}
	void seed( uint32_t seed_ )
	{
		_seed = seed_;
	}
	void ship( unsigned ship_ )
	{
		_ship = ship_;
	}
	void user_completed( unsigned user_completed_ )
	{
		_user_completed = user_completed_;
	}
	void classic( bool classic_ )
	{
		_classic = classic_;
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
	bool get( unsigned index_, int &sx_, int &sy_, bool& bomb_, bool& missile_,
	          uint32_t& seed_, uint32_t& seed2_ ) const
	{
		if ( _data.size() > index_ )
		{
			sx_ = _data[ index_ ].sx;
			sy_ = _data[ index_ ].sy;
			bomb_ = _data[ index_ ].bomb;
			missile_ = _data[ index_ ].missile;
			seed_ = _data[ index_ ].seed;
			seed2_ = _data[ index_ ].seed2;
			return true;
		}
		return false;
	}
	uint32_t seed() const
	{
		return _seed;
	}
	unsigned ship() const
	{
		return _ship;
	}
	unsigned user_completed() const
	{
		return _user_completed;
	}
	bool classic() const
	{
		return _classic;
	}
	void clear()
	{
		_seed = 1;
		_ship = 0;
		_user_completed = 0;
		_classic = false;
		_data.clear();
	}
	size_t size() const
	{
		return _data.size();
	}
private:
	uint32_t _seed;
	unsigned _ship;
	unsigned _user_completed;
	bool _classic;
	vector<DemoDataItem> _data;
};

//-------------------------------------------------------------------------------
class IniParameter : public map<string, string>
//-------------------------------------------------------------------------------
{
public:
	IniParameter() : _found( false ) {}
	int value( const string& id_ ) { return strtol( at( id_ ).c_str(), NULL, 0 ); }
	double dvalue( const string& id_ ) { return atof( at( id_ ).c_str() ); }
	int value( const string& id_, int min_, int max_, int default_ )
	{
		_found = false;
		if ( id_.empty() )
			return default_;

		if ( find( id_ ) == end() )
			return default_;

		_found = true;
		int value = rangedValue( this->value( id_ ), min_, max_ );

		return value;
	}
	double value( const string& id_, double min_, double max_, double default_ )
	{
		_found = false;
		if ( id_.empty() )
			return default_;

		if ( find( id_ ) == end() )
			return default_;

		_found = true;
		double value = rangedValue( this->dvalue( id_ ), min_, max_ );

		return value;
	}
	const char* value( const string& id_, size_t max_, const char *default_ )
	{
		_found = false;
		if ( id_.empty() )
			return default_;

		if ( find( id_ ) == end() )
			return default_;

		_found = true;
		string& value = at( id_ );
		if ( value.size() > max_ )
			value.erase( max_ );

		return value.c_str();
	}
	bool found() const { return _found; }
	void found( bool found_ ) { _found = found_; }
private:
	bool _found;
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
	   SCORE,
	   NO_STATE
	};
	FltWin( int argc_ = 0, const char *argv_[] = 0 );
	int run();
	bool trainMode() const { return _trainMode; }
	bool isFullscreen() const { return fullscreen_active() || !border(); }
	bool gimmicks() const { return _gimmicks; }
	void draw_fadeout();
	void draw_tv() const;
	void draw_tvmask() const;
	bool focus_out() const { return _focus_out; }
private:
	void add_score( unsigned score_ );
	void position_spaceship();
	void addScrollinZone();
	void addScrolloutZone();

	State changeState( State toState_ = NEXT_STATE, bool force_ = false );

	int iniValue( const string& id_, int min_, int max_, int default_ );
	double iniValue( const string& id_, double min_, double max_, double default_ );
	const char *iniValue( const string& id_, size_t max_, const char* default_ );
	void init_parameter();
	bool loadDefaultIniParameter();
	bool loadTranslations();
	bool loadLevel( unsigned level_, string& levelFileName_ );
	bool validDemoData( unsigned level_ = 0 );
	unsigned pickRandomDemoLevel( unsigned minLevel_ = 0, unsigned maxLevel_ = 0 );
	string demoFileName( unsigned  level_ = 0 ) const;
	bool loadDemoData( unsigned level_ = 0, bool dryrun_ = false );
	bool saveDemoData() const;
	bool collisionWithTerrain( const Object& o_ ) const;

	void create_explosion( int x_, int y_, Explosion::ExplosionType type_,
		                    double strength_ = 1.0, const Fl_Color *colors_ = 0, int nColors = 0 );
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
	void clear_level_image_cache();
	Fl_Image *terrain_as_image();
	Fl_Image *landscape_as_image();
	Fl_Image *background_as_image();
#endif
	bool create_terrain();
	void create_level();
	bool revert_level();

	void draw_badies() const;
	void draw_bombs() const;
	void draw_cumuluses() const;
	void draw_drops() const;
	void draw_explosions() const;
	void draw_missiles() const;
	void draw_phasers() const;
	void draw_radars() const;
	void draw_rockets() const;
	void draw_objects( bool pre_ ) const;

	int drawText( int x_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const;
	int drawTextBlock( int x_, int y_, const char *text_, size_t sz_, int line_height_, Fl_Color c_, ... ) const;
	int drawTable( int w_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const;
	int drawTableBlock( int w_, int y_, const char *text_, size_t sz_, int line_height_, Fl_Color c_, ... ) const;
	void draw_score();
	void draw_scores();
	void draw_title();
	void draw_shaded_background( int xoff_, int W_ );
	void draw_shaded_landscape( int xoff_, int W_ );
	void draw_outline( int xoff_, int W_, int outline_width_,
	                   Fl_Color outline_color_sky_, Fl_Color outline_color_ground ) const;
	void draw_landscape( int xoff_, int W_ );
	bool draw_decoration();
	void draw();
	void do_draw();

	string firstTimeSetup();

	void update_badies();
	void update_bombs();
	void update_cumuluses();
	void update_drops();
	void update_explosions();
	void update_missiles();
	void update_phasers();
	void update_radars();
	void update_rockets();
	void update_objects();

	bool dropBomb();
	bool fireMissile();
	int handle( int e_ );
	void keyClick() const;

	void onActionKey( bool delay_ = true );
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

	void setIcon();
	void setTitle();
	void newUser();
	void setUser();
	void resetUser();
	bool toggleBorder();
	bool toggleFullscreen();
	bool setFullscreen( bool fullscreen_ );
	virtual void show();
	void toggleShip( bool prev_ = false );
	void toggleUser( bool prev_ = false );

	bool zoominShip( bool updateOrigin_ );
	bool zoomoutShip();

	bool paused() const { return _state == PAUSED; }
	void bombUnlock();
	static void cb_action_key_delay( void *d_ );
	static void cb_fire_key_delay( void *d_ );
	static void cb_bomb_unlock( void *d_ );
	static void cb_demo( void *d_ );
	static void cb_paused( void *d_ );
	static void cb_update( void *d_ );
	State state() const { return _state; }
	State last_state() const { return _last_state; }
	int ship() const	{ return _state == DEMO ? _demoData.ship() :	( _user.ship < 0  ? _ship : _user.ship );	}
	bool classic() const { return _state == DEMO ? _demoData.classic() : _classic; }
	unsigned user_completed() const { return _state == DEMO ? _demoData.user_completed() : _user.completed; }
	unsigned endLevel() const { return reversLevel() ? 1 : MAX_LEVEL; }
	bool reversLevel() const { return _internal_levels ? false : ( user_completed() % 2 != 0 ); }
	int levelIncrement() const { return reversLevel() ? -1 : 1; }

	void setPaused( bool paused_ );
	void toggleBgSound() const;
	void toggleSound() const;
	void togglePaused();
	void setBgSoundFile();
	void startBgSound() const;
#ifdef NO_MULTIRES
#define flt_draw( buf_, n_, x_, y_ ) fl_draw( buf_, n_, x_, y_ )
#else
	void flt_draw( const char* buf_, int n_, int x_, int y_ )
	{
		x_ = lround( SCALE_X * x_ );
		y_ = lround( SCALE_Y * y_ );
		if ( x_ < 0 )
			x_ = w() + x_;
		if ( y_ < 0 )
			y_ = h() + y_;
		fl_draw( buf_, n_, x_, y_ );
	}
#endif
protected:
	Terrain T;
	Terrain TBG;
	vector<Missile *> Missiles;
	vector<Bomb *> Bombs;
	vector<Rocket *> Rockets;
	vector<Phaser *> Phasers;
	vector<Radar *> Radars;
	vector<Drop *> Drops;
	vector<Explosion *> Explosions;
	vector<Bady *> Badies;
	vector<Cumulus *> Cumuluses;

	int _rocket_start_prob;
	int _rocket_radar_start_prob;
	int _rocket_min_start_speed;
	int _rocket_max_start_speed;
	int _rocket_max_start_dist;
	int _rocket_min_start_dist;
	int _rocket_var_start_dist;
	int _rocket_dx_range;

	int _drop_start_prob;
	int _drop_min_start_speed;
	int _drop_max_start_speed;
	int _drop_max_start_dist;
	int _drop_min_start_dist;
	int _drop_var_start_dist;
	int _drop_dx_range;

	int _bady_min_start_speed;
	int _bady_max_start_speed;
	int _bady_min_hits;
	int _bady_max_hits;
	int _bady_dx_range;

	int _cumulus_min_start_speed;
	int _cumulus_max_start_speed;

	int _phaser_dx_range;
private:
	State _state;
	State _last_state;
	int _xoff;
	int _draw_xoff;
	int _xdelta;
	double _dxoff;
	int _final_xoff;
	bool _left, _right, _up, _down;
	Spaceship *_spaceship;
	int _ship;
	Rocket& _rocket;
	Phaser& _phaser;
	Radar& _radar;
	Drop& _drop;
	Bady& _bady;
	Cumulus& _cumulus;
	bool _bomb_lock;
	bool _collision;	// level ended with collision
	bool _done;			// level ended by finishing
	unsigned _frame;
	unsigned _hiscore;
	string _hiscore_user;
	unsigned _first_level;
	unsigned _end_level;
	unsigned _level;
	bool _completed;
	unsigned _done_level;
	unsigned _level_repeat;
	int _bonus;
	bool _cheatMode;
	bool _trainMode;	// started with level specification
	bool _mouseMode;
	bool _joyMode;
	bool _gimmicks;
	int _effects;
	bool _scanlines;
	bool _tvmask;
	bool _faintout_deco;
	bool _classic;
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
	IniParameter _ini;	// ini section of level file
	IniParameter _defaultIniParameter;
	IniParameter _texts;
	AnimText *_anim_text;
	ImageAnimation *_zoomoutShip;
	ImageAnimation *_zoominShip;
	bool _disableKeys;
	string _bgsound;
	DemoData _demoData;
	Fl_Image *_terrain;
	Fl_Image *_landscape;
	Fl_Image *_background;
	User _user;
	static Fl_Waiter _waiter;
	Fl_Joystick _joystick;
	bool _showFirework;
	string _lang;
	unsigned _alpha_matte;
	double _TO;
	vector<int> _colorChangeList;
	bool _exit_demo_on_collision; // set to true, if demo should end at collision
	bool _dimmout;
};

/*static*/ Fl_Waiter FltWin::_waiter;

#include "Fl_Fireworks.H"
//-------------------------------------------------------------------------------
class Fireworks : public Fl_Fireworks
//-------------------------------------------------------------------------------
{
typedef Fl_Fireworks Inherited;
public:
	Fireworks( FltWin& win_, const char *greeting_ = "", bool fullscreen_ = false ) :
		Inherited( win_, fullscreen_ ? 20.0 : 10.0 ),
		_text( 0 ), _fltwin( &win_ )
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
		welcome << "CG\n© " << crYearStr << "\n\n" << greeting_;
		(_text = new AnimText( 0, win_.h(), win_.w(), welcome.str().c_str(),
			FL_GRAY, FL_RED, 50, 40, false ))->start();
		while ( Fl::first_window() && win_.children() )
		{
			if ( Fl::focus() != &win_ && win_.focus_out() )
				break;
#ifndef WIN32
			Fl::wait( 0.0005 );
#else
			Fl::wait( 0.0 );
#endif
		}
	}
	~Fireworks()
	{
		delete _text;
	}
	void draw()
	{
		Inherited::draw();
		_text->draw();
		_fltwin->draw_tv();
	}
	int handle( int e_ )
	{
		if ( e_ == FL_JOY_BUTTON_UP )
			e_ = FL_RELEASE;
		return Inherited::handle( e_ );
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
					f->_text->y( f->_text->y() - f->scale_y() * 2 );
					if ( f->_text->y() < f->scale_y() * 30 )
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
	double scale_x() const { return SCALE_X; }
	double scale_y() const { return SCALE_Y; }
private:
	AnimText *_text;
	FltWin *_fltwin;
};

//-------------------------------------------------------------------------------
// class FltWin : public Fl_Double_Window
//-------------------------------------------------------------------------------
FltWin::FltWin( int argc_/* = 0*/, const char *argv_[]/* = 0*/ ) :
	Inherited( SCREEN_W, SCREEN_H, "FLTrator " VERSION ),
	_state( START ),
	_last_state( NO_STATE ),
	_xoff( 0 ),
	_draw_xoff( 0 ),
	_xdelta( 0 ),
	_dxoff( 0. ),
	_final_xoff( 0 ),
	_left( false), _right( false ), _up( false ), _down( false ),
	_spaceship( 0 ),
	_ship( 0 ),
	_rocket( *new Rocket() ),
	_phaser( *new Phaser() ),
	_radar( *new Radar() ),
	_drop( *new Drop() ),
	_bady( *new Bady() ),
	_cumulus( *new Cumulus() ),
	_bomb_lock( false),
	_collision( false ),
	_done( false ),
	_frame( 0 ),
	_hiscore( 0 ),
	_first_level( 1 ),
	_end_level( MAX_LEVEL ),
	_level( 0 ),
	_completed( false ),
	_done_level( 0 ),
	_level_repeat( 0 ),
	_bonus( 0 ),
	_cheatMode( false ),
	_trainMode( false ),
	_mouseMode( false ),
	_joyMode( false ),
	_gimmicks( true ),
	_effects( 0 ),
	_scanlines( false ),
	_tvmask( false ),
	_faintout_deco( true ),
	_classic( false ),
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
	_zoomoutShip( 0 ),
	_zoominShip( 0 ),
	_disableKeys( false ),
	_terrain( 0 ),
	_landscape( 0 ),
	_background( 0 ),
	_showFirework( true ),
	_alpha_matte( 0 ),
	_TO( 0. ),
	_exit_demo_on_collision( false ),
	_dimmout( false )
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

	// read ini file
	loadDefaultIniParameter();
	_ini = _defaultIniParameter;
	_lang = _ini.value( "lang", 3, "" );
	loadTranslations();

#ifndef NO_MULTIRES
	// override max allowed screen dimensions from ini
	MAX_SCREEN_W = _ini.value( "MAX_SCREEN_W", 800, 3840, 1920 );
	MIN_SCREEN_W = _ini.value( "MIN_SCREEN_W", 320, 640, 320 );
	MAX_SCREEN_H = _ini.value( "MAX_SCREEN_H", 600, 2160, 1200 );
#endif

	// override fltkWaitDelay from ini
	double fltkWaitDelay = _ini.value( "fltk_wait_delay", 0.0, 0.01, _waiter.fltkWaitDelay() );
	_waiter.fltkWaitDelay( fltkWaitDelay );

#ifdef WIN32
	if ( getenv( "FLTRATOR_SET_PRIORITY" ) )
		SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
#endif

	string defaultArgs;
	string cfgName( "fltrator" );
	_cfg = new Cfg( "CG", cfgName.c_str() );
	char *value = 0;
	int ret = _cfg->get( "defaultArgs", value, "" );

	// set default spaceship image as icon
	setIcon();

	if ( !ret || Fl::get_key( FL_Control_L ) )
	{
		if ( argc <= 1 )
		{
			defaultArgs = firstTimeSetup();
			_cfg->flush();
		}
	}
	if ( defaultArgs.empty() )
	{
		defaultArgs = value;
	}
	free( value );
	delete _cfg;
	_cfg = 0;
	trim( defaultArgs );
	LOG( "defaultArgs: '" << defaultArgs << "'" );

	// use Verdana as application font (or value from ini file)
	const char *defaultFont = Fl::get_font( FL_HELVETICA );
	bool oldFontNameFormat = defaultFont && defaultFont[0] == ' '; // Xft format?
	static string font = _ini.value( "font", 50, oldFontNameFormat ? " Verdana" : "Verdana" );
	static string ifont = _ini.value( "ifont", 50, oldFontNameFormat ? "PVerdana" : "Verdana bold italic" );
	Fl::set_font( FL_HELVETICA,  font.c_str() );	// font name must be static!
	Fl::set_font( FL_HELVETICA_BOLD_ITALIC, ifont.c_str() );

	string defaultArgsSave = defaultArgs;	// needed for --info
	while ( defaultArgs.size() || argc > 1 )
	{
		if ( unknown_option.size() )
			break;
		if ( argc > 1 )
		{
			arg = argv_[argi++];
			--argc;
			if ( arg[0] == '-' && ( arg != "-i" && arg[1] != 'U' ) )
				defaultArgs.erase();
		}
		else
		{
			string defaultArg;
			size_t pos = defaultArgs.find( ' ' );
			if ( pos != string::npos )
			{
				defaultArg = defaultArgs.substr( 0, pos );
				defaultArgs.erase( 0, pos + 1 );
				trim( defaultArgs );
			}
			else
			{
				defaultArg = defaultArgs;
				defaultArgs.erase();
			}
			trim( defaultArg );
			arg = defaultArg;
		}
		if ( arg.find( "--" ) == 0 )
		{
			string longopt = arg.substr( 2 );
			if ( (string("help")).find( longopt ) == 0 || "-h" == arg )
			{
				usage = true;
				break;
			}
			else if ( (string("info")).find( longopt ) == 0 )
			{
				info = true;
				break;
			}
			else if ( (string("version")).find( longopt ) == 0 )
			{
				cout << VERSION << endl;
				exit( EXIT_SUCCESS );
			}
			else if ( (string("classic")).find( longopt ) == 0 )
			{
				_classic = true;
			}
			else
			{
				unknown_option = arg;
			}
			continue;
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
		else if ( arg.find( "-R" ) == 0 || arg.find( "-r" ) == 0 )
		{
			setup( atoi( arg.substr( 2 ).c_str() ), _HAVE_SLOW_CPU, arg[1] == 'r' );
		}
#ifndef NO_MULTIRES
		else if ( arg.find( "-W" ) == 0 )
		{
			int W, H;
			if ( setupScreenSize( arg.substr( 2 ), W, H ) )
			{
				// TODO: leak
				FltImage::uncache();
				_rocket = *new Rocket();
				_phaser = *new Phaser();
				_radar = *new Radar();
				_drop = *new Drop();
				_bady = *new Bady();
				_cumulus = *new Cumulus();
				size( W, H );
				setup( FPS, _HAVE_SLOW_CPU, _USE_FLTK_RUN );
			}
		}
#endif
		else if ( arg[0] == '-' )
		{
			for ( size_t i = 1; i < arg.size(); i++ )
			{
				switch ( arg[i] )
				{
					case 'a':
						_ship = 1;
						break;
					case 'b':
						Audio::instance()->bg_disable();
						break;
					case 'c':
						_hide_cursor = true;
						break;
					case 'C':
						_correct_speed = true;
						_USE_FLTK_RUN = false;
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
						setup( -1, _HAVE_SLOW_CPU, _USE_FLTK_RUN );
						_gimmicks = true;
						_effects++;
						break;
					case 'i':
						_internal_levels = true;
						break;
					case 'j':
						_joyMode = true;
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
						setup( -1, true, _USE_FLTK_RUN );
						_gimmicks = false;
						_effects = 0;
						break;
					case 'X':
						Audio::instance()->noExplosions( true );
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
		const char *name = fl_filename_name( argv_[0] );
		cout << "Usage:" << endl
		     << "  " << name << " [level] [levelfile] [options]" << endl << endl
		     << "\tlevel      [1, 10] startlevel" << endl
		     << "\tlevelfile  name of levelfile (for testing a new level)" << endl
		     << "Options:" << endl
		     << "  -a\tuse alternate (penetrator like) ship image" << endl
		     << "  -b\tdisable background sound" << endl
		     << "  -c\thide mouse cursor in window" << endl
		     << "  -e\tenable Esc as boss key" << endl
		     << "  -d\tdisable auto demo" << endl
		     << "  -f\tenable full screen mode" << endl
		     << "  -h\tdisplay this help" << endl
		     << "  -i\tplay internal (auto-generated) levels" << endl
		     << "  -l\tleft handed play" << endl
		     << "  -j\tuse joystick for input" << endl
		     << "  -m\tuse mouse for input" << endl
		     << "  -n\treset user (only if startet with -U)" << endl
		     << "  -o\tdo not position window" << endl
		     << "  -p\tdisable pause on focus out" << endl
		     << "  -q\tdisable ramdomness" << endl
		     << "  -s\tstart with sound disabled" << endl
		     << "  -x\tdisable gimmick effects" << endl
		     << "  -A\"playcmd\"\tspecify audio play command" << endl
		     << "   \te.g. -A\"cmd=playsound -q %f; bgcmd=playsound -q %f %p; ext=wav\"" << endl
		     << "  -C\tuse speed correction measurement" << endl
		     << "  -F{F}\trun with settings for fast computer (turns on most {all} features)" << endl
		     << "  -R/r{value}\tset runtype (r=FLTK, R=custom) and frame rate to 'value' fps [20,25,40,50,100,200]" << endl
		     << "  -S\trun with settings for slow computer (turns off gimmicks/features)" << endl
		     << "  -Uusername\tstart as user 'username'" << endl
#ifndef NO_MULTIRES
		     << "  -Wwidthxheight\tuse screen size width x height" << endl
		     << "  -W\tuse whole screen work area as screen size" << endl
		     << "  -Wf\tuse full screen area as screen size" << endl
#endif
		     << "  -X\tturn off explosion sounds (for a more chilled experience)" << endl
		     << endl
		     << "  --classic\tplay in classic look (same color for landscape/sky/ground + outline)" << endl
		     << "  --help\tprint out this text and exit" << endl
		     << "  --info\tprint out some runtime information and exit" << endl
		     << "  --version\tprint out version  and exit" << endl;
		exit( EXIT_SUCCESS );
	}

	if ( _internal_levels )
		cfgName += "_internal";	// don't mix internal levels with real levels
	_cfg = new Cfg( "CG", cfgName.c_str() );

	if ( info )
	{
		cout << "fltkWaitDelay = " << _waiter.fltkWaitDelay() << endl
		     << "USE_FLTK_RUN  = " << _USE_FLTK_RUN << endl
		     << "DX            = " << DX << endl
		     << "FRAMES        = " << FRAMES << endl
		     << "FPS           = " << FPS << endl
		     << "Home dir      = " << homeDir() << endl
		     << "Config file   = " << _cfg->pathName() << endl
		     << "Default cmd   = '" << defaultArgsSave << "'" << endl
		     << "Audio::cmd    = '" << Audio::instance()->cmd() << "'" << endl
		     << "Audio::bg_cmd = '" << Audio::instance()->cmd( true ) << "'" << endl
		     << "FLTK version  = " << Fl::version() << endl;
#if FLTK_HAS_NEW_FUNCTIONS
		cout << "FLTK_HAS_NEW_FUNCTIONS" << endl;
#endif
#ifdef NO_PREBUILD_LANDSCAPE
		cout << "NO_PREBUILD_LANDSCAPE" << endl;
#endif
		exit( EXIT_SUCCESS );
	}
	// Issue warnings for non sensible option combinations
	if ( _joyMode && _mouseMode )
	{
		PERR( "Warning: Both joystick AND mouse control enabled." );
	}
	if ( _mouseMode && _hide_cursor)
	{
		PERR( "Warning: Both mouse control and hide cursor enabled." );
	}

	// let user disable scanline effect (or turn it on with -FFF )
	_scanlines = _ini.value( "scanlines", 0, 1, ( _effects > 2 ) );
	// tvmask effect can only be turned on with ini file currently
	_tvmask = _ini.value( "tvmask", 0, 1, false );
	// faintout deco can be turned off
	_faintout_deco = _ini.value( "faintout_deco", 0, 1, true );

	if ( _joyMode )
		_joystick.attach();

	if ( _trainMode )
		_enable_boss_key = true; 	// otherwise no exit!
	else
		_levelFile.erase();

	if ( _user.name.empty() && !_trainMode )
		_user.name = _cfg->last_user();

	if ( _user.name.empty() && !_trainMode )
		_user.name = DEFAULT_USER;

	if ( !_trainMode && !reset_user )
	{
		setUser();
		_ship = _user.ship < 0 ? _ship : _user.ship;
	}

	create_spaceship();

	// set current spaceship image as icon
	setIcon();

	_level = _first_level;
	_hiscore = _cfg->hiscore();
	_hiscore_user = _cfg->best().name;
	wavPath.ext( Audio::instance()->ext() );

	fl_make_path( demoPath().c_str() );	// create demo path for sure

	Fl::visual( FL_DOUBLE | FL_RGB );

	setFullscreen( fullscreen );
	show();
}

void FltWin::onStateChange( State from_state_ )
//-------------------------------------------------------------------------------
{
	_last_state = from_state_;
	switch ( _state )
	{
		case TITLE:
			_dimmout = G_paused;
			Audio::instance()->stop_bg();
			if ( !G_paused && _gimmicks && Fl::focus() == this &&
		        from_state_ != NO_STATE )
			{
				if ( !_trainMode && from_state_ != DEMO && _showFirework )
				{
					_showFirework = false;
					_disableKeys = true;
					Fireworks fireworks( *this, _texts.value( "greeting", 50,
					                     "proudly presents..." ), isFullscreen() );
					_disableKeys = false;
				}
				if ( shown() )
				{
					Audio::instance()->play( "menu" );
					setTitle();
				}
			}
			onTitleScreen();
			break;
		case SCORE:
			_dimmout = false;
			Fl::remove_timeout( cb_demo, this );
			_showFirework = true;
			break;
		case LEVEL:
			_dimmout = false;
			onNextScreen( ( from_state_ != PAUSED || _done ) );
			break;
		case DEMO:
			setPaused( false );
			onDemo();
			break;
		case LEVEL_FAIL:
			Audio::instance()->play( "x_ship" );
			if ( _level_repeat + 1 > MAX_LEVEL_REPEAT )
			{
				Audio::instance()->play( "x_lost" );
				_showFirework = true;
			}
			static Fl_Color spaceship_explode[] = { FL_WHITE, FL_CYAN };
			spaceship_explode[1] = _spaceship->explosionColor();
			create_explosion( _spaceship->cx(), _spaceship->cy(),
				Explosion::MC_FALLOUT_STRIKE, 2.0,
				spaceship_explode, nbrOfItems( spaceship_explode ) );
			Audio::instance()->stop_bg();
		case LEVEL_DONE:
		{
			if ( _state == LEVEL_DONE )
			{
				Audio::instance()->stop_bg();
				string done_sound( _level == _end_level ? "win" : "done" );
				Audio::instance()->play( done_sound.c_str(), true, true ); // bgsound/once
				_done_level = _level;
				_first_level = _done_level + levelIncrement();
				if ( !_trainMode )
				{
					_user.completed += _done_level == _end_level;
					if ( _done_level == _end_level )
						_done_level = 0;
					_user.level = _done_level;
					_cfg->writeUser( _user );
					_cfg->flush();
				}
			}
			if ( _first_level > MAX_LEVEL || _first_level < 1 )
			{
				_first_level = reversLevel() ? MAX_LEVEL : 1;
				_showFirework = true;
			}
			double TO = _defaultIniParameter.value( "wait_time_fail", 1.0, 5.0, 2.0 );
			if ( _state == LEVEL_FAIL && _level_repeat + 1 > MAX_LEVEL_REPEAT )
				TO *= 2;
			if ( _state == LEVEL_DONE && _level == _end_level )
				TO = 60.0;
			_TO = TO;
			Fl::add_timeout( TO, cb_paused, this );
			changeState( PAUSED );
			if ( _joyMode && !_joystick.isAttached() )
				_joystick.attach();
			break;
		}
		case PAUSED:
			_alpha_matte = 0;
			if ( _done && !_completed )
				_dimmout = true;
			break;
		default:
			break;
	}
}

FltWin::State FltWin::changeState( State toState_/* = NEXT_STATE*/,
                                   bool force_/* = false*/ )
//-------------------------------------------------------------------------------
{
	State state = force_ ? NO_STATE : _state;
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
		if ( state != toState_ )
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
	int X = _spaceship->w() + 10; // on left side of terrain
	int ground = T[X].ground_level();
	int sky = T[X].sky_level();
	int Y = ground + ( h() - ground - sky ) / 2; // halfway between ground/sky
	// position center of spaceship at X/Y
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
	_final_xoff = T.size();
}

void FltWin::addScrolloutZone()
//-------------------------------------------------------------------------------
{
	_final_xoff = T.size();
	size_t minTerrainWidth = T.size() + w() + w() / 2;
	T.push_back( T.back() );
	T.back().object( 0 ); // ensure there are no objects in scrollout zone!
	while ( T.size() < minTerrainWidth )
		T.push_back( T.back() );
}

bool FltWin::collisionWithTerrain( const Object& o_ ) const
//-------------------------------------------------------------------------------
{
	int xoff = 0;
	int yoff = 0;
	int X = o_.x();
	int Y = o_.y();
	int W = o_.w();
	int H = o_.h();

	// determine object's rectangle to read from screen
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

	W = X - xoff + W < w() ? W : w() - ( X - xoff );
	H = Y - yoff + H < h() ? H : h() - ( Y - yoff );

	// read image from screen
	const uchar *screen = fl_read_image( 0, X, Y, W, H );

	static const int d = 3;
	bool collided = false;

	// get current background color r/g/b
	unsigned char  R, G, B;
	Fl::get_color( T.bg_color, R, G, B );

	// all transparent pixels of object must have
	// background color, otherwise object touches landscape
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
				break;
			}
		}
		if ( collided ) break;
	}
	delete[] screen;

	return collided;
} // collisionWithTerrain

int FltWin::iniValue( const string& id_,
                      int min_, int max_, int default_ )
//-------------------------------------------------------------------------------
{
	_ini.found( false );
	if ( _internal_levels )
		return default_;
	return _ini.value( id_, min_, max_, default_ );
}

double FltWin::iniValue( const string& id_,
                         double min_, double max_, double default_ )
//-------------------------------------------------------------------------------
{
	_ini.found( false );
	if ( _internal_levels )
		return default_;
	return _ini.value( id_, min_, max_, default_ );
}

const char* FltWin::iniValue( const string& id_,
                              size_t max_, const char* default_ )
//-------------------------------------------------------------------------------
{
	_ini.found( false );
	if ( _internal_levels )
		return default_;
	return _ini.value( id_, max_, default_ );
}

void FltWin::init_parameter()
//-------------------------------------------------------------------------------
{
	// all parameters we understand
	static const char level_name[]              = "level_name";
	static const char rocket_start_prob[]       = "rocket_start_prob";
	static const char rocket_radar_start_prob[] = "rocket_radar_start_prob";
	static const char rocket_start_speed[]      = "rocket_start_speed";
	static const char rocket_min_start_speed[]  = "rocket_min_start_speed";
	static const char rocket_max_start_speed[]  = "rocket_max_start_speed";
	static const char rocket_min_start_dist[]   = "rocket_min_start_dist";
	static const char rocket_max_start_dist[]   = "rocket_max_start_dist";
	static const char rocket_var_start_dist[]   = "rocket_var_start_dist";
	static const char rocket_dx_range[]         = "rocket_dx_range";
	static const char drop_start_prob[]         = "drop_start_prob";
	static const char drop_start_speed[]        = "drop_start_speed";
	static const char drop_min_start_speed[]    = "drop_min_start_speed";
	static const char drop_max_start_speed[]    = "drop_max_start_speed";
	static const char drop_min_start_dist[]     = "drop_min_start_dist";
	static const char drop_max_start_dist[]     = "drop_max_start_dist";
	static const char drop_var_start_dist[]     = "drop_var_start_dist";
	static const char drop_dx_range[]           = "drop_dx_range";
	static const char bady_start_speed[]        = "bady_start_speed";
	static const char bady_min_start_speed[]    = "bady_min_start_speed";
	static const char bady_max_start_speed[]    = "bady_max_start_speed";
	static const char bady_hits[]               = "bady_hits";
	static const char bady_min_hits[]           = "bady_min_hits";
	static const char bady_max_hits[]           = "bady_max_hits";
	static const char bady_dx_range[]           = "bady_dx_range";
	static const char cumulus_start_speed[]     = "cumulus_start_speed";
	static const char cumulus_min_start_speed[] = "cumulus_min_start_speed";
	static const char cumulus_max_start_speed[] = "cumulus_max_start_speed";
	static const char phaser_dx_range[]         = "phaser_dx_range";

	ostringstream defLevelName;
	defLevelName << ( _internal_levels ? "Internal level" : "Level" )  << " " << _level;
	_level_name = iniValue( level_name, 35, defLevelName.str().c_str() );

	_rocket_start_prob = iniValue( rocket_start_prob, 0, 100,
	          (int)( 45. + 35. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ) ); // 45% - 80%
	_rocket_radar_start_prob = iniValue( rocket_radar_start_prob, 0, 100,
	          (int)( 60. + 40. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ) ); // 60% - 100%
	_rocket_min_start_speed = iniValue( rocket_start_speed, 1, 10,
	               4 + _level / 2 );
	if ( _ini.found() )
	{
		_rocket_max_start_speed = _rocket_min_start_speed;
	}
	else
	{
		_rocket_min_start_speed = iniValue( rocket_min_start_speed, 1, 10,
		          4 + _level / 2 - 1 );
		_rocket_max_start_speed = iniValue( rocket_max_start_speed, 1, 10,
		          _rocket_min_start_speed + 2 );
	}
	_rocket_min_start_dist = SCALE_X * iniValue( rocket_min_start_dist, 0, 50, 50 );
	_rocket_max_start_dist = SCALE_X * iniValue( rocket_max_start_dist, 50, 400, 400 );
	_rocket_var_start_dist = SCALE_X * iniValue( rocket_var_start_dist, 0, 200, 200 );
	_rocket_dx_range = iniValue( rocket_dx_range, 0, 2, 0 );

	_drop_start_prob = iniValue( drop_start_prob, 0, 100,
	          (int)( 45. + 30. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ) ); // 45% - 75%
	_drop_min_start_speed = iniValue( drop_start_speed, 1, 10,
	               4 + _level / 2 );
	if ( _ini.found() )
	{
		_drop_max_start_speed = _drop_min_start_speed;
	}
	else
	{
		_drop_min_start_speed = iniValue( drop_min_start_speed, 1, 10,
		          4 + _level / 2 - 1 );
		_drop_max_start_speed = iniValue( drop_max_start_speed, 1, 10,
		          _drop_min_start_speed + 1 );
	}
	_drop_min_start_dist = SCALE_X * iniValue( drop_min_start_dist, 0, 50, 50 );
	_drop_max_start_dist = SCALE_X * iniValue( drop_max_start_dist, 50, 400, 400 );
	_drop_var_start_dist = SCALE_X * iniValue( drop_var_start_dist, 0, 200, 200 );
	_drop_dx_range = iniValue( drop_dx_range, 0, 2, 0 );

	_bady_min_start_speed = iniValue( bady_start_speed, 1, 10, 2 );
	if ( _ini.found() )
	{
		_bady_max_start_speed = _bady_min_start_speed;
	}
	else
	{
		_bady_min_start_speed = iniValue( bady_min_start_speed, 1, 10, 2 - 1 );
		_bady_max_start_speed = iniValue( bady_max_start_speed, 1, 10, 2 );
	}

	_bady_min_hits = iniValue( bady_hits, 1, 5, 5 );
	if ( _ini.found() )
	{
		_bady_max_hits = _bady_min_hits;
	}
	else
	{
		_bady_min_hits = iniValue( bady_min_hits, 1, 5, 5 );
		_bady_max_hits = iniValue( bady_max_hits, 1, 5, 5 );
	}
	_bady_dx_range = iniValue( bady_dx_range, 0, 2, 0 );

	 _cumulus_min_start_speed = iniValue( cumulus_start_speed, 1, 10, 2 );
	if ( _ini.found() )
	{
		_cumulus_max_start_speed = _cumulus_min_start_speed;
	}
	else
	{
		_cumulus_min_start_speed = iniValue( cumulus_min_start_speed,1, 10, 2 - 1 );
		_cumulus_max_start_speed = iniValue( cumulus_max_start_speed, 1, 10, 2 + 1 );
	}

	_phaser_dx_range = iniValue( phaser_dx_range,0, 10, 0 );
}

string FltWin::demoFileName( unsigned level_/* = 0*/ ) const
//-------------------------------------------------------------------------------
{
	ostringstream os;
	int level = level_ ? level_ : _level;
	assert( level );
	os << ( _internal_levels ? "di" : "d" ) /* << ( _user.completed ? "c" : "" ) */
	   << ( ( SCREEN_W != SCREEN_NORMAL_W || SCREEN_H != SCREEN_NORMAL_H ) ?
	      ( string( "_" ) + asString( w() ) + (string)"x" + asString( h() ) ) : "" )
	   << "_" << level;
	if ( 1 != DX )
		os << "_" << DX;
	os << ".txt";
	return demoPath( os.str() );
}

bool FltWin::validDemoData( unsigned level_/* = 0*/ )
//-------------------------------------------------------------------------------
{
	return loadDemoData( level_, true );
}

unsigned FltWin::pickRandomDemoLevel( unsigned minLevel_/* = 0*/, unsigned maxLevel_/* = 0*/ )
//-------------------------------------------------------------------------------
{
	// pick a random level between [minLevel, maxLevel]
	unsigned minLevel( minLevel_ ? minLevel_ : 1 );
	unsigned maxLevel( maxLevel_ ? maxLevel_ : MAX_LEVEL );
	vector<unsigned> possibleLevels;
	for ( unsigned level = minLevel; level <= maxLevel; level++ )
	{
		if ( validDemoData( level ) )
		{
			// found valid data for level 'level' - store as possible candidate
			possibleLevels.push_back( level );
		}
	}
	// randomly pick one of the candidates..
	if ( possibleLevels.size() )
		return possibleLevels[ Random::pRand() % possibleLevels.size() ];
	return minLevel;	// return a valid level even if there's no demo data
}

bool FltWin::loadDemoData( unsigned level_/* = 0*/, bool dryrun_/* = false*/ )
//-------------------------------------------------------------------------------
{
	if ( !dryrun_ )
	{
		_demoData.clear();
		_demoData.ship( _ship );
	}
	ifstream f( demoFileName( level_ ).c_str() );
	if ( !f.is_open() )
		return false;
	uint32_t seed;
	unsigned int dx;
	f >> seed;
	int ship;
	f >> ship;
	f >> dx;
	if ( DX != dx )
		return false;
	if ( dryrun_ )
		return true;
	LOG( "using demo data " <<  demoFileName().c_str() );
	_demoData.seed( seed );
	unsigned long flags;
	f >> flags;
	unsigned user_completed( flags & 0xffff );
	bool classic( ( flags >> 16 ) & 1 );
	bool correct_speed( ( flags >> 17 ) & 1 );
	_exit_demo_on_collision = correct_speed;
	_demoData.ship( ship );
	_demoData.user_completed( user_completed );
	_demoData.classic( classic );
	int index = 0;
	while ( f.good() )
	{
		int sx, sy;
		bool bomb, missile;
		uint32_t seed, seed2;
		f >> sx;
		f >> sy;
		f >> bomb;
		f >> missile;
		int c = f.peek();	// have value for seed?
		if ( ' ' == c || '\t' == c )
		{
			f >> seed;
			f >> seed2;
		}
		if ( !f.good() ) break;
		while ( f.good() )
		{
			_demoData.set( index, sx, sy, bomb, missile, seed, seed2 );
			index++;
			int c = (f >> std::ws).peek(); // (peek one character skipping whitespace)
			if ( '*' != c ) break;
			f.get();
		}
	}
	return true;
}

bool FltWin::saveDemoData() const
//-------------------------------------------------------------------------------
{
	ofstream f( demoFileName().c_str() );
	if ( !f.is_open() )
		return false;
	LOG( "save to demo data " << demoFileName().c_str() );
	f << _demoData.seed() << endl;
	f << _ship << endl;
	f << DX << endl;
	unsigned user_completed( _user.completed );
	if ( _level == _end_level && user_completed )	// save last level with corrected completed
		user_completed--;
	unsigned flags( user_completed | ( _classic << 16 ) | ( _correct_speed << 17 ) );
	f << flags << endl;
	int index = 0;
	int osx( 0 ), osy( 0 );
	bool obomb( false ), omissile( false );
	uint32_t oseed( 0 );
	uint32_t oseed2( 0 );
	while ( f.good() )
	{
		int sx, sy;
		bool bomb, missile;
		uint32_t seed, seed2;
		if ( !_demoData.get( index, sx, sy, bomb, missile, seed, seed2 ) )
			break;
		if ( ( index && sx == 0 && sy == 0 && bomb == 0 && missile == 0 ) ||
		     ( index && sx == osx && sy == osy && obomb == bomb && omissile == missile &&
		     oseed == seed && oseed2 == seed2 ) )
			f << '*';	// data repeat
		else
		{
			if ( seed == oseed && seed2 == oseed2 )
				f << sx << " " << sy << " " << bomb << " " << missile << endl;
			else
				f << sx << " " << sy << " " << bomb << " " << missile << " "
				  << seed << " " << seed2 << endl;
			osx = sx; osy = sy; obomb = bomb; omissile = missile; oseed = seed; oseed2 = seed2;
		}
		index++;
	}
	return true;
}

static istream& readColor( istream& s_, Fl_Color& c_ )
//-------------------------------------------------------------------------------
{
	string c;
	s_ >> c;
	if ( !s_.fail() )
	{
		unsigned long color = strtoul( c.c_str(), NULL, 0 );
		if ( color > 0xff && color % 256 != 0 && color <= 0xffffff )
			color = color << 8;
		c_ = color;
	}
	return s_;
}

static ifstream& readColors( ifstream& f_, Fl_Color& c_, vector<Fl_Color>& alt_ )
//-------------------------------------------------------------------------------
{
	string line;
	getline( f_, line );
	if ( line.empty() )
		getline( f_, line );
	stringstream is;
	is << line;
	readColor( is, c_ );
	while ( !is.fail() )
	{
		Fl_Color c;
		readColor( is, c );
		if ( !is.fail() )
			alt_.push_back( c );
	}
	return f_;
}

static void loadParameter( ifstream& f_, IniParameter& ini_ )
//-------------------------------------------------------------------------------
{
	string line;
	while ( getline( f_, line ) )
	{
		if ( isspace( line[0] ) )
			continue;
		if ( line[0] == ';' || line[0] == '/' || line[0] == '#' )
			continue;
		size_t pos = line.find( "=" );
		if ( pos == string::npos )
			continue;
		string name = line.substr( 0, pos );
		trim( name );
		if ( name.empty() ) continue;
		string value = line.substr( pos + 1 );
		trim( value );
		while ( value.size() && value[ value.size() - 1 ] == '\\' )
		{
			value.erase( value.size() - 1 );
			getline( f_, line );
			trim( line );
			value.push_back( '\n' );
			value += line;
		}
		DBG( "add ini parameter '" << name << "' = '" << value << "'" );
		ini_[ name ] = value;
	}
}

bool FltWin::loadDefaultIniParameter()
//-------------------------------------------------------------------------------
{
	string iniFileName( levelPath( "ini.txt" ) );
	LOG( "iniFileName: '" << iniFileName.c_str() << "'" );
	ifstream f( iniFileName.c_str() );
	if ( !f.is_open() )
		return false;
	loadParameter( f, _defaultIniParameter );
	return true;
}

bool FltWin::loadTranslations()
//-------------------------------------------------------------------------------
{
	if ( _lang.empty() )
		return false;

	string langFileName( homeDir() + "lang_" + _lang + ".txt" );
	LOG( "langFileName: '" << langFileName.c_str() << "'" );
	ifstream f( langFileName.c_str() );
	if ( !f.is_open() )
		return false;
	loadParameter( f, _texts );
	return true;
}

bool FltWin::loadLevel( unsigned level_, string& levelFileName_ )
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
	// read from level file...
	unsigned long version;
	f >> version;
	f >> T.flags;
	if ( version )
	{
		// new format
		f >> T.ls_outline_width;
		readColor( f, T.outline_color_ground );
		readColor( f, T.outline_color_sky );
	}

	readColors( f, T.bg_color, T.alt_bg_colors );
	readColors( f, T.ground_color, T.alt_ground_colors );
	readColors( f, T.sky_color, T.alt_sky_colors );

	int filler = floor( SCALE_X ) - 1;
	int subfiller = lround( fmod( SCALE_X, 1. ) * 10 );
	int subfill = 0;
	int cnt = 0;
	while ( f.good() )	// access data is ignored!
	{
		int s, g;
		int obj;
		f >> s;
		f >> g;
		f >> obj;
		if ( !f.good() ) break;
		for ( int i = 0; i < filler; i++ )
			T.push_back( TerrainPoint( g, s , 0 ) );
		if ( filler >= 0 )
		{
			subfill += subfiller;
			if ( subfill >= 10 )
			{
				T.push_back( TerrainPoint( g, s , 0 ) );
				subfill -= 10;
			}
		}
		if ( SCALE_X >= 1. || ( obj || (int)(SCALE_X * ( cnt + 1 )) != (int)( SCALE_X * cnt ) ) )
			T.push_back( TerrainPoint( g, s, obj ) );
		cnt++;
		if ( obj & O_COLOR_CHANGE )
		{
			// fetch extra data for color change object
			Fl_Color bg_color( T.bg_color );
			Fl_Color ground_color( T.ground_color );
			Fl_Color sky_color( T.sky_color );
			readColor( f, bg_color );
			readColor( f, ground_color );
			readColor( f, sky_color );
			if ( f.good() )
			{
				T.back().bg_color = bg_color;
				T.back().ground_color = ground_color;
				T.back().sky_color = sky_color;
			}
		}
	}
	f.clear(); 	// reset for reading of ini section
	loadParameter( f, _ini );
	return true;
}

#ifndef NO_PREBUILD_LANDSCAPE
void FltWin::clear_level_image_cache()
//-------------------------------------------------------------------------------
{
	delete _terrain;
	delete _landscape;
	delete _background;
	_terrain = 0;
	_landscape = 0;
	_background = 0;
}

Fl_Image *FltWin::terrain_as_image()
//-------------------------------------------------------------------------------
{
	int W = T.size();
	int H = h();
	Fl_Image_Surface *img_surf = new Fl_Image_Surface( W, H );
	assert( img_surf );
	img_surf->set_current();

	// draw bg
	fl_rectf( 0, 0, W, h(), T.bg_color );

	// draw landscape
	draw_landscape( 0, W );

	Fl_RGB_Image *image = img_surf->image();
	delete img_surf;
	Fl_Display_Device::display_device()->set_current(); // direct graphics requests back to the display
	LOG( "terrain_as_image " << image->w() << "x" << image->h() );
	return image;
}

static void color_to_transparence( Fl_Image *img_, Fl_Color c_, uchar alpha_ = 0 )
//-------------------------------------------------------------------------------
{
	assert( img_ );
	if ( img_->d() < 3 )
		return;

	uchar r, g, b;
	Fl::get_color( c_, r, g, b );

#if FLTK_HAS_IMAGE_SCALING
	assert( img_->w() == img_->data_w() && img_->h() == img_->data_h() );
#endif
	uchar *p = (uchar *)img_->data()[0];
	uchar *e = p + img_->w() * img_->h() * img_->d();
	while ( p < e )
	{
		if ( p[0] == r && p[1] == g && p[2] == b )
		{
			p[3] = alpha_; // set color c_ alpha value (0 = max. transparency)
		}
		p += 4;
	}
}

static void faintout_rgb_image( Fl_Image *img_ )
//-------------------------------------------------------------------------------
{
	assert( img_ );
	if ( img_->d() < 3 )
		return;

	uchar *p = (uchar *)img_->data()[0];
#if FLTK_HAS_IMAGE_SCALING
	uchar *e = p + img_->data_w() * img_->data_h() * img_->d();
#else
	uchar *e = p + img_->w() * img_->h() * img_->d();
#endif
	while ( p < e )
	{
		Fl_Color c( fl_rgb_color( p[0], p[1], p[2] ) );
		Fl_Color a = fl_color_average( FL_GRAY, c, 0.3 );
		Fl::get_color( a, p[0], p[1], p[2] );
		p += 4;
	}
}

static Fl_RGB_Image *read_RGBA_image( int W_, int H_ )
//-------------------------------------------------------------------------------
{
	uchar *screen = fl_read_image( NULL, 0, 0, W_, H_, 255 );
	Fl_RGB_Image *image = new Fl_RGB_Image( screen, W_, H_, 4 );
	image->alloc_array = 1;
	return image;
}

static const Fl_Color BlueBoxColor = fl_rgb_color( 254, 254, 254 );

Fl_Image *FltWin::background_as_image()
//-------------------------------------------------------------------------------
{
	int W = T.size();
	int H = h();
	Fl_Image_Surface img_surf( W, H );
	img_surf.set_current();

	// set screen to white to be used for transparency
	fl_rectf( 0, 0, W, h(), BlueBoxColor );

	if ( _effects > 1 && !classic() )
	{
		draw_shaded_background( 0, W );
	}
	else
	{
		fl_color( T.bg_color );
		for ( int i = 0; i < W; i++ )
		{
			fl_yxline( i, T[i].sky_level(), h() - T[i].ground_level() );
		}
	}
	Fl_Image *image = read_RGBA_image( W, H );
	color_to_transparence( image, BlueBoxColor );
	Fl_Display_Device::display_device()->set_current(); // direct graphics requests back to the display
	LOG( "background_as_image " << image->w() << "x" << image->h() );
	return image;
}

Fl_Image *FltWin::landscape_as_image()
//-------------------------------------------------------------------------------
{
	int W = T.size();
	int H = h();
	Fl_Image_Surface img_surf( W, H );
	img_surf.set_current();

	if ( _effects && !classic() )
	{
		Fl_Color bg = T.bg_color;
		T.bg_color = BlueBoxColor;
		draw_shaded_landscape( 0, W );
		T.bg_color = bg;
	}
	else
	{
		// draw bg in white to be used for transparency
		fl_rectf( 0, 0, W, h(), BlueBoxColor );

		// draw landscape
		draw_landscape( 0, W );
	}
	Fl_Image *image = read_RGBA_image( W, H );
	color_to_transparence( image, BlueBoxColor );
	Fl_Display_Device::display_device()->set_current(); // direct graphics requests back to the display
	LOG( "landscape_as_image " << image->w() << "x" << image->h() );
	return image;
}

#endif //NO_PREBUILD_LANDSCAPE

bool FltWin::create_terrain()
//-------------------------------------------------------------------------------
{
	assert( _level > 0 && _level <= MAX_LEVEL );
	static uint32_t seed = 0;
	static unsigned lastCachedTerrainLevel = 0;
	static unsigned lastCachedUserCompleted = 0;
	static bool lastCachedClassic = false;
	static Terrain TC;

	if ( _level != lastCachedTerrainLevel
	     || classic() != lastCachedClassic // recreate also when completed/classic flag differ
	     || user_completed() != lastCachedUserCompleted )
	{
		// level must be recreated
		DBG( "level " << _level << " must be re-created "
		     << lastCachedTerrainLevel << " " << classic() << ":" << lastCachedClassic
           << " " << user_completed() << ":" << lastCachedUserCompleted );
		_ini = _defaultIniParameter;
		T.clear();
		lastCachedTerrainLevel = 0;
		seed = 0;
	}
	else
	{
		T = TC;	// load from "cache"
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
	DBG(  "seed #" << _level << ": " << seed );
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
		// always create internal level, to keep random generator in sync
		T.clear();
		create_level();
		loaded = (int)T.size() > w();
	}
	else if ( T.empty() )
	{
		loaded = loadLevel( _level, levelFile );	// try to load level from landscape file
		if ( loaded && user_completed() / 2 )
		{
			// TODO: just a test to use different colors dep. on completed state
			if ( T.alt_bg_colors.size() )
			{
				size_t idx = user_completed() / 2; // revers level has still same color
				if ( idx > T.alt_bg_colors.size() )
					idx = T.alt_bg_colors.size();
				T.bg_color = T.alt_bg_colors[ idx - 1 ];
			}
		}
	}
	if ( (int)T.size() < w() )
	{
		string err( T.empty() && errno ? strerror( errno ) : "File is not a valid level file" );
		PERR( "Failed to load level file " << levelFile << ": " << err.c_str() );
		return false;
	}
	else if ( !_internal_levels && loaded )
	{
		revert_level();
		if ( reversLevel() && !( T.flags & Terrain::NO_SCROLLOUT_ZONE ) )
			addScrollinZone();
		else if ( !reversLevel() && !( T.flags & Terrain::NO_SCROLLIN_ZONE ) )
			addScrollinZone();
		addScrolloutZone();	// always add scrollout zone

		// save "original" colors (for color change restore) into first index
		T[0].sky_color = T.sky_color;
		T[0].ground_color = T.ground_color;
		T[0].bg_color = T.bg_color;
	}
#ifndef NO_PREBUILD_LANDSCAPE
	if ( _level != lastCachedTerrainLevel || !_terrain )
	{
		clear_level_image_cache();
		// terrains with color change cannot be prebuild as image!
		_terrain = T.hasColorChange() ? 0 : terrain_as_image();
		if ( _terrain && _gimmicks && _effects && !_classic )
		{
			_landscape = landscape_as_image();
			_background = background_as_image();
		}
	}
#endif
	if ( lastCachedTerrainLevel != _level )
	{
		// store last cached level + flags completed/classic (hacky!)
		lastCachedTerrainLevel = _level;
		lastCachedUserCompleted = user_completed();
		lastCachedClassic = classic();
		TC = T;
	}
	TBG.clear();
	T.check();
	if ( _effects )
	{
		// currently levels without sky draw mountains in background
		if ( !T.hasSky() )
		{
			int back = _final_xoff + w() / 2;
			for ( int i = 0; i < back; i++ )
				TBG.push_back( T[back - i - 1] );
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
			size_t n_stars = 1. / SCALE_X * TBG.size() / 10;
			for ( size_t i = 0; i < n_stars; i++ )
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

bool FltWin::revert_level()
//-------------------------------------------------------------------------------
{
	if ( reversLevel() )
	{
		size_t n = T.size();
		for ( size_t i = 0; i < n; i++ )
			T.push_back( T[n - i - 1] );
		T.erase( T.begin(), T.begin() + n );

		// have to revert again the color changes...
		int last_x = T.size() - 1;
		for ( size_t i = 0; i < T.size(); i++ )
		{
			if ( T[i].has_object( O_COLOR_CHANGE ) )
			{
				for ( size_t j = last_x; j > 0; j-- )
				{
					if ( j <= i )
						return true;
					if ( T[j].has_object( O_COLOR_CHANGE ) )
					{
						Fl_Color s = T[j].sky_color;
						Fl_Color g = T[j].ground_color;
						Fl_Color b = T[j].bg_color;

						T[j].sky_color = T[i].sky_color;
						T[j].ground_color = T[i].ground_color;
						T[j].bg_color = T[i].bg_color;

						T[i].sky_color = s;
						T[i].ground_color = g;
						T[i].bg_color = b;

						last_x = j - 1;
						break;
					}
				}
			}
		}
		return true;
	}
	return false;
}

static void connectPoints( vector<Point> &terrain_, Terrain& T_, bool edgy_ )
//-------------------------------------------------------------------------------
{
	static const int minDX = 20;	// 20
	static const int minDY = 4;	// 4
	static const int pertPerc = 8;	//	8
	bool done( false );
	while ( !done )
	{
		done = true;
		for ( size_t i = 0; i < terrain_.size() - 1; i++ )
		{
			int dx = terrain_[i + 1].x - terrain_[i].x;
			if ( dx < minDX ) continue;
			int dy = terrain_[i + 1].y - terrain_[i].y;
			if ( abs( dy ) < minDY )
			{
				if ( edgy_ )
					continue;
				dy = minDY;
			}

			done = false;
			int mx = ( terrain_[i].x + terrain_[i + 1].x ) / 2;
			int my = terrain_[i].y + ((double)dy / dx ) * ( mx - terrain_[i].x );

			int pert = ceil( ( dx > abs( dy ) ? (double)dx : (double)( abs( dy )) ) / pertPerc );
			int py = rangedRandom( -pert / 2 , pert );
			my += py;
			terrain_.insert( terrain_.begin() + i + 1, Point( mx, my ) );
			break;
		}
	}

	int x = 0;
	int y = 0;
	for ( size_t i = 0; i < terrain_.size(); i++ )
	{
		int xn = terrain_[i].x;
		int yn = terrain_[i].y;
		int dx = xn - x;
		int dy = yn - y;
		int xo = 0;
		while ( x++ < xn )
		{
			int yy = dx ? y + ((double)dy / dx) * xo : y;
			assert( x > 0 );
			if ( x - 1 < (int)T_.size() )
				T_[x - 1].ground_level( yy );
			xo++;
		}
		assert( x > 0 );
		if ( x - 1 < (int)T_.size() )
			T_[x - 1].ground_level( yn );
		x = xn;
		y = yn;
	}
}

void FltWin::create_level()
//-------------------------------------------------------------------------------
{
	struct terrain_data
	{
		bool sky;
		int edgy;
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
	static const struct terrain_data level_data[] = {
		{ /*1*/false, 0, FL_GREEN, FL_BLUE, FL_GREEN, 0, FL_BLACK, FL_BLACK,
			30, 20, 0, 0, 0, 0 },
		{ /*2*/true, 0, fl_rgb_color( 241, 132, 0 ), fl_rgb_color( 97, 64, 41 ), fl_rgb_color( 139, 13, 6 ),
		       3, FL_BLACK, FL_BLACK,
			20, 20, 25, 0, 0, 0 },
		{ /*3*/false, 0, FL_RED, FL_GRAY, FL_RED, 2, FL_WHITE, FL_WHITE,
			30, 15, 0, 2, 0, 0 },
		{ /*4*/true, 0, FL_DARK_GREEN, FL_CYAN, FL_DARK_YELLOW, 3, FL_BLACK, FL_YELLOW,
			30, 10, 20, 1, 0, 0 },
		{ /*5*/false, 2, FL_BLACK, FL_MAGENTA, FL_BLACK, 4, FL_RED, FL_RED,
			35, 20, 0, 3, 0, 0 },
		{ /*6*/false, 0, FL_DARK_RED, FL_DARK_GREEN, FL_DARK_RED, 0, FL_YELLOW, FL_YELLOW,
			35, 20, 0, 2, 1, 0 },
		{ /*7*/true, 1, FL_BLACK, FL_BLACK, FL_BLACK, 4, FL_GRAY, FL_CYAN,
			20, 15, 20, 1, 0, 0 },
		{ /*8*/false, 0, FL_WHITE, FL_DARK_BLUE, FL_WHITE, 4, FL_RED, FL_CYAN,
			30, 20, 0, 4, 2, 0 },
		{ /*9*/true, 0, FL_DARK_BLUE, FL_BLUE, FL_DARK_BLUE, 2, FL_CYAN, FL_CYAN,
			30, 15, 20, 0, 1, 2 },
		{ /*10*/true, 0, FL_BLACK, fl_lighter( FL_YELLOW ), FL_DARK_RED, 2, FL_BLACK, FL_WHITE,
			30, 10, 20, 2, 2, 4 }
	};

	assert( nbrOfItems( level_data ) >= MAX_LEVEL );
	int set = ( _level - 1 ) % nbrOfItems( level_data );
	const struct terrain_data& level = level_data[ set ];

	T.ground_color = level.ground_color;
	T.bg_color = level.bg_color;
	T.sky_color = level.sky_color;
	T.ls_outline_width = level.outline_width;
	T.outline_color_ground = level.outline_color_ground;
	T.outline_color_sky = level.outline_color_sky;

	int rockets = level_data[ set ].rockets;
	int radars = level_data[ set ].radars;
	int drops = level_data[ set ].drops;
	int badies = level_data[ set ].badies;
	int cumulus = level_data[ set ].cumulus;
	int phasers = level_data[ set ].phasers;
	vector<Point> terrain;

	//
	// Generate ground
	//
	double hardestFactor = 0.7 + ((double)_level / MAX_LEVEL ) * 0.3;	// factor to make higher levels harder

	int maxGround = ( h() * ( level.sky ? 3 : 4 ) ) / 5; // use 3/5 (with sky) or 4/5 ( w/o sky) of height
	int minGround = h() / 20;	// minimum ground level

	int bott = ( h() / 3 ) * hardestFactor;
	int range = ceil( (double)maxGround * hardestFactor );

	DBG( "range: " << range << " bott: " << bott << " hardestfactor: " << hardestFactor );

	static const size_t INTERNAL_TERRAIN_SIZE( 9 * SCREEN_W );
	size_t X = 0;
	int bott1 = minGround + bott / 2  + rangedRandom( -bott / 4, bott / 4 );
	while ( X < INTERNAL_TERRAIN_SIZE )
	{
		/*
                          flat2
                         2)  3)
              peak       +---+
                        /     \
                       / |   | \
             flat1    /  |   |  +------- bott2
  bott1   +----------+   |   |  4)
          |          1)  |   |  |
          0)             |  peak_dist2
                     |   |
                    peak_dist1

		*/
		int flat1 = Random::Rand() % ( w() / 2 ) + w() / 8;
		int r = ( X == 0 ) ? range / 2 : range;	// don't start with a high mountain
		int peak = minGround + (( Random::Rand() % ( r / 2 )  + r / 2 ) * hardestFactor);
		int peak_dist1 = ( Random::Rand() % ( peak * 2 ) ) + peak / 4;
		int peak_dist2 = ( Random::Rand() % ( peak * 2 ) ) + peak / 4;
		int flat2 = Random::Rand() % ( w() / 2 ) + ( !!level.edgy * w() / 5 );
		if ( !level.edgy )
		{
			if ( Random::Rand() % 3 == 0 )
				flat2 = Random::Rand() % 10;
		}
		int bott2 = minGround + bott + rangedRandom( -bott / 2, bott / 2 );
		if ( bott2 > peak - 20 )
			bott2 = peak - 20;

		terrain.push_back( Point( X, bott1 ) );	// 0)
		X += flat1;
		terrain.push_back( Point( X, bott1 ) );	// 1)

		if ( X >= INTERNAL_TERRAIN_SIZE ) break;

		X += level.edgy == 1 ? 1 : peak_dist1;
		terrain.push_back( Point( X, peak ) );	// 2)

		if ( flat2 )
		{
			X += flat2;
			terrain.push_back( Point( X, peak ) );	// 3)
			if ( X >= INTERNAL_TERRAIN_SIZE ) break;
		}

		X += level.edgy == 1 ? 1 : peak_dist2;
		terrain.push_back( Point( X, bott2 ) );	// 4)

		bott1 = bott2;
	}
	T.resize( terrain.back().x, TerrainPoint( 0 ) );
	connectPoints( terrain, T, !!level.edgy );

	//
	// Generate sky
	//
	if ( level.sky )
	{
		int top = h() / 12;
		for ( size_t i = 0; i < T.size(); i++ )
		{
			int ground = T[i].ground_level();
			int free = h() - ground;
			int sky = (int)( (double)top * (double)free / (double)h() );
			sky += _level * 5;
			if ( h() - ground - sky < _spaceship->h() + 20 )
				sky = h() - ground - _spaceship->h() - 20;
			if ( sky  < -1 || sky >= h() )
			{
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

	// create object at x within 'max_dist', at least at 'min_dist'
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
				objects.insert( objects.begin() + Random::Rand() % objects.size(), objects[0] );
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
			// pick an object if needed
			if ( !o )
			{
				int index = Random::Rand() % objects.size();
				o = objects[ index ];
				objects.erase( index, 1 );
			}
			switch ( o )
			{
				case 'r':
					if ( ( level.edgy && flat ) || !level.edgy )
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
			//	too low for objects
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
	revert_level();
	addScrolloutZone();
}

void FltWin::draw_badies() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Badies.size(); i++ )
	{
		Badies[i]->draw();
	}
}

void FltWin::draw_bombs() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Bombs.size(); i++ )
	{
		Bombs[i]->draw();
	}
}

void FltWin::draw_cumuluses() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Cumuluses.size(); i++ )
	{
		Cumuluses[i]->draw();
	}
}

void FltWin::draw_drops() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Drops.size(); i++ )
	{
		Drops[i]->draw();
	}
}

void FltWin::draw_explosions() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Explosions.size(); i++ )
	{
		Explosions[i]->draw();
	}
}

void FltWin::draw_missiles() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Missiles.size(); i++ )
	{
		Missiles[i]->draw();
	}
}

void FltWin::draw_phasers() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Phasers.size(); i++ )
	{
		Phasers[i]->draw();
	}
}

void FltWin::draw_radars() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Radars.size(); i++ )
	{
		Radars[i]->draw();
	}
}

void FltWin::draw_rockets() const
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Rockets.size(); i++ )
	{
		Rockets[i]->draw();
	}
}

void FltWin::draw_objects( bool pre_ ) const
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
		draw_explosions();
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
	flt_font( FL_HELVETICA_BOLD_ITALIC, sz_ );
	Fl_Color cc = fl_contrast( FL_WHITE, c_ );
	fl_color( cc );

	if ( x_ != -1 )
		x_ *= SCALE_X;
	y_ *= SCALE_Y;
	if ( y_ < 0 )
		y_ = h() + y_;
	if ( x_ < -1 )
		x_ = w() + x_;

	int x = x_;
	if ( x_ < 0 )
	{
		// centered
		x = ( w() - (int)fl_width( buf ) ) / 2;
	}

	fl_draw( buf, strlen( buf ), x + ceil( SCALE_Y * 2 ), y_ + ceil( SCALE_Y * 2 ) );
	fl_color( c_ );
	fl_draw( buf, strlen( buf ), x, y_ );
	return lround( x / SCALE_X );
}

int FltWin::drawTextBlock( int x_, int y_, const char *text_, size_t sz_,
                           int line_height_, Fl_Color c_, ... ) const
//-------------------------------------------------------------------------------
{
	char buf[1024];
	va_list argp;
	va_start( argp, c_ );
	vsnprintf( buf, sizeof( buf ), text_, argp );
	va_end( argp );

	int x = x_;
	int y = y_;
	char *text = buf;
	while ( text )
	{
		char *nl = strchr( text, '\r' );
		if ( !nl )
			nl = strchr( text, '\n' );
		if ( nl )
			*nl = 0;
		int X = drawText( x, y, text, sz_, c_ );
		if ( x_ >= 0 )	// keep text centered
			x = X;
		y += line_height_;
		if ( nl )
		{
			text = nl + 1;
			if ( *nl == '\r' && *text == '\n' )
				text++;
		}
		else
			text = 0;
	}
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
	flt_font( FL_HELVETICA_BOLD_ITALIC, sz_ );
	Fl_Color cc = fl_contrast( FL_WHITE, c_ );
	fl_color( cc );

	w_ *= SCALE_X;
	y_ *= SCALE_Y;

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
	if ( w_ <= 0 )	// just measure text
		return lround( double( wl + wr ) / SCALE_X );

	int x =  ( w() - w_ ) / 2;

	fl_draw( l, strlen( l ), x + SCALE_Y * 2, y_ + SCALE_Y * 2 );
	if ( r )
		fl_draw( r, strlen( r ), x + w_ - wr + SCALE_Y * 2, y_ + SCALE_Y * 2 );
	static int DOT_SIZE = lround( SCALE_X * 5 );
	int lw = ( x + w_ - wr - 2 * DOT_SIZE ) - ( x + wl + 2 * DOT_SIZE );
	lw = ( ( lw + DOT_SIZE / 2 ) / DOT_SIZE + 1 ) * DOT_SIZE;
	int lx = x + wl + 2 * DOT_SIZE;
	for ( int i = 0; i < lw / ( DOT_SIZE * 2 ); i++ )
		fl_rectf( lx + SCALE_Y * 2 + i * DOT_SIZE * 2, y_ - SCALE_Y * 3, DOT_SIZE, DOT_SIZE );
	fl_color( c_ );
	fl_draw( l, strlen( l ), x, y_ );
	if ( r )
		fl_draw( r, strlen( r ), x + w_ - wr, y_ );
	fl_color( fl_color_average( c_, cc, .8 ) );
	for ( int i = 0; i < lw / ( DOT_SIZE * 2 ); i++ )
		fl_rectf( lx + i * DOT_SIZE * 2, y_ - SCALE_Y * 5, DOT_SIZE, DOT_SIZE );

	return x;
}

int FltWin::drawTableBlock( int w_, int y_, const char *text_, size_t sz_,
                            int line_height_, Fl_Color c_, ... ) const
//-------------------------------------------------------------------------------
{
	char buf[1024];
	va_list argp;
	va_start( argp, c_ );
	vsnprintf( buf, sizeof( buf ), text_, argp );
	va_end( argp );

	int y = y_;
	int x = -1;
	int max_w = 0;
	char *text = buf;
	while ( text )
	{
		char *nl = strchr( text, '\r' );
		if ( !nl )
			nl = strchr( text, '\n' );
		if ( nl )
			*nl = 0;
		x = drawTable( w_, y, text, sz_, c_ );
		if ( w_ <=  0 && x > max_w ) // just measuring table width
			max_w = x;
		y += line_height_;
		if ( nl )
		{
			text = nl + 1;
			if ( *nl == '\r' && *text == '\n' )
				text++;
		}
		else
			text = 0;
	}
	return w_ < 0 ? max_w : x;
}

void FltWin::draw_fadeout()
//-------------------------------------------------------------------------------
{
	if ( _dimmout || ( _effects > 1 && _state == PAUSED && !_done ) )
	{
		static int bytes = w() * h() * 4;
		static uchar *screen = 0;
		static Fl_RGB_Image *matte = 0;

		if ( !screen )
		{
			screen = new uchar[ bytes ];
			memset( screen, 0, bytes );
		}
		if ( !matte )
			matte = new Fl_RGB_Image( screen, w(), h(), 4 );

		unsigned alpha = 128; // default grayout value for paused mode
		if ( !_dimmout )
		{
			// Calculate the current fadeout alpha value:
			// determine real fps, in order to calculate total_frames
			// (important when run with speed correction)
			static double fps = FPS;
			if ( _alpha_matte < 10 )
				fps = ( (double)w() / _DDX ) / 4;
			double total_frames = _TO * fps;
			double perc = (double)_alpha_matte / total_frames;
			alpha = perc * 256;
			if ( alpha > 255 )
				alpha = 255;
			_alpha_matte++;
		}
		// change alpha in matte array
		uchar *p = screen;
		for ( int i = 0; i < bytes; i += 4, p += 4 )
			*(p + 3) = alpha;
		matte->uncache();
		matte->draw( 0, 0 );
	}
}

void FltWin::draw_tvmask() const
//-------------------------------------------------------------------------------
{
	// NOTE: the idea for screen mask display and the mask values are taken from:
	//       https://sourceforge.net/projects/view64/
	//       (the KISS implementation is my own).
	static const char *tvmask_data[] = {
		// r/g/b/a values of a 6x4 pixel repeatable color mask
		"\xda\xda\xda\x20\xda\xda\xda\x20\xda\xda\xda\x20\xff\xda\xda\x20\xda\xff\xda\x20\xda\xda\xff\x20",
		"\xff\xda\xda\x20\xda\xff\xda\x20\xda\xda\xff\x20\xff\xda\xda\x20\xda\xff\xda\x20\xda\xda\xff\x20",
		"\xff\xda\xda\x20\xda\xff\xda\x20\xda\xda\xff\x20\xda\xda\xda\x20\xda\xda\xda\x20\xda\xda\xda\x20",
		"\xff\xda\xda\x20\xda\xff\xda\x20\xda\xda\xff\x20\xff\xda\xda\x20\xda\xff\xda\x20\xda\xda\xff\x20"
	};
	static const int d = 4;
	static const int tvmask_w = strlen( tvmask_data[0] ) / d;
	static const int tvmask_h = nbrOfItems( tvmask_data );
	static Fl_RGB_Image *tvmask = 0;
	if ( !tvmask )
	{
		int bytes = w() * h() * d;
		uchar *data = new uchar[ bytes ];
		tvmask = new Fl_RGB_Image( data, w(), h(), d );
		for ( int line = 0; line < h(); line++ )
		{
			for ( int col = 0; col < w(); col += tvmask_w )
			{
				int n = min( tvmask_w, w() - col ) * d;
				memcpy( data, tvmask_data[line % tvmask_h], n );
				data += n;
			}
		}
	}
	tvmask->draw( 0, 0 );
}

void FltWin::draw_tv() const
//-------------------------------------------------------------------------------
{
	if ( _scanlines )
	{
		static const int d = 4;
		static Fl_RGB_Image *scanlines = 0;
		if ( !scanlines )
		{
			int bytes = w() * h() * d;
			uchar *data = new uchar[ bytes ];
			memset( data, 0, bytes );
			scanlines = new Fl_RGB_Image( data, w(), h(), d );
			int step = ceil( SCALE_Y * 2 );
			if ( step < 2 )
				step = 2;
			for ( int y = 0; y < h(); y += step )
				memset( &data[y * w() * 4], 0x20, w() * d ); // rgba=0x20202020
		}
		scanlines->draw( 0, 0 );
	}
	if ( _tvmask )
		draw_tvmask();
}

void FltWin::draw_score()
//-------------------------------------------------------------------------------
{
	static FltImage _lifes[3];
	if ( _effects && !_lifes[0].image() )
	{
		_lifes[0].get( imgPath.get( "lifes1.gif" ).c_str() );
		_lifes[1].get( imgPath.get( "lifes2.gif" ).c_str() );
		_lifes[2].get( imgPath.get( "lifes3.gif" ).c_str() );
	}
	if ( _state == DEMO )
	{
		drawText( -1, -30, _texts.value( "demo", 50,
		          "*** DEMO - Hit space to cancel ***" ),
		          30, FL_WHITE );
		return;
	}
	if ( _trainMode )
	{
		drawText( -110, 20, _texts.value( "trainer", 7, "TRAINER" ), 20, FL_RED );
	}

	flt_font( FL_HELVETICA, 30 );
	fl_color( fl_contrast( FL_WHITE, T.ground_color ) );

	char buf[50];
	int n = snprintf( buf, sizeof( buf ), "Hiscore: %06u", _hiscore );
	flt_draw( buf, n, -280, -30 );

	if ( !_effects )
	{
		n = snprintf( buf, sizeof( buf ), "L %u/%u Score: %06d", _level,
		              MAX_LEVEL_REPEAT - _level_repeat, _user.score );
		flt_draw( buf, n, 20, -30 );
		n = snprintf( buf, sizeof( buf ), "%d %%",
	              (int)( (float)_xoff / (float)( _final_xoff  - _spaceship->x() ) * 100. ) );
		flt_draw( buf, n, SCREEN_NORMAL_W / 2 - 20 , -30 );
	}
	else
	{
		n = snprintf( buf, sizeof( buf ), "%u%s       Score: %06d",
	                 _level, ( _level < 10 ? " " : "" ), _user.score );
		flt_draw( buf, n, 20, -30 );
		int lifes = MAX_LEVEL_REPEAT - _level_repeat;
		if ( lifes )
		{
			_lifes[lifes - 1].draw( SCALE_X * 60, h() - SCALE_Y * 52 );
		}
		int proc = (int)( (float)_xoff / (float)( _final_xoff - _spaceship->x() ) * 100. );
		int X = w() / 2 - SCALE_X * 16;
		int Y = h() - SCALE_Y * 49;
		int W = lround( SCALE_X * 100 );	// 100% = 100px
		int H = SCALE_Y * 18;
		fl_rect( X - 1, Y -1, W + 2, H + 2 );
		fl_color( T.ground_color == T.bg_color || classic() ?
		          fl_contrast( FL_WHITE, T.ground_color ) : T.bg_color );
		fl_rectf( X, Y, lround( proc * SCALE_X ), H );

		if ( reversLevel() && !_completed )	// draw a down-arrow to denote going reverse levels
			fl_draw_symbol( "@+22->", 0, h() - SCALE_Y * 45, SCALE_X * 20, SCALE_Y * 30, FL_RED );
	}

	if ( G_paused )
	{
		drawText( -1, SCREEN_NORMAL_H / 2, _texts.value( "paused", 30, "** PAUSED **" ), 50, FL_WHITE );
	}
	else if ( _done )
	{
		if ( _level == _end_level && ( !_trainMode || _cheatMode ) )
		{
			static bool revers_level = false;
			string s;
			if ( !_completed )
			{
				_completed = true;
				revers_level = reversLevel() || _internal_levels || _trainMode;
				Audio::instance()->enable();	// turn sound on
#ifndef NO_PREBUILD_LANDSCAPE
				clear_level_image_cache();	// switch to direct drawing for grayout!
#endif
				if ( revers_level )
					s = _texts.value( "mission_complete", 50, "** MISSION COMPLETE! **" );
				else
					s = _texts.value( "mission_part", 50, "** WELL DONE! **" );
				(_anim_text = new AnimText( 0, SCALE_Y * 10, w(), s.c_str(),
					FL_WHITE, FL_RED, 50, 40, false ))->start();

				// add a "completed" bonus
				int completed_bonus = _bonus * ( _user.completed % 2 + 1 );
				_bonus += completed_bonus;;
				add_score( completed_bonus );
			}
			if ( _anim_text )
				_anim_text->draw();
			if ( revers_level )
				s = _texts.value( "praise_completed", 200,
			                     "You bravely mastered all challenges\n"
			                     "and deserve to be called a REAL HERO.\n"
			                     "Well done! Take some rest now, or ..." );
			else
				s = _texts.value( "praise", 200,
		                        "You succeeded to conquer all hazards\n"
			                     "and have reached your destination!\n"
				                  "\n"
			 	                  "What a pity, that you have to get back\n"
			                     "now from where you started...." );
			drawTextBlock( -1, 150, s.c_str(), 30, 40, FL_WHITE );
			if ( revers_level )
				s = _texts.value( "space_to_restart", 50, "** hit space to restart **" );
			else
				s = _texts.value( "space_to_go_back", 50, "** hit space to go back **" );
			drawText( -1, 400, s.c_str(), 40, FL_YELLOW );
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
			drawText( -1, 50, _texts.value( "level_finished", 50,  "LEVEL %u FINISHED!" ),
			          50, FL_WHITE, _level );
		}
		if ( _bonus > 0 )
			drawText( -1, -80, _texts.value( "bonus", 20, "Bonus: %u" ),
			          30, FL_YELLOW, _bonus );
	}
	else if ( _collision )
	{
		if ( _level_repeat + 1 > MAX_LEVEL_REPEAT )
		{
			drawTextBlock( -1, 50, _texts.value( "start_again", 50, "START AGAIN!" ),
			               50, 60, FL_RED );
		}
	}
}

void FltWin::draw_scores()
//-------------------------------------------------------------------------------
{
	fl_rectf( 0, 0, w(), h(), 0x11111100 );
	drawText( -1, 120, _texts.value( "congratulation", 20, "CONGRATULATION!" ),
	          70, FL_RED );
	drawText( -1, 200, _texts.value( "topped", 30, "You topped the hiscore:" ),
	          40, FL_RED );
	static const int TW = 440;
	drawTable( TW, 290, "%s\t%06u", 30, FL_WHITE, _texts.value( "old_hiscore", 20, "old hiscore" ), _hiscore );
	int Y = 330;
	if ( _hiscore )
	{
		string bestname( _hiscore_user );
		drawTable( TW, Y, "%s\t%s", 30, FL_WHITE, _texts.value( "held_by", 20, "held by" ),
		           bestname.empty() ?  "???" : bestname.c_str() );
		Y += 40;
	}
	drawTable( TW, Y, "%s\t%06u", 30, FL_WHITE, _texts.value( "new_hiscore", 20, "new hiscore" ), _user.score );

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
		drawText( -1, 420, _texts.value( "enter_name", 30, "Enter your name:   %s" ),
		          32, FL_GREEN, inpfield.c_str() );
	else
		drawText( -1, 420, _texts.value( "score_goes_to", 30, "Score goes to:   %s" ),
		          32, FL_GREEN, _user.name.c_str() );
	drawText( -1, -50, _texts.value( "space_to_continue", 30, "** hit space to continue **" ),
	          40, FL_YELLOW );

	// scanline effect
	draw_tv();
}

void FltWin::draw_title()
//-------------------------------------------------------------------------------
{
	static Fl_Image *bgImage = 0;
	static Fl_Image *bgImage2 = 0;
	static int _flip = 0;
	static int reveal_height = 0;
	static AnimText *title_anim;

	if ( _frame <= 1 )
	{
		_flip = 0;
		delete bgImage;
		bgImage = 0;
		delete bgImage2;
		bgImage2 = 0;
		reveal_height = 0;
	}

	int border_w = lround( 40 * SCALE_X );
	int border_h = lround( 20 * SCALE_Y );
	int dborder_h = lround( 40 * SCALE_Y );
	if ( !G_paused )
	{
		// speccy loading stripes
		static const Fl_Color c[] = { FL_GREEN, FL_BLUE, FL_YELLOW };
		int ci = Random::pRand() % 3;
		int stripe_h = SCALE_Y * 4;
		for ( int y = 0; y < h(); y += stripe_h )
		{
			fl_color( c[ci] );
			for ( int n = 0; n < stripe_h; n++ )
			{
				if ( y + n <= border_h || y + n >= h() - border_h )
					fl_line( 0, y + n , w(), y + n );
				else
				{
					fl_line( 0, y + n, border_w, y + n );
					fl_line( w() - border_w, y + n, w(), y + n );
				}
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
	{
		// draw menu background rectangle
		static Fl_Color title_color = _ini.value( "title_color", 0, 0xffffff, 0x202020 ) << 8;
		if ( _effects > 1 )
		{
			// gradient top/bottom 'title_color_beg' => 'title_color'
			static Fl_Color title_color_beg = _ini.value( "title_color_beg", 0, 0xffffff, 0x808080 ) << 8;
			grad_rect( border_w, border_h, w() - 2 * border_w, h() - dborder_h + 1, h(),
			           false, title_color_beg, title_color );
		}
		else
		{
			// single color 'title_color'
			fl_rectf( border_w, border_h, w() - 2 * border_w, h() - dborder_h + 1,
			          title_color << 8 );
		}
	}
	if ( _spaceship && _spaceship->origImage() && !bgImage )
	{
#if FLTK_HAS_NEW_FUNCTIONS
		Fl_Image::RGB_scaling( FL_RGB_SCALING_BILINEAR );
#endif
//		bgImage = _spaceship->origImage()->copy( w() - SCALE_X * 120, h() - SCALE_Y * 150 );
		bgImage = fl_copy_image( _spaceship->origImage(),  w() - SCALE_X * 120, h() - SCALE_Y * 150 );
//		bgImage2 = _rocket.origImage()->copy( w() / 3, h() - SCALE_Y * 150 );
		bgImage2 = fl_copy_image( _rocket.origImage(), w() / 3, h() - SCALE_Y * 150 );
	}
	if ( !title_anim )
		(title_anim = new AnimText( 0, SCALE_Y * 45, w(), "FL'TRATOR",
		                             FL_RED, FL_WHITE, 90, 80, false ))->start();
	if ( !G_paused && _cfg->non_zero_scores() && _flip++ % (FPS * 8) > ( FPS * 5) )
	{
		if ( bgImage2 )
			bgImage2->draw( SCALE_X * 60, SCALE_Y * 120 );
		size_t n = _cfg->non_zero_scores();
		if ( n > 6 )
			n = 6;
		int y = 335 - n / 2 * 42;
		drawText( -1, y, _texts.value( "hiscores", 20, "Hiscores" ), 35, FL_GREEN );
		y += 50;
		for ( size_t i = 0; i < n; i++ )
		{
			string name( _cfg->users()[i].name );
			drawTable( 360, y, "%05u\t%s", 30,
			              _cfg->users()[i].completed ? FL_YELLOW : FL_WHITE,
			              _cfg->users()[i].score, name.empty() ? DEFAULT_USER : name.c_str() );
			y += 42;
		}
	}
	else
	{
		fl_push_clip( border_w, border_h, w() - 2 * border_w, h() - dborder_h );
		if ( _zoominShip && !_zoominShip->done() )
			_zoominShip->draw();
		else if ( bgImage )
			bgImage->draw( SCALE_X * 60, SCALE_Y * 40 );
		fl_pop_clip();
		int x = drawText( -1, 220, "%s (%u)", 30, _user.completed ? FL_YELLOW : FL_GREEN,
		          _user.name.empty() ? "N.N." : _user.name.c_str(), _first_level );
		if ( reversLevel() )	// draw a down-arrow to denote going reverse levels
			fl_draw_symbol( "@+22->", SCALE_X * ( x - 20 ), SCALE_Y * (210 - 15), SCALE_X * 20, SCALE_Y * 30, FL_RED );
      static int TW = -1; // dynamic calc. of required table width for key table
		int tw = drawTableBlock( TW, 270, _texts.value( "key_help", 100,
			"%c/%c\tup/down\n"
		   "%c/%c\tleft/right\n"
			"%c\tfire missile\n"
			"space\tdrop bomb\n"
			"7-9\thold game" ),
		   30, 42, FL_WHITE,
			KEY_UP, KEY_DOWN,
			KEY_LEFT, KEY_RIGHT,
			KEY_RIGHT );
		if ( TW <= 0 )
			TW = rangedValue( tw + 50, 390, 460 );
		if ( _mouseMode || _joyMode )
			drawText( -1, 500, _joyMode ? _texts.value( "joystick_mode", 30, "joystick mode" ) :
			                              _texts.value( "mouse_mode", 30, "mouse mode" ),
			          12, FL_YELLOW );
	}
	if ( G_paused )
		drawText( -1, -50, _texts.value( "paused_title", 20, "** PAUSED **" ), 40, FL_YELLOW );
	else if ( _dimmout )
	 {
		reveal_height = h();
		drawText( -1, -50, _texts.value( "get_ready", 20, "GET READY!" ), 40, FL_YELLOW );
	}
	else
	{
		static AnimText *space_to_start = 0;
		if ( !space_to_start )
			(space_to_start = new AnimText( 0, h() - 100 * SCALE_Y, w(),
				_texts.value( "space_to_start", 28, "** hit space to start **" ),
			FL_YELLOW, FL_BLACK, 40, 36, false ))->start();
		space_to_start->draw();
		drawText( -1, -26, _texts.value( "title_help", 90, "%c/%c: toggle user     %c/%c: toggle ship     s: %s sound     b: %s music" ),
			10, FL_GRAY,
			KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
			Audio::instance()->enabled() ? _texts.value( "disable", 12, "disable" ) :
			                               _texts.value( "enable", 12, "enable" ),
			Audio::instance()->bg_enabled() ? _texts.value( "disable", 12, "disable" ) :
			                               _texts.value( "enable", 12, "enable" ) );
		drawText( -90, -26, "fps=%d", 8, FL_CYAN, FPS );
	}
	drawText( 45, 34, VERSION, 8, FL_CYAN );

	if ( title_anim )
		title_anim->draw();

	if ( _gimmicks && reveal_height < h() - dborder_h )
	{
		fl_rectf( border_w, border_h + reveal_height, w() - 2 * border_w, h() - reveal_height - border_h * 2, FL_BLACK );
		reveal_height += 6 * _DX;
	}

	// scanline effect
	draw_tv();

	// fade out effect
	draw_fadeout();
}

void FltWin::draw_shaded_background( int xoff_, int W_ )
//-------------------------------------------------------------------------------
{
	// draw a shaded bg
	T.check();
	int sky_min = T.min_sky;
	int ground_min = T.min_ground;
	int H = h() - sky_min - ground_min + 1;
	assert( H > 0 );
	int y = 0;
	int W = W_;
	Fl_Color c = fl_lighter( T.bg_color );
	while ( y < H )
	{
		int x = 0;
		fl_color( fl_color_average( T.bg_color, c, float( y ) / H ) );
		while ( x < W )
		{
			while ( x < W &&
			        ( T[xoff_ + x].sky_level() > y + sky_min ||
			        h() - T[xoff_ + x].ground_level() < y + sky_min ) )
				x++;
			int x0 = x;
			while ( x < W &&
			        ( T[xoff_ + x].sky_level() <= y + sky_min &&
			        h() - T[xoff_ + x].ground_level() >= y + sky_min ) )
				x++;
			if ( x > x0 )
				fl_xyline( x0, y + sky_min , x - 1 );
		}
		y++;
	}
}

void FltWin::draw_shaded_landscape( int xoff_, int W_ )
//-------------------------------------------------------------------------------
{
	T.check();
	int s = T.max_sky;
	int g = T.max_ground;

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
			grad_rect( X, 0, D, S, s, false, c, T.sky_color );
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
			grad_rect( X, h() - G, D, G, g, true, c, T.ground_color );
			W -= D;
			X += D;
		}
	}

	// restore background
	fl_color( T.bg_color );
	for ( int i = 0; i < W_; i++ )
	{
		fl_yxline( i, T[xoff_ + i].sky_level(), h() - T[xoff_ + i].ground_level() );
	}
}

void FltWin::draw_outline( int xoff_, int W_, int outline_width_,
                           Fl_Color outline_color_sky_,
                           Fl_Color outline_color_ground_ ) const
//-------------------------------------------------------------------------------
{
	// draw ONLY outline
	// NOTE: WIN32 must set line style AFTER setting drawing color
	outline_width_ = lround( SCALE_Y * outline_width_ );

	// sky
	fl_color( outline_color_sky_ );
	fl_line_style( FL_SOLID|FL_CAP_SQUARE|FL_JOIN_ROUND, outline_width_ );
	bool vertex = false;
	for ( int i = 0; i < W_; i++ )
	{
		if ( T[xoff_ + i].sky_level() >= 0 )
		{
			if ( vertex )
			{
				fl_vertex( i, T[xoff_ + i].sky_level() );
			}
			else
			{
				fl_begin_line();
				if ( i )
					fl_vertex( i - 1, T[xoff_ + i - 1].sky_level() );
				fl_vertex( i, T[xoff_ + i].sky_level() );
				vertex = true;
			}
		}
		else
		{
			if ( vertex )
			{
				fl_vertex( i, T[xoff_ + i].sky_level() );
				fl_end_line();
				vertex = false;
			}
		}
	}
	if ( vertex )
		fl_end_line();

	// ground
	fl_color( outline_color_ground_ );
#ifdef WIN32
	fl_line_style( FL_SOLID|FL_CAP_SQUARE|FL_JOIN_ROUND, outline_width_ );
#endif
	fl_begin_line();
	for ( int i = 0; i < W_; i++ )
		fl_vertex( i, h() - T[xoff_ + i].ground_level() );
	fl_end_line();
	fl_line_style( 0 );
}

void FltWin::draw_landscape( int xoff_, int W_ )
//-------------------------------------------------------------------------------
{
	// draw landscape
	if ( _effects && !classic() )
		return draw_shaded_landscape( xoff_, W_ );

	Fl_Color outline_color_sky = T.outline_color_sky;
	Fl_Color outline_color_ground = T.outline_color_ground;
	int outline_width = T.ls_outline_width;
	if ( classic() )
	{
		// in classic mode draw only outline
		// make sure outline contrast with background
		outline_color_sky = T.sky_color == T.bg_color ?
			fl_contrast( T.sky_color, T.bg_color ) : T.sky_color;

		outline_color_ground = T.ground_color == T.bg_color ?
			fl_contrast( T.ground_color, T.bg_color ) : T.ground_color;
		// and use a default ouline width if none specified
		outline_width = outline_width ? outline_width : 3;
	}
	else
	{
		// standard non-shaded drawing
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
	}
	// draw outline (if set)
	if ( outline_width )
		draw_outline( xoff_, W_, outline_width, outline_color_sky, outline_color_ground );
}

bool FltWin::draw_decoration()
//-------------------------------------------------------------------------------
{
	bool redraw( false );
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
				static int sz = -1;
				if ( sz < 0 )
					sz = lround( SCALE_Y * 1 );
				// draw with a "twinkle" effect
				( Random::pRand() % 10 || G_paused ) ?
					sz <= 1 ? fl_point( x, h() - sy ) :
				   fl_pie( x - sz / 2, h() - sy - sz / 2, sz, sz, 0., 360. ) :
				   fl_pie( x - sz, h() - sy - sz, sz * 2, sz * 2, 0., 360. );
			}
		}
		redraw = true;
	}
#ifndef NO_PREBUILD_LANDSCAPE
	// test decoration object
	if ( _effects && _gimmicks && _landscape && !_classic ) // only possible with image-cached terrain
	{
		static Object deco;
		static int deco_x = -1;
		static int deco_y = -1;
		bool changed = deco.image( imgPath.get( "deco.png" ).c_str(), 2 ); // scale deco images x 2
		if ( changed && _faintout_deco )
			faintout_rgb_image( deco.image() );
		if ( ( !G_paused && _xoff < (int)_DX ) || deco_x == -1 || changed )
		{
			// calc. a new random position for the deco object
			deco_x = Random::pRand() % ( T.size() / 8 ) + T.size() / 16;
			int H = h() - T.min_sky - T.min_ground + 1;
			deco_y = Random::pRand() % ( H ? H / 2 : h() / 2 ) + H / 2 + T.min_ground / 3;
		}
		if ( deco.name().size() )
		{
			int xoff = _xoff / 4;	// scrollfactor 1/4
			for ( int x = 0; x < (int)SCREEN_W + deco.w(); x++ )
			{
				if ( xoff + (int)x == deco_x )
				{
					deco.x( x - deco.w() );
					deco.y( h() - deco_y - deco.h() / 2 );
					deco.draw();
					redraw = true;
					break;
				}
			}
		}
	}
#endif
	if ( TBG.flags & 1 && !classic() )
	{
		// test for "parallax scrolling" background plane
		int xoff = _xoff / 3;	// scrollfactor 1/3
		fl_color( fl_lighter( T.bg_color ) );
		for ( size_t i = 0; i < SCREEN_W; i++ )
		{
			if ( _xoff + i >= T.size() ) break;
			// TODO: take account of outline_width if drawn (currently not)?
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
	int xoff = _xoff;
	_xoff = _draw_xoff;
	do_draw();
	_xoff = xoff;
}

void FltWin::do_draw()
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
		if ( _colorChangeList.size() && _xoff >= _colorChangeList[0] )	// have we passed the next color change offset?
		{
			int xoff = _colorChangeList[0];
			_colorChangeList.erase( _colorChangeList.begin() );
			if ( !T[xoff].sky_color && !T[xoff].ground_color && !T[xoff].bg_color )
			{
				// a "restore" object (0/0/0)
				T.sky_color = T[0].sky_color;
				T.ground_color = T[0].ground_color;
				T.bg_color = T[0].bg_color;
			}
			else
			{
				// a normal color change object
				T.sky_color = T[xoff].sky_color;
				T.ground_color = T[xoff].ground_color;
				T.bg_color = T[xoff].bg_color;
			}
		}

		// draw bg
		fl_rectf( 0, 0, w(), h(), T.bg_color );

		// draw landscape
		draw_landscape( _xoff, w() );
	}

	draw_objects( true );	// objects for collision check

	if ( !G_paused && !paused() && !_done && !_collision )
	{
		_collision |= collisionWithTerrain( *_spaceship );
		if ( _collision )
			onCollision();
	}

	if ( _landscape && _background )
	{
		// "blit" in pre-built images
		_background->draw( 0, 0, w(), h(), _xoff );
		draw_decoration();
		_landscape->draw( 0, 0, w(), h(), _xoff );

		// must redraw objects
		draw_objects( true );
	}
	else
	{
		bool redraw_objects( false );
		if ( _effects > 1 && !classic() )	// only if turned on additionally
		{
			// shaded bg
			draw_shaded_background( _xoff, SCREEN_W );
			redraw_objects = true;
		}

		if ( draw_decoration() || redraw_objects )
		{
			// must redraw objects
			draw_objects( true );
		}
	}

	if ( !paused() || _frame % (FPS / 2) < FPS / 4 || _collision )
		if ( !_zoomoutShip || _zoomoutShip->done() )
			_spaceship->draw();

	draw_objects( false );	// objects AFTER collision check (=without collision)

	draw_score();

	if ( G_paused )
		reveal_width = w();
	if ( _gimmicks && reveal_width < w() )
	{
		fl_rectf( reveal_width, 0, w() - reveal_width, h(), FL_BLACK );
		reveal_width += 6 * _DX;
	}

	// draw animated title and zoomout ship over reveal rect
	if ( _anim_text )
	{
		if ( !_anim_text->done() )
			_anim_text->draw();
		else
		{
			delete _anim_text;
			_anim_text = 0;
		}
	}
	if ( _zoomoutShip && !_zoomoutShip->done() )
		_zoomoutShip->draw();

	// scanline effect
	draw_tv();

	// fade out effect
	draw_fadeout();
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
				Audio::instance()->play( "x_bomb" );
				(*r)->explode();
				create_explosion( (*r)->cx(), (*r)->cy(),
					Explosion::MC_FALLOUT_STRIKE, 0.8 );
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
				Audio::instance()->play( "x_bomb" );
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
				Audio::instance()->play( "x_bomb" );
				(*ra)->explode();
				create_explosion( (*ra)->cx(), (*ra)->cy(),
					Explosion::MC_FALLOUT_STRIKE, 0.8,
					radar_explosion, nbrOfItems( radar_explosion ) );
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
		if ( (*d)->collisionWithObject( *_spaceship ) )
		{
			if ( (*d)->x() > _spaceship->cx() + _spaceship->w() / 4 )	// front of ship
			{
				//	drop deflected
				(*d)->x( (*d)->x() + 10 );
				++d;
				continue;
			}
			//	drop fully hit spaceship
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
				Audio::instance()->play( "x_missile_hit" );
				(*r)->explode();
				create_explosion( (*r)->cx(), (*r)->cy(),
					Explosion::MC_FALLOUT_STRIKE, 0.5 );
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
				Audio::instance()->play( "x_bomb" );
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
					Audio::instance()->play( "x_missile" );
					(*ra)->explode();
					create_explosion( (*ra)->cx(), (*ra)->cy(),
						Explosion::MC_FALLOUT_STRIKE, 0.5,
						radar_explosion, nbrOfItems( radar_explosion ) );
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
					static const Fl_Color bady_smash_colors[] = { fl_rgb_color( 174, 70, 57 ), fl_rgb_color( 126, 127, 2 ) };
					create_explosion( (*b)->cx(), (*b)->cy(),
						Explosion::MC_FALLOUT_DOT, 0.5,
						bady_smash_colors, nbrOfItems( bady_smash_colors ) );
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
				create_explosion( (*d)->cx(), (*d)->cy(),
					Explosion::STRIKE, 0.3,
					drop_explosion_color, nbrOfItems( drop_explosion_color ) );
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
			if ( (*r)->collisionWithObject( *_spaceship ) )
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

void FltWin::create_explosion( int x_, int y_, Explosion::ExplosionType type_, double strength_,
                               const Fl_Color *colors_, int nColors_ )
//-------------------------------------------------------------------------------
{
	if ( _gimmicks )
	{
		Explosions.push_back( new Explosion( x_, y_, type_, strength_, colors_, nColors_ ) );
		Explosions.back()->start();
	}
}

void FltWin::create_spaceship()
//-------------------------------------------------------------------------------
{
	delete _spaceship;
	_spaceship = new Spaceship( w() / 2, 40, w(), h(), ship(),
		_effects != 0 );	// before create_terrain()!
	string shipId = "spaceship" + asString( ship() );
	string id( shipId + ".bomb_x_offset" );
	_spaceship->bombXOffset( lround( SCALE_Y *_defaultIniParameter.value( id, 0, 20, 14 ) ) );
	id = shipId + ".explosion_color";
	long explosion_color = _defaultIniParameter.value( id, 0, 0xffffff, 0xffff );
	_spaceship->explosionColor( (Fl_Color)( explosion_color << 8 ) );
	id = shipId + ".missile_color";
	long missile_color = _ini.value( id, 0, 0xffffff, 0xffffff );	// per level!
	_spaceship->missileColor( (Fl_Color)( missile_color << 8 ) );
}

void FltWin::create_objects()
//-------------------------------------------------------------------------------
{
	// Only consider scrolled part!
	// Plus half width of broadest object in advance in order to
	// make appearance of new objects smooth.
	static int wx = 0;
	if ( !wx )
		wx = _cumulus.w() / 2;	// assumption: cumulus is broadest object
	for ( int i = (_xoff == 0 ? 0 : w() - _xdelta); i < w() + wx ; i++ )
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
			r->dxRange( _rocket_dx_range );
			Rockets.push_back( r );
			o &= ~O_ROCKET;
		}
		if ( o & O_RADAR && i - _radar.w() / 2 < w() )
		{
			Radar *r = new Radar( i, h() - T[_xoff + i].ground_level() - _radar.h() / 2 );
			Radars.push_back( r );
			o &= ~O_RADAR;
		}
		if ( o & O_PHASER && i - _phaser.w() / 2 < w())
		{
			Phaser *p = new Phaser( i, h() - T[_xoff + i].ground_level() - _phaser.h() / 2 );
			p->dxRange( _phaser_dx_range );
			Phasers.push_back( p );
			o &= ~O_PHASER;
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
			d->dxRange( _drop_dx_range );
			Drops.push_back( d );
			o &= ~O_DROP;
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
			b->dxRange( _bady_dx_range );
			Badies.push_back( b );
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
	for ( size_t i = 0; i < Explosions.size(); i++ )
		delete Explosions[i];
	Explosions.clear();
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
		Bady& badie = *Badies[i];
		if ( !paused() )
			badie.x( badie.x() - _xdelta );

		int top = T[_xoff + badie.x() + badie.w() / 2].sky_level();
		int bottom = h() - T[_xoff + badie.x() + badie.w() / 2].ground_level();
		bool gone = badie.x() < -badie.w();
		if ( gone )
		{
			Bady *b = Badies[i];
			Badies.erase( Badies.begin() + i );
			delete b;
			i--;
			continue;
		}
		else if ( !badie.started() )
		{
			// bady fall strategy: always start!
			int speed = rangedValue( rangedRandom( _bady_min_start_speed, _bady_max_start_speed ), 1, 10 );
			badie.start( speed );
			assert( badie.started() );
		}
		else if ( ( badie.y() + badie.h() >= bottom &&
		            !badie.turned() ) ||
		          ( badie.y() <= top  &&
		            badie.turned() ) )
		{
			badie.turn();
		}
	}
}

void FltWin::update_bombs()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Bombs.size(); i++ )
	{
		Bomb& bomb = *Bombs[i];
		bool gone = bomb.y() > h() ||
		            bomb.y() + bomb.h() > h() - T[_xoff + bomb.x()].ground_level();
		if ( gone )
		{
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
		Cumulus& cumulus = *Cumuluses[i];
		if ( !paused() )
			cumulus.x( cumulus.x() - _xdelta );

		int top = T[_xoff + cumulus.x() + cumulus.w() / 2].sky_level();
		int bottom = h() - T[_xoff + cumulus.x() + cumulus.w() / 2].ground_level();
		bool gone = cumulus.x() < -cumulus.w();
		if ( gone )
		{
			Cumulus *c = Cumuluses[i];
			Cumuluses.erase( Cumuluses.begin() + i );
			delete c;
			i--;
			continue;
		}
		else if ( !cumulus.started() )
		{
			// cumulus fall strategy: always start!
			int speed = rangedValue( rangedRandom( _cumulus_min_start_speed, _cumulus_max_start_speed ), 1, 10 );
			cumulus.start( speed );
			assert( cumulus.started() );
		}
		else if ( ( cumulus.y() + cumulus.h() >= bottom &&
		            !cumulus.turned() ) ||
		          ( cumulus.y() <= top  &&
		            cumulus.turned() ) )
		{
			cumulus.turn();
		}
	}
}

void FltWin::update_drops()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Drops.size(); i++ )
	{
		Drop& drop = *Drops[i];
		if ( !paused() )
			drop.x( drop.x() - _xdelta );

		int bottom = h() - T[_xoff + drop.x()].ground_level();
		if ( 0 == bottom ) bottom += drop.h();	// glide out completely if no ground
		bool gone = drop.y() + drop.h() / 2 > bottom || drop.x() < -drop.w();
		if ( gone )
		{
			Drop *d = Drops[i];
			Drops.erase( Drops.begin() + i );
			if ( d->x() + d->w() > 0 && bottom )
			{
				create_explosion( d->cx(), d->cy(),
					Explosion::SPLASH_STRIKE, 0.3,
					drop_explosion_color, nbrOfItems( drop_explosion_color ) );
			}
			delete d;
			i--;
			continue;
		}
		else
		{
			// drop fall strategy...
			if ( drop.nostart() ) continue;
			if ( drop.dropped() ) continue;
			// if user has completed all 10 levels and revers, objects start later (which is harder)...
			int dist = ( user_completed() / 2 ) ? drop.cx() - _spaceship->cx() // center distance S<->R
			                                    : drop.x() - _spaceship->x();
			int start_dist = (int)drop.data1();	// 400 - _level * 20 - Random::Rand() % 200;
			if ( dist <= 0 || dist >= start_dist ) continue;
			// start drop with probability drop_start_prob/start_dist
			unsigned drop_prob = _drop_start_prob;
			if ( Random::Rand() % 100 < drop_prob )
			{
				int speed = rangedValue( rangedRandom( _drop_min_start_speed, _drop_max_start_speed ), 1, 10 );
				drop.start( speed );
				assert( drop.dropped() );
				continue;
			}
			drop.nostart( true );
		}
	}
}

void FltWin::update_explosions()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Explosions.size(); i++ )
	{
		Explosion& explosion = *Explosions[i];
		if ( !paused() )
			explosion.x( explosion.x() - _xdelta );
		if ( explosion.done() )
		{
			Explosion *e = Explosions[i];
			Explosions.erase( Explosions.begin() + i );
			delete e;
			i--;
			continue;
		}
	}
}

void FltWin::update_missiles()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Missiles.size(); i++ )
	{
		Missile& missile = *Missiles[i];
		bool gone = missile.exhausted() ||
		            missile.x() > w() ||
		            missile.y() > h() - T[_xoff + missile.x() + missile.w()].ground_level() ||
		            missile.y() < T[_xoff + missile.x() + missile.w()].sky_level();
		if ( gone )
		{
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
		Phaser& phaser = *Phasers[i];
		if ( !paused() )
			phaser.x( phaser.x() - _xdelta );

		bool gone = phaser.exploded() || phaser.x() < -phaser.w();
		if ( gone )
		{
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
		Radar& radar = *Radars[i];
		if ( !paused() )
			radar.x( radar.x() - _xdelta );

		bool gone = radar.exploded() || radar.x() < -radar.w();
		if ( gone )
		{
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
		Rocket& rocket = *Rockets[i];
		if ( !paused() )
			rocket.x( rocket.x() - _xdelta );

		int top = T[_xoff + rocket.x()].sky_level();
		if ( top < 0 ) top -= rocket.h();	// glide out completely if no sky ( <= -1 )
		bool gone = rocket.exploded() || rocket.x() < -rocket.w();
		// NOTE: when there's no sky, explosion is not visible, but still occurs,
		//       so checking exploded() suffices as end state for all rockets.
		if ( gone )
		{
			Rocket *r = Rockets[i];
			Rockets.erase( Rockets.begin() + i );
			delete r;
			i--;
			continue;
		}
		else if ( rocket.y() <= top && !rocket.exploding() )
		{
			if ( _completed && !reversLevel() )
			{
				static const Fl_Color mega_explosion_colors[] = { FL_YELLOW, FL_WHITE, FL_RED, FL_GREEN };
				rocket.explode();
				create_explosion( rocket.cx(), rocket.cy(),
					Explosion::MC_FALLOUT_STRIKE, 1.2,
				   mega_explosion_colors, nbrOfItems( mega_explosion_colors ) );
			}
			else
				rocket.crash();
		}
		else if ( !rocket.exploding() )
		{
			// rocket fire strategy...
			if ( rocket.nostart() ) continue;
			if ( rocket.lifted() ) continue;
			// if user has completed all 10 levels and revers, objects start later (which is harder)...
			int dist = ( user_completed() / 2 ) ? rocket.cx() - _spaceship->cx() // center distance S<->R
			                                    : rocket.x() - _spaceship->x();
			int start_dist = (int)rocket.data1();	// 400 - _level * 20 - Random::Rand() % 200;
			if ( dist <= 0 || dist >= start_dist ) continue;
			// start rocket with probability rocket_start_prob/start_dist
			unsigned lift_prob = _rocket_start_prob;
			// if a radar is within start_dist then the
			// probability gets much higher
			int x = rocket.x();
			for ( size_t ra = 0; ra < Radars.size(); ra++ )
			{
				if ( Radars[ra]->defunct() ) continue;
				if ( Radars[ra]->x() >= x ) continue;
				if ( Radars[ra]->x() <= x - start_dist ) continue;
				// there's a working radar within start_dist
				lift_prob = _rocket_radar_start_prob;
				break;
			}
			if ( Random::Rand() % 100 < lift_prob )
			{
				int speed = rangedValue( rangedRandom( _rocket_min_start_speed, _rocket_max_start_speed ), 1, 10 );
				rocket.start( speed );
				assert( rocket.lifted() );
				continue;
			}
			rocket.nostart( true );
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
	update_explosions();
}

void FltWin::setIcon()
//-------------------------------------------------------------------------------
{
#if FLTK_HAS_NEW_FUNCTIONS
	if ( _spaceship && _spaceship->origImage() )
		Fl_Window::default_icon( (Fl_RGB_Image *)_spaceship->origImage() );
	else
	{
		Fl_GIF_Image defIcon( imgPath.get( "spaceship0.gif" ).c_str() );
		Fl_RGB_Image rgbDefIcon( &defIcon );
		Fl_Window::default_icon( &rgbDefIcon);
	}
#endif
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

void FltWin::newUser()
//-------------------------------------------------------------------------------
{
	_user.name = DEFAULT_USER;
	setUser();
	keyClick();
	changeState( TITLE, true );	// immediate display + reset demo timer
}

void FltWin::setUser()
//-------------------------------------------------------------------------------
{
	_user = _cfg->readUser( _user.name );
	if ( !_user.level )
	{
		_first_level = reversLevel() ? MAX_LEVEL : 1;
	}
	else
	{
		_first_level = _user.level + levelIncrement();
		if ( _first_level > MAX_LEVEL )
			_first_level = MAX_LEVEL;
		if ( _first_level < 1 )
			_first_level = 1;
	}
	_level = _first_level;
	DBG( "user " << _user.name << " completed: " << _user.completed );
}

void FltWin::resetUser()
//-------------------------------------------------------------------------------
{
	_user.score = 0;
	_user.completed = 0;
	_user.ship = -1;
	_first_level = 1;
	_level = 1;
	_cfg->writeUser( _user );	// but no flush() - must play one level to make permanent!
	_cfg->load();
	keyClick();
	changeState( TITLE, true );	// immediate display + reset demo timer
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
				size_t picked = Random::pRand() % bgSoundList.size();
				if ( bgSoundList[ picked ] == _bgsound && bgSoundList.size() > 1 )
				{
					bgSoundList.erase( bgSoundList.begin() + picked );
					picked = Random::pRand() % bgSoundList.size();
				}
				_bgsound = bgSoundList[ picked ];
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
		_dimmout = paused_;
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

	if ( fullscreen_ )
	{
		// Check special case resolution matches work area
		Fl::screen_work_area( X, Y, W, H );
		if ( W == (int)SCREEN_W && H == (int)SCREEN_H )
		{
			PERR( "Use full screen work area " << X << "/" << Y << " " << W << "x" << H );
			border( 0 );
			position( X, Y );
			show();
			return true;
		}
	}

	bool isFull = fullscreen_active();

	if ( fullscreen_ )
	{
		// switch to fullscreen
		if ( !isFull )
		{
			ox = x();
			oy = y();
			hide();
			border( 0 ); // this is neccessary, otherwise border may be seen (shortly)!
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
					border( 1 );
					PERR( "Failed to set resolution to " << SCREEN_W << "x"<< SCREEN_H <<
					      " (is: " << W << "x" << H << ")" );
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
			border( 1 );
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
	return fullscreen_active();
}

/*virtual*/
void FltWin::show()
//-------------------------------------------------------------------------------
{
	Inherited::show();
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
}

bool FltWin::toggleBorder()
//-------------------------------------------------------------------------------
{
	if ( !fullscreen_active() )
	{
		if ( border() )
		{
			hide();
			border( 0 );
			show();
			position( x(), y() - 20 );
		}
		else
		{
			hide();
			border( 1 );
			show();
		}
		return true;
	}
	return false;
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
	_dimmout = !G_paused;
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
	int ships = _defaultIniParameter.value( "ships", 2, 9, 4 );
	_ship += prev_ ? -1 : 1;
	if ( _ship < 0 )
		_ship = ships - 1;
	if ( _ship > ships - 1 )
		_ship = 0;
	_user.ship = _ship;
	_cfg->writeUser( _user );
	_cfg->flush();
	keyClick();
	create_spaceship();
	changeState( TITLE, true );	// immediate display + reset demo timer
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
		string user_name = _cfg->users()[found].name;
		if ( _user.name != user_name )
		{
			_user.name = user_name;
			setUser();
			_cfg->writeUser( _user );
			_cfg->flush();
			if ( _user.ship >= 0 && _user.ship != _ship )
			{
				_ship = _user.ship;
				create_spaceship();
			}
		}
		keyClick();
		changeState( TITLE, true );	// immediate display + reset demo timer
	}
}

bool FltWin::dropBomb()
//-------------------------------------------------------------------------------
{
	if ( !_bomb_lock && !paused() && Bombs.size() < 5 )
	{
		Bomb *b = new Bomb( _spaceship->x() + _spaceship->bombPoint().x,
		                    _spaceship->y() + _spaceship->bombPoint().y +
		                    _spaceship->bombXOffset() );
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
		                          _spaceship->y() + _spaceship->missilePoint().y,
		                          _spaceship->missileColor() );
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

/*static*/
void FltWin::cb_action_key_delay( void *d_ )
//-------------------------------------------------------------------------------
{
	FltWin *f = (FltWin *)d_;
	f->onActionKey( false );
}

/*static*/
void FltWin::cb_fire_key_delay( void *d_ )
//-------------------------------------------------------------------------------
{
	FltWin *f = (FltWin *)d_;
	f->_right = true;
}

void FltWin::onActionKey( bool delay_/* = true*/ )
//-------------------------------------------------------------------------------
{
	Fl::remove_timeout( cb_demo, this );
	Fl::remove_timeout( cb_paused, this );
	Fl::remove_timeout( cb_action_key_delay, this );
	if ( delay_ )
	{
		// Delay handling of action key to allow re-entering mainloop
		// for drawing updates.
		// This has become necessary because creating the level images
		// may take a while in higher resolutions, so some optical
		// feedback is required.
		Fl::add_timeout( 1. / 50, cb_action_key_delay, this );
		_dimmout = true;	// signal: gray out screen
		redraw();	// update the screen
		return;
	}

	if ( _state == DEMO )
	{
		_demoData.clear();
		_done = true;	// ensure ship gets reloaded in onNextScreen()
		G_paused = false;
		return;
	}
	if ( _state == SCORE )
	{
		if ( _input.empty() )
			_input = DEFAULT_USER;
		else
		{
			if ( _user.name == DEFAULT_USER && _cfg->existsUser( _input ) )
			{
				// do not allow overwriting existing user
				return;
			}
		}
		if ( _user.name == DEFAULT_USER && _input != DEFAULT_USER )
			_cfg->removeUser( _user.name );	// remove default user
		if ( _user.name == DEFAULT_USER )
			_user.name = _input;
		_cfg->writeUser( _user );
		_cfg->flush();
		_hiscore = _cfg->hiscore();
		_hiscore_user = _cfg->best().name;
		// score saved.
	}

	changeState();
}

void FltWin::onCollision()
//-------------------------------------------------------------------------------
{
	if ( _done || ( _state == DEMO && !_exit_demo_on_collision ) )
		_collision = false;
	else if ( _cheatMode )
	{
		if ( _xoff % 20 == 0 )
			Audio::instance()->play( "boink" );
		_collision = false;
	}
	if ( _collision && _spaceship)
		_spaceship->explode();
	if ( _state == DEMO && _exit_demo_on_collision )
		_done = true;
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
	if ( !_focus_out && !_showFirework )
		return;

	if ( _state == TITLE || _state == DEMO )
	{
		setPaused( false );
		_state = START;
		changeState( TITLE );
	}
	if ( _state == PAUSED )
	{
		setPaused( false );
		Fl::remove_timeout( cb_paused, this );
		Fl::add_timeout( _TO, cb_paused, this );
		return;
	}
	setTitle();
}

void FltWin::onLostFocus()
//-------------------------------------------------------------------------------
{
	if ( !_focus_out )
		return;

	_left = _right = _up = _down = false;	// invalidate keys
	Fl::focus( 0 );
	setPaused( true );	// (this also stops bg sound)
	if ( _state == TITLE || _state == DEMO )
	{
		changeState( TITLE, true );
	}
	setTitle();
}

void FltWin::onDemo()
//-------------------------------------------------------------------------------
{
	// starting demo...
	_state = DEMO;
	onNextScreen( true );
}

void FltWin::onNextScreen( bool fromBegin_/* = false*/ )
//-------------------------------------------------------------------------------
{
	DBG( "onNextScreen(" << fromBegin_ << ") " << _first_level );
	delete _spaceship;
	_spaceship = 0;
	delete _anim_text;
	_anim_text = 0;

	delete_objects();

	_xoff = 0;
	_draw_xoff = 0;
	_dxoff = 0.;
	_frame = 0;
	_collision = false;
	bool completed( _completed );
	bool done( _done );

	_completed = false;
	_done = false;
	_left = _right = _up = _down = false;

	bool show_scores = false;
	if ( _state == LEVEL )
	{
		if ( _demoData.size() && (int)_demoData.size() >= _final_xoff - w() / 2 &&
		     !_trainMode && !_no_demo )
			saveDemoData();
		_demoData.clear();
	}

	_end_level = reversLevel() ? 1 : MAX_LEVEL;
	if ( fromBegin_ )
	{
		if ( completed && !_trainMode )
		{
			show_scores = _cfg->readUser( _user.name ).score > (int)_hiscore;
			if ( show_scores )
			{
				_input = _user.name == DEFAULT_USER ? "" : _user.name;
				changeState( SCORE );
				return;
			}

			changeState( TITLE );
			return;
		}

		Random::Srand( time( 0 ) );	// otherwise determined because of demo seed!
		_level = _state == DEMO ? pickRandomDemoLevel( 1,
		                          max( 3, _user.completed ? (int)MAX_LEVEL : (int)_level ) )
		                        : _first_level;
		_level_repeat = 0;
		_bonus = 0;
		_user.score = _cfg->readUser( _user.name).score;
	}
	else
	{
		show_scores = _cfg->readUser( _user.name ).score > (int)_hiscore && !done;

		if ( done || _state == DEMO )
		{
			_level += levelIncrement();
			_level_repeat = 0;

			if ( _level > MAX_LEVEL || _level < 1 || _state == DEMO/*new: demo stops after one level*/ )
			{
				_level = _first_level; // restart with level 1
				_user.score = _cfg->readUser( _user.name ).score;
				changeState( TITLE );
				return;
			}
		}
		else
		{
			_level_repeat++;
			if ( _level_repeat > MAX_LEVEL_REPEAT )
			{
				_level = _first_level;
				_level_repeat = 0;
				_bonus = 0;

				show_scores &= _level == _first_level && _level_repeat == 0;
				if ( show_scores )
				{
					_input = _user.name == DEFAULT_USER ? "" : _user.name;
					changeState( SCORE );
					return;
				}
				else
				{
					changeState( TITLE );
				}
			}
		}
	}

	if ( _state == DEMO )
	{
		loadDemoData();
		if ( _correct_speed )
			_exit_demo_on_collision = true;
	}

	create_spaceship();	// after loadDemoData()!

	if ( !create_terrain() )
	{
		hide();
		return;
	}

	// prepare color change list
	_colorChangeList.clear();
	for ( size_t i = 0; i < T.size(); i++ )
		if ( T[ i ].object() & O_COLOR_CHANGE )
			_colorChangeList.push_back( i );

	if ( _state == DEMO )
	{
		if ( _demoData.size() && (int)_demoData.size() < _final_xoff - w() / 2 )
		{
			// clear incomplete demo data
			PERR( "demo " << demoFileName() << " is corrupt" <<
			      " (" << _demoData.size() << "<" << _final_xoff - w() / 2 << ")" );
			_demoData.clear();
		}

		if ( !_demoData.size() )
		{
			changeState( TITLE );
		}
	}

	position_spaceship();

	if ( !_level_repeat )
	{
		(_anim_text = new AnimText( 0, SCALE_Y * 20, w(), _level_name.c_str() ))->start();
		if ( _state == LEVEL )
			setBgSoundFile();
	}
	// start ship animation titlescreen->playscreen effect
	zoomoutShip();

	if ( Fl::focus() == this )
		setPaused( false );

	if ( _state == LEVEL )
	{
		startBgSound();
	}
}

bool FltWin::zoominShip( bool updateOrigin_ )
//-------------------------------------------------------------------------------
{
	if ( !_gimmicks )
		return false;

	if ( !_spaceship )
		create_spaceship();

	// start ship movin animation?
	if ( _zoominShip && _zoominShip->src().name() == _spaceship->flt_image().name() )
		if ( !_zoominShip->done() )
			return false;

	// start new animation
	int x_origin = _zoominShip ? Random::pRand() % w() : _spaceship->x();
	int y_origin = _zoominShip ? Random::pRand() % h() / 2 + h() / 2 : _spaceship->y();
	if ( !_zoominShip || _zoominShip->src().name() != _spaceship->flt_image().name() )
	{
		// ship changed, need to re-create animation from new ship image
		delete _zoominShip;

		_zoominShip = new ImageAnimation( _spaceship->flt_image(), 1.0,
	                                     _effects > 1 && FPS >= 100 );
		_zoominShip->setSizeMoveFromTo(
		                 ImageAnimation::Rect( x_origin, y_origin,
		                                       _spaceship->w(), _spaceship->h() ),
		                 ImageAnimation::Rect( SCALE_X * 60, SCALE_Y * 40, w() - SCALE_X * 120, h() - SCALE_Y * 150 ) );
		_zoominShip->start();
	}
	else if ( updateOrigin_ )
	{
		// update only start position (keep images)
		_zoominShip->stop();
		_zoominShip->setMoveFrom( x_origin, y_origin );
		_zoominShip->start();
	}
	return true;
}

bool FltWin::zoomoutShip()
//-------------------------------------------------------------------------------
{
	if ( !_gimmicks )
		return false;

	if ( !_zoomoutShip || _zoomoutShip->src().name() != _spaceship->flt_image().name() )
	{
		// ship changed, need to re-create animation from new ship image
		delete _zoomoutShip;
		_zoomoutShip = new ImageAnimation( _spaceship->flt_image(), 1.0,
		                                   _effects > 1 && FPS >= 100 );
		_zoomoutShip->setSizeMoveFromTo(
		                 ImageAnimation::Rect( SCALE_X * 60, SCALE_Y * 40, w() - SCALE_X * 120, h() - SCALE_Y * 150 ),
		                 ImageAnimation::Rect( _spaceship->x(), _spaceship->y(),
		                                       _spaceship->w(), _spaceship->h() ) );
	}
	else
	{
		// update only destination position (keep images)
		_zoomoutShip->stop();
		_zoomoutShip->setMoveTo( _spaceship->x(), _spaceship->y() );
	}
	_zoomoutShip->start();
	return true;
}

void FltWin::onTitleScreen()
//-------------------------------------------------------------------------------
{
	if ( _trainMode )
		return;
	bool enterTitle = last_state() != TITLE && last_state() != NO_STATE;
	_state = TITLE;
	_frame = 0;
	Fl::remove_timeout( cb_demo, this );
	if ( !G_paused && Fl::focus() == this )
	{
		// set timeout for demo start
		if ( !_no_demo )
			Fl::add_timeout( 20.0, cb_demo, this );

		// start ship animation playscreen->titlescreen effect
		zoominShip( enterTitle );
	}
}

void FltWin::onUpdateDemo()
//-------------------------------------------------------------------------------
{

	int cx = 0;
	int cy = 0;
	bool bomb( false );
	bool missile( false );
	uint32_t seed, seed2;
	if ( !_demoData.get( _xoff, cx, cy, bomb, missile, seed, seed2 ) )
		_done = true;
	else
	{
		Random::Srand( seed, seed2 );

		update_objects();

		create_objects();

		check_hits();

		// use Spaceship::right()/left() methods for accel/decel feedback in demo
		while ( _spaceship->cx() < cx )
		{
			int ocx = _spaceship->cx();
			_spaceship->right();
			if ( ocx == _spaceship->cx() )
				break;
		}
		while ( _spaceship->cx() > cx )
		{
			int ocx = _spaceship->cx();
			_spaceship->left();
			if ( ocx == _spaceship->cx() )
				break;
		}
//		_spaceship->cx( cx );
		_spaceship->cy( cy );
		if ( _zoomoutShip && !_zoomoutShip->done() )
			_zoomoutShip->setMoveTo( _spaceship->x(), _spaceship->y() );
		if ( bomb )
			dropBomb();
		if ( missile )
			fireMissile();

		// reset missile/bomb for this xoff (maybe read again with next update)
		_demoData.set( _xoff, cx, cy, false, false, seed, seed2 );
	}

	_draw_xoff = _xoff;
	redraw();

	if ( _done )
	{
		_demoData.clear();
		onNextScreen();	// ... but for now, just skip pause in demo mode
		return;	// do not increment _xoff now!
	}

	_dxoff += _DDX;
	int oxoff = _xoff;
	_xoff = _dxoff;
	_xdelta = _xoff - oxoff;
	if ( _xoff + w() > (int)T.size() )
	{
		_xoff = T.size() - w();
		_dxoff = _xoff;
		_done = true;
	}
}

bool FltWin::correctDX()
//-------------------------------------------------------------------------------
{
	// intended scroll rate: 800px in 4s => 0.2px / ms
	_DDX = SCALE_X * (double)_waiter.elapsedMicroSeconds() * 0.0002;
	_DX = ceil( _DDX );
#if 0
	// set limit
	static unsigned int MAX_DX = 0;
	if ( !MAX_DX )
		MAX_DX = ceil( SCALE_X * 10 );
	if ( _DX > MAX_DX )
	{
		_DX = MAX_DX;
		return false;
	}
#endif
	return true;
}

void FltWin::onUpdate()
//-------------------------------------------------------------------------------
{
	if ( _mouseMode )
	{
		handle( TIMER_CALLBACK );
	}

	if ( _state == TITLE || _state == SCORE || G_paused )
	{
		_draw_xoff = _xoff;
		redraw();
		return;
	}

	uint32_t seed, seed2;
	seed = Random::Seed( seed2 );
	_demoData.setSeed( _xoff, seed, seed2 );

	update_objects();

	create_objects();

	check_hits();

	if ( paused() )
	{
		Audio::instance()->check( true );	// reliably stop bg-sound
		_draw_xoff = _xoff;
		redraw();
		return;
	}

	static unsigned last_score_step = 0;
	unsigned step = _xoff / SCORE_STEP;
	if ( step != last_score_step )
	{
		if ( _xoff > (int)( last_score_step * SCORE_STEP ) )
			add_score( 1 );
		last_score_step = step;
		Audio::instance()->check();	// test bg-sound expired
	}

	if ( _left && _xoff + _spaceship->cx() < _final_xoff - w() / 2 ) // no retreat at end!
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
	if ( _down )
		_spaceship->down();
	if ( _zoomoutShip && !_zoomoutShip->done() )
		_zoomoutShip->setMoveTo( _spaceship->x(), _spaceship->y() );

	_demoData.setShip( _xoff, _spaceship->cx(), _spaceship->cy() );

	_draw_xoff = _xoff;
	redraw();
	if ( _collision )
		changeState( LEVEL_FAIL );
	else if ( _done )
		changeState( LEVEL_DONE );

	if ( !_done )
	{
		_dxoff += _DDX;
		int oxoff = _xoff;
		_xoff = _dxoff;
		_xdelta = _xoff - oxoff;
		if ( _xoff + _spaceship->x() > _final_xoff )
		{
			_xoff = _final_xoff - _spaceship->x();
			_dxoff = _xoff;
			_done = true;
			_bonus = 500 - 200 * _level_repeat;
			add_score( _bonus );
		}
	}
}

#include "Fl_ZXAttrEffect.H"
//-------------------------------------------------------------------------------
class ZXAttrEffect : public Fl_ZXAttrEffect
//-------------------------------------------------------------------------------
{
typedef Fl_ZXAttrEffect Inherited;
public:
	ZXAttrEffect( Fl_Window& win_ ) :
		Inherited( win_ )
	{
		while ( Fl::first_window() && win_.children() )
		{
#ifndef WIN32
			Fl::wait( 0.0005 );
#else
			Fl::wait( 0.0 );
#endif
		}
	}
};

int FltWin::handle( int e_ )
//-------------------------------------------------------------------------------
{
#define F10_KEY  (FL_F + 10)
#define F12_KEY  (FL_F + 12)
	static bool ignore_space = false;

	if ( FL_FOCUS == e_ )
	{
		Fl::focus( this );
		onGotFocus();
		return 1;
	}
	if ( FL_UNFOCUS == e_ )
	{
		onLostFocus();
		return 1;
	}

	if ( _disableKeys )
		return Inherited::handle( e_ );

	if ( _mouseMode )
	{
		static int x0 = 0;
		static int y0 = 0;
		static bool dragging = false;
		static int pushed = 0;
		static int wheel = 0;
		static bool bomb = false;

		int x = Fl::event_x();
		int y = Fl::event_y();
		switch ( e_ )
		{
			case TIMER_CALLBACK:
				// trigger bomb with longer, non-dragging button press
				if ( pushed )
					pushed += _DX;
				if ( bomb || ( pushed > 40 * SCALE_X && !dragging ) )
				{
					dropBomb();
					pushed = 0;
				}
				bomb = false;
				static int cnt = 0;
				cnt += _DX;
				if ( cnt >= 10 )
				{
					cnt = 0;
					if ( wheel )
					{
						wheel--;
						if ( !wheel )
						{
							_up = _down = false;
						}
					}
				}
				return 1;
				break;
			case FL_LEAVE:
				dragging = false;
				pushed = 0;
				bomb = false;
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

				// try smoother x/y movement if one direction dominates...
				if ( dy && abs( dx ) >= abs( dy ) * 4 )
				{
					dy = 0;
				}
				else if ( dx && abs( dy ) >= abs( dx ) * 4 )
				{
					dx = 0;
				}
				// determine movement of ship
				if ( dx < 0 )
				{
					_left = true;
					_right = false;
				}
				if ( dx > 0 )
				{
					_right = true;
					_left = false;
				}
				if ( dy < 0 )
				{
					_up = true;
					_down = false;
				}
				if ( dy > 0 )
				{
					_down = true;
					_up = false;
				}
				if ( dx == 0 )
				{
					_left = _right = false;
				}
				if ( dy == 0 )
				{
					_up = _down = false;
				}
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
				{
					_up = true;
					_down = false;
				}
				else if ( Fl::event_dy() > 0 )
				{
					_down = true;
					_up = false;
				}
				break;
			case FL_PUSH:
				setPaused( false );
				dragging = false;
				pushed = _state == LEVEL;
				bomb = Fl::event_button3();	// always trigger bomb with right mouse
				x0 = x;
				y0 = y;
				break;
			case FL_RELEASE:
				if ( _state == TITLE || _state == SCORE || _state == DEMO || _done )
				{
					if ( x < 40 * SCALE_X && y < 40 * SCALE_Y )
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
				// trigger missile with short non-dragging button press
				if ( pushed && !dragging )
				{
					_speed_right = 0;	//???
					fireMissile();
				}
				_left = _right = _up = _down = false;
				dragging = false;
				pushed = 0;
				bomb = false;
				break;
		}
	}	// _mouseMode

	int c = Fl::event_key();

	//	handle event 'e' / key 'c'
	if ( ( e_ == FL_KEYDOWN || e_ == FL_KEYUP ) && c == FL_Escape )
	{
		// get rid of escape key closing window (except in boss mode and title screen)
		if ( _enable_boss_key || _state == TITLE || fullscreen_active() )
		{
			if ( e_ == FL_KEYDOWN && _gimmicks && !_enable_boss_key )
			{
				_disableKeys = true;
				ZXAttrEffect zxattr( *this );
				_disableKeys = false;
			}
			hide();
		}
		return 1;
	}

	if ( e_ == FL_KEYUP )
	{
		if ( _state != SCORE )
		{
			if ( 's' == c )
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
			else if ( 'b' == c )
			{
				if ( _state == TITLE )
					keyClick();
				toggleBgSound();
			}
			else if ( 'x' == c )
			{
				if ( _state == TITLE )
					keyClick();
				Audio::instance()->noExplosions( !Audio::instance()->noExplosions() );
			}
		}
		if ( F10_KEY == c && ( _state != LEVEL || G_paused ) )
		{
			toggleFullscreen();
		}
		else if ( F12_KEY == c && ( _state != LEVEL || G_paused ) )
		{
			toggleBorder();
		}
	}
	if ( _state == TITLE || _state == SCORE || _state == DEMO || _done )
	{
		if ( e_ == FL_JOY_AXIS )
		{
			if ( _joystick.left() ) { e_ = FL_KEYUP; c = KEY_LEFT; }
			if ( _joystick.right() ) { e_ = FL_KEYUP; c = KEY_RIGHT; }
			if ( _joystick.up() ) { e_ = FL_KEYUP; c = KEY_UP; }
			if ( _joystick.down() ) { e_ = FL_KEYUP; c = KEY_DOWN; }
		}
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
			else if ( 'n' == c )
				newUser();
			else if ( 'd' == c )
				cb_demo( this );
			else if ( '-' == Fl::event_text()[0] )
				setup( FPS - 1, _HAVE_SLOW_CPU, _USE_FLTK_RUN );
			else if ( '+' == Fl::event_text()[0] )
			{
				setup( -FPS - 1, _HAVE_SLOW_CPU, _USE_FLTK_RUN );
			}
		}
		if ( ( e_ == FL_KEYDOWN && ' ' == c ) ||
		     ( e_ == FL_JOY_BUTTON_DOWN ) )
		{
			setPaused( false );
			ignore_space = true;
			keyClick();
			onActionKey();
		}
		return Inherited::handle( e_ );
	}

	// joystick handling
	if ( FL_JOY_BUTTON_DOWN == e_ )
	{
		unsigned int button = _joystick.event().number;
		if ( button > _joystick.numberOfButtons() / 2 )
			fireMissile();
		else
			dropBomb();
	}
	else if ( FL_JOY_AXIS == e_ )
	{
		_left = _joystick.left();
		_right = _joystick.right();
		_up = _joystick.up();
		_down = _joystick.down();
		if ( !_left && !_right )
			_speed_right = 0;
	}

	if ( e_ == FL_KEYDOWN && Fl::get_key( c ) )
	{
		if ( F10_KEY != c )
			setPaused( false );
		if ( KEY_LEFT == c )
			_left = true;
		else if ( KEY_RIGHT == c && !Fl::has_timeout( cb_fire_key_delay, this ) && !_right )
		{
			Fl::remove_timeout( cb_fire_key_delay, this );
			Fl::add_timeout( 0.15, cb_fire_key_delay, this );
		}
		else if ( KEY_UP == c )
			_up = true;
		else if ( KEY_DOWN == c )
			_down = true;
		return 1;
	}
	if ( e_ == FL_KEYUP && !Fl::get_key( c ) )
	{
		if ( KEY_LEFT == c )
			_left = false;
		else if ( KEY_RIGHT == c )
		{
			_right = false;
			_speed_right = 0;
			if ( Fl::has_timeout( cb_fire_key_delay, this ) )
				fireMissile();
			Fl::remove_timeout( cb_fire_key_delay, this );
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
		LOG( "using Fl::run()" );
		Fl::add_timeout( FRAMES, cb_update, this );
		return Fl::run();
	}

	LOG( "using own main loop" );
	while ( Fl::first_window() )
	{
		_waiter.wait( FPS );
		_correct_speed && correctDX();
		_state == DEMO ? onUpdateDemo() : onUpdate();
	}
	return 0;
}

string FltWin::firstTimeSetup()
//-------------------------------------------------------------------------------
{
	string cmd;
	fl_message_title_default( "FLTrator first time setup" );
	static const char YES[] = "YES";
	static const char NO[] = "NO";
	int fast_slow = fl_choice( "Do you have a fast computer?\n\n"
	                           "If you have any recent hardware\n"
	                           "you should answer 'yes'.",
	                           "ASK ME LATER",  NO, YES );
	if ( !fast_slow )	// abort by Esc or "ask me later'
	{
		fl_message_title_default( 0 );
		return cmd;
	}

	int speed = 0;
	if ( fast_slow == 2 )
	{
		speed = fl_choice( "*How* fast is your computer?\n\n"
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
		speed = fl_choice( "*How* slow is your computer?\n\n"
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
	int ship = fl_choice( "Use penetrator like spaceship type?\n\n"
	                      "'No' uses the standard ship.",
	                      NULL, NO, YES );
	if ( ship == 2 )
		cmd += " -a";

	if ( fast_slow == 2 )
	{
		int fullscreen = fl_choice( "Start in fullscreen mode?\n\n"
		                            "NOTE: You can always toggle fullscreen mode\n"
		                            "on game title screen with F10.",
		                            NULL, NO, YES );
		if ( fullscreen == 2 )	// YES
		{
			cmd += " -f";
			if ( speed == 2 )	// very fast computer
#ifdef WIN32
				cmd += " -Wf -r40";
#else
				cmd += " -Wf -R50";
#endif
		}
		else if ( speed == 2 )	// very fast computer
#ifdef WIN32
			cmd += " -W -r40";
#else
			cmd += " -W -R50";
#endif

		if ( speed == 2 )	// very fast computer
		{
			int all_effects = fl_choice( "Turn on ALL effects?\n\n",
			                             NULL, NO, YES );
			if ( all_effects == 2 ) // YES
			{
				size_t pos = cmd.find( "-FF" );
				if ( pos != string::npos )
					cmd.insert( pos + 1, "F" );
			}
		}
	}

	trim( cmd );
	_cfg->set( "defaultArgs", cmd.c_str() );
	fl_message( "Configuration saved to\n"
	            "'%s'.\n\n"
	            "To re-run next time start with\n"
	            "'Control-Left' key pressed.",
	            _cfg->pathName().c_str() );
	fl_message_title_default( 0 );
	return cmd;
}

static void cleanup()
//-------------------------------------------------------------------------------
{
	static bool recurs = false;
	if ( !recurs )
	{
		recurs = true;
		Audio::instance()->shutdown();
		restoreScreenResolution();
	}
}

static void signalHandler( int sig_ )
//-------------------------------------------------------------------------------
{
	signal( sig_, SIG_DFL );
	if ( sig_ != SIGINT && sig_ != SIGTERM )
		PERR(  endl << "Sorry, FLTrator crashed (signal " << sig_ << ")." );
	cleanup();
	exit( EXIT_FAILURE );
}

static void installSignalHandler()
//-------------------------------------------------------------------------------
{
	static const int signals[] = { SIGABRT, SIGFPE, SIGILL, SIGINT, SIGSEGV, SIGTERM };
	for ( size_t i = 0; i < nbrOfItems( signals ); i++ )
		signal( signals[i], signalHandler );
}

#ifdef WIN32
#include "win32_console.H"
#endif
//-------------------------------------------------------------------------------
int main( int argc_, const char *argv_[] )
//-------------------------------------------------------------------------------
{
	atexit( cleanup );
#ifdef WIN32
	Console console;	// output goes to command window (if started from there)
#endif
	installSignalHandler();

	string cmd;
	for ( int i = 1; i < argc_; i++ )
		cmd += ( i > 1 ? (string)" " : (string)"" + argv_[i] );
	LOG( "cmd: '" << cmd << "'" );
	fl_register_images();
	Fl::scheme( "gtk+" );
	int seed = time( 0 );
	Random::pSrand( seed );

	FltWin fltrator( argc_, argv_ );
	return fltrator.run();
}
