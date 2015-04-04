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
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_GIF_Image.H>
#include <FL/Fl_Preferences.H>
#include <FL/filename.H>
#include <FL/fl_draw.H>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#if ( defined APPLE ) || ( defined __linux__ ) || ( defined __MING32__ )
#include <unistd.h>
#endif
#include <fstream>
#include <sstream>
#include <iostream>
#include <ctime>
#include <cassert>

#ifdef WIN32
#include <mmsystem.h>
#define random rand
#define srandom srand
#endif

// fallback Windows native
#ifndef R_OK
#define R_OK	4
#endif
#define _access access


// umcomment to draw a black line at rim of ground/sky?
//#define DRAW_LS_OUTLINE

static const unsigned MAX_LEVEL = 10;
//static const int MAX_LEVEL = 2;	// for testing end screen :-)
static const unsigned  MAX_LEVEL_REPEAT = 3;

static const unsigned SCREEN_W = 800;
static const unsigned SCREEN_H = 600;

#ifdef WIN32
static const unsigned FPS = 64;
#else
static const unsigned FPS = 200;
//static const unsigned FPS = 100;
#endif

static const double FRAMES = 1. / FPS;
static const unsigned DX = 200 / FPS;

static bool G_paused = false;


using namespace std;

//-------------------------------------------------------------------------------
enum ObjectType
//-------------------------------------------------------------------------------
{
	O_ROCKET = 1,
	O_DROP = 2,
	O_BADY = 4,
	O_CUMULUS = 8,

	// internal
	O_MISSILE = 1<<16,
	O_BOMB = 1<<17,
	O_SHIP = 1<<18
};

static string homeDir()
//-------------------------------------------------------------------------------
{
	char home_path[ FL_PATH_MAX ];
#ifdef WIN32
	fl_filename_expand( home_path, "$APPDATA/" );
#else
	fl_filename_expand( home_path, "$HOME/." );
#endif
	return (string)home_path + "fltrator/";
}

static string levelPath( const string& file_ )
//-------------------------------------------------------------------------------
{
	static const char levelDir[] = "levels/";
	if ( access( file_.c_str(), R_OK ) == 0 )
		return file_;
	string localPath( levelDir + file_ );
	if ( access( localPath.c_str(), R_OK ) == 0 )
		return localPath;
	return homeDir() + levelDir + file_;
}

static string wavPath( const string& file_ )
//-------------------------------------------------------------------------------
{
	static const char wavDir[] = "wav/";
	if ( access( file_.c_str(), R_OK ) == 0 )
		return file_;
	string localPath( wavDir + file_ );
	if ( access( localPath.c_str(), R_OK ) == 0 )
		return localPath;
	return homeDir() + wavDir + file_;
}

static string imgPath( const string& file_ )
//-------------------------------------------------------------------------------
{
	static const char imgDir[] = "images/";
	if ( access( file_.c_str(), R_OK ) == 0 )
		return file_;
	string localPath( imgDir + file_ );
	if ( access( localPath.c_str(), R_OK ) == 0 )
		return localPath;
	return homeDir() + imgDir + file_;
}

//-------------------------------------------------------------------------------
class Rect
//-------------------------------------------------------------------------------
{
public:
	Rect(int x_, int y_, int w_, int h_) :
		_x(x_),
		_y(y_),
		_w(w_),
		_h(h_)
	{
	}
	bool intersects( const Rect& r_ ) const
	{
		return within( _x, _y, r_ ) || within( r_.x(), r_.y(), *this );
	}
	bool contains( const Rect& r_ ) const
	{
		return within( r_.x(), r_.y(), *this ) &&
		       within( r_.x() + r_.w() - 1, r_.y() + r_.h() - 1, *this );
	}
	bool inside( const Rect& r_ ) const
	{
		return within( _x, _y, r_ ) &&
		       within( _x +_w - 1, _y + _h - 1, r_ );
	}
	int x() const { return _x; }
	int y() const { return _y; }
	int w() const { return _w; }
	int h() const { return _h; }
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

//-------------------------------------------------------------------------------
class PlaySound
//-------------------------------------------------------------------------------
{
public:
	static PlaySound *instance()
	{
		static PlaySound *inst = 0;
		if ( !inst )
			inst = new PlaySound();
		return inst;
	}
	bool play( const char *file_ )
	{
		int ret = 0;
		if ( !_disabled && file_ )
		{
			string cmd( _cmd );
			if ( cmd.size() )
			{
				bool bg( true );
				size_t pos = cmd.find( "%f" );
				if ( pos == string::npos )
				{
					pos = cmd.find( "%F" );
					if ( pos != string::npos )
						bg = false;
				}
				if ( pos == string::npos )
					cmd += ( ' ' + wavPath( file_ ) );
				else
				{
					cmd.erase( pos, 2 );
					cmd.insert( pos, wavPath( file_ ) );
					if ( bg )
						cmd += " &";
				}
			}
//			printf( "cmd: %s\n", cmd.c_str() );
#ifdef WIN32
//			::PlaySound( wavPath( file_ ).c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NOSTOP );
			if ( cmd.empty() )
				cmd = "playsound " + wavPath( file_ );
			STARTUPINFO si = { 0 };
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
			si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			si.hStdOutput =  GetStdHandle(STD_OUTPUT_HANDLE);
			si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
			si.wShowWindow = SW_HIDE;
			PROCESS_INFORMATION pi;
			ret = !CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
			CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
#elif __APPLE__
			if ( cmd.empty() )
				 cmd =  "play -q " + wavPath( file_ ) + " &";
//			printf( "cmd: %s\n", cmd.c_str() );
			ret = system( cmd.c_str() );
#else
			// linux
			if ( cmd.empty() )
				cmd = "aplay -q -N " + wavPath( file_ ) + " &";
//			printf( "cmd: %s\n", cmd.c_str() );
			ret = system( cmd.c_str() );
#endif
		}
		return !_disabled && !ret;
	}
	bool disabled() const { return _disabled; }
	void disable( bool disable_ = true )
	{
		_disabled = disable_;
	}
	void cmd( const string& cmd_ ) { _cmd = cmd_; }
private:
	PlaySound() :
		_disabled( false ) {}
private:
	bool _disabled;
	string _cmd;
};

//-------------------------------------------------------------------------------
class Object
//-------------------------------------------------------------------------------
{
public:
	static void cb_object( void *d_ )
	{
		((Object *)d_)->update();
		Fl::repeat_timeout( ((Object *)d_)->_timeout, cb_object, d_ );
	}
	Object( ObjectType o_, int x_, int y_, const char *image_ = 0, int w_ = 0, int h_ = 0 ) :
		_o( o_ ),
		_x( x_ ),
		_y( y_ ),
		_w( w_ ),
		_h( h_ ),
		_speed( 1 ),
		_state(0),
		_nostart( false ),
		_timeout( 0.05 ),
		_image( image_ ? Fl_Shared_Image::get( imgPath( image_ ).c_str() ) : 0  )
	{
		if ( image_ )
		{
			assert( _image && _image->count() );
			_w = _image->w();
			_h = _image->h();
		}
	}
	void image( const char *image_ )
	{
		assert( image_ );
		Fl_Shared_Image *image = Fl_Shared_Image::get( imgPath( image_ ).c_str() );
		if ( image && image->count() )
		{
			if ( _image )
				_image->release();
			_image = image;
			_w = _image->w();
			_h = _image->h();
		}
	}
	bool isTransparent( size_t x_, size_t y_ ) const
	{
		// valid only for pixmaps!
		return _image->data()[y_ + 2][x_] == ' ';
	}
	bool started() const { return _state > 0; }
	virtual double timeout() const { return _timeout; }
	void timeout( double timeout_ ) { _timeout = timeout_; }
	virtual const char* start_sound() const { return 0; }
	virtual const char* image_started() const { return 0; }
	void start( size_t speed_ = 1 )
	{
		const char *img = image_started();
		if ( img )
			image( img );
		_speed = speed_;
		Fl::add_timeout( timeout(), cb_object, this );
		PlaySound::instance()->play( start_sound() );
		_state = 1;
	}
	virtual void update()
	{
		_state++;
	}
	virtual void draw()
	{
		_image->draw( x(), y() );
#ifdef DEBUG
		fl_color(FL_YELLOW);
		Rect r(rect());
		fl_rect(r.x(),r.y(),r.w(),r.h());
		fl_point( cx(), cy() );
#endif
	}
	void draw_collision() const
	{
		int pts = w() * h() / 20;
		for ( int i = 0; i < pts; i++ )
		{
			int X = random() % w();
			int Y = random() % h();
			if ( !isTransparent( X, Y ) )
			{
				fl_rectf( x() + X, y() + Y, 4, 4,
					( random() % 2 ? FL_RED : FL_YELLOW ) );
			}
		}
	}
	virtual ~Object()
	{
		Fl::remove_timeout( cb_object, this );
		if ( _image )
			_image->release();
	}
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
protected:
	ObjectType _o;
	int _x, _y;
	int _w, _h;
	unsigned _speed;
	unsigned _state;
private:
	bool _nostart;
	double _timeout;
	Fl_Shared_Image *_image;
};

//-------------------------------------------------------------------------------
class Rocket : public Object
//-------------------------------------------------------------------------------
{
typedef Object Inherited;
public:
	Rocket( int x_= 0, int y_ = 0 ) :
		Inherited( O_ROCKET, x_, y_, "rocket.gif" ),
		_exploding( false ),
		_exploded( false )
	{
	}
	virtual ~Rocket()
	{
		Fl::remove_timeout( cb_explosion_end, this );
	}
	bool lifted() const { return started(); }
	virtual const char *start_sound() const { return "launch.wav"; }
	virtual const char* image_started() const { return "rocket_launched.gif"; }
	void draw()
	{
		if ( !_exploded )
			Inherited::draw();
		 if ( _exploding )
			draw_collision();
	}
	void update()
	{
		if ( G_paused ) return;
		if ( !_exploding && !_exploded )
		{
			size_t delta = ( 1 + _state / 10 ) * _speed;
			if ( delta > 12 )
				delta = 12;
			_y -= delta;
		}
		Inherited::update();
	}
	void onExplosionEnd()
	{
//		_exploding = false;
		_exploded = true;
	}
	static void cb_explosion_end( void *d_ )
	{
		((Rocket *)d_)->onExplosionEnd();
	}
	void crash()
	{
		_explode( 0.2 );
	}
	void explode()
	{
		_explode( 0.05 );
	}
	bool exploding() const { return _exploding; }
	bool exploded() const { return _exploded; }
private:
	void _explode( double to_ )
	{
		if ( !_exploding && !_exploded )
		{
			_exploding = true;
			Fl::add_timeout( to_, cb_explosion_end, this );
		}
	}
private:
	bool _exploding;
	bool _exploded;
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
	virtual const char *start_sound() const { return "drop.wav"; }
	void hit()
	{
		//	TODO: other image?
	}
	void update()
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
		_up( false ),	// default zuerst nach unten
		_hits( 0 )
	{
	}
	bool moving() const { return started(); }
	virtual const char *start_sound() const { return "bady.wav"; }
	void hit()
	{
		 _hits++;
		if ( badlyhit() )
		{
			image( "bady_hit.gif" );
		}
	}
	int hits() const { return _hits; }
	bool badlyhit() const { return _hits >= 4; }
	void turn() { _up = !_up; }
	bool turned() const { return _up; }
	void update()
	{
		if ( G_paused ) return;
		size_t delta = 2;
		if ( delta > 12 )
			delta = 12;
		_y = _up ? _y - delta : _y + delta;
		Inherited::update();
	}
private:
	bool _up;
	int _hits;
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
	void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		size_t delta = 2;
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
		Object( O_MISSILE, x_, y_, 0, 20, 3 )
	{
		update();
	}
	virtual const char *start_sound() const { return "missile.wav"; }
	void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		_x += 15;
	}
	virtual void draw()
	{
		fl_rectf( x(), y(), w(), h(), FL_WHITE );
#ifdef DEBUG
		fl_color(FL_YELLOW);
		Rect r(rect());
		fl_rect(r.x(),r.y(),r.w(),r.h());
#endif
	}
};

//-------------------------------------------------------------------------------
class Bomb : public Object
//-------------------------------------------------------------------------------
{
typedef Object Inherited;
public:
	Bomb( int x_, int y_ ) :
		Inherited( O_BOMB, x_, y_, 0, 20, 20 ),
		_dy( 10 )
	{
		update();
	}
	virtual const char *start_sound() const { return "bomb_f.wav"; }
	void update()
	{
		if ( G_paused ) return;
//		printf( "speed: %u\n", speed() );
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
	void draw()
	{
//		fl_color( fl_rgb_color(0x38,0xa8,0xa9) );
		fl_color( fl_rgb_color(0x56,0x56,0x56) );
		fl_pie( x(), y(), w(), h(), 0, 360);
		fl_color( FL_BLACK );
		fl_arc( x(), y(), w(), h(), 0, 360);
#ifdef DEBUG
		fl_color(FL_YELLOW);
		Rect r(rect());
		fl_rect(r.x(),r.y(),r.w(),r.h());
#endif
	}
	virtual double timeout() const { return started() ? 0.05 : 0.1; }
private:
	int _dy;
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
	void object( unsigned int object_ ) { _object = object_; }
	unsigned int object()  { return _object; }
	bool has_object( int type_ ) const { return _object & type_; }
	void has_object( int type_, bool has_ )
	{
		if ( has_ )
			_object |= type_;
		else
			_object &= ~type_;
	}
private:
	int _ground_level;
	int _sky_level;
	unsigned int _object;
};

//-------------------------------------------------------------------------------
class Terrain : public vector<TerrainPoint>
//-------------------------------------------------------------------------------
{
};

//-------------------------------------------------------------------------------
class Spaceship : public Object
//-------------------------------------------------------------------------------
{
typedef Object Inherited;
public:
	Spaceship( int x_, int y_, int W_, int H_ ) :
		Inherited( O_SHIP, x_, y_, "spaceship.gif" ),
		_W( W_ ),
		_H( H_ )
	{
	}
	void left()
	{
		if ( _x > 0 )
			_x -= DX;
	}
	void right()
	{
		if ( _x < _W / 2 )
			_x += DX;
	}
	void up()
	{
		if ( _y > _h / 2 )
			_y -= DX;
	}
	void down()
	{
		if ( _y < _H )
			_y += DX;
	}
private:
	int _W, _H;
};

//-------------------------------------------------------------------------------
class FltWin : public Fl_Double_Window
//-------------------------------------------------------------------------------
{
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
private:
	void add_score( unsigned score_ );
	void position_spaceship();
	void addScrolloutZone();
	void create_terrain();
	void draw_objects( bool pre_ );
	void draw_missiles();
	void draw_bombs();
	void draw_rockets();
	void draw_drops();
	void draw_badies();
	void draw_cumuluses();
	void changeState( State toState_ = NEXT_STATE );
	void check_hits();
	void check_missile_hits();
	void check_bomb_hits();
	void check_rocket_hits();
	void check_drop_hits();
	bool loadLevel( int level_, const string levelFileName_ = "" );
	void update_missiles();
	void update_bombs();
	bool collision( Object& o_, int w_, int h_ );
	void create_objects();
	void update_objects();
	void update_drops();
	void update_badies();
	void update_cumuluses();
	void update_rockets();
	int drawText( int x_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const;
	void draw_score();
	void draw_scores();
	void draw_title();
	void draw();
	int handle( int e_ );
	void onActionKey();
	void onDemo();
	void onLostFocus();
	void onGotFocus();
	void onNextScreen( bool fromBegin_ =  false );
	void onTitleScreen();
	void onStateChange( State from_state_ );
	void onUpdate();
	bool paused() const { return _state == PAUSED; }
	void bombUnlock();
	static void cb_bomb_unlock( void *d_ );
	static void cb_demo( void *d_ );
	static void cb_paused( void *d_ );
	static void cb_update( void *d_ );
protected:
	Terrain T;
	Fl_Color BG_COLOR;
	Fl_Color LS_COLOR;
	Fl_Color SKY_COLOR;
	vector<Missile *> Missiles;
	vector<Bomb *> Bombs;
	vector<Rocket *> Rockets;
	vector<Drop *> Drops;
	vector<Bady *> Badies;
	vector<Cumulus *> Cumuluses;
private:
	State _state;
	int _xoff;
	int _ship_delay;
	int _ship_voff;
	int _ship_y;
	bool _left, _right, _up, _down;
	Spaceship *_spaceship;
	Rocket _rocket;
	Drop _drop;
	Bady _bady;
	Cumulus _cumulus;
	bool _bomb_lock;
	bool _collision;	// level ended with collision
	bool _done;			// level ended by finishing
	unsigned _frame;
	unsigned _hiscore;
	unsigned _old_hiscore;
	unsigned _score;
	unsigned _first_level;
	unsigned _level;
	unsigned _level_repeat;
	bool _cheatMode;
	bool _levelMode;	// started with level specification
	string _levelFile;
	string _input;
	string _username;
	Fl_Preferences *_cfg;
	unsigned _speed_right;
	bool _internal_levels;
	bool _enable_boss_key; // ESC
	bool _focus_out;
	bool _no_random;
};

//-------------------------------------------------------------------------------
// class FltWin : public Fl_Double_Window
//-------------------------------------------------------------------------------
FltWin::FltWin( int argc_/* = 0*/, const char *argv_[]/* = 0*/ ) :
	Inherited( SCREEN_W, SCREEN_H, "FL-Trator v1.0" ),
	BG_COLOR( FL_BLUE ),
	LS_COLOR( FL_GREEN ),
	SKY_COLOR( FL_DARK_GREEN ),
	_state( START ),
	_xoff( 0 ),
	_left( false), _right( false ), _up( false ), _down( false ),
	_spaceship( new Spaceship( w() / 2, 40, w(), h() ) ),
	_bomb_lock( false),
	_collision( false ),
	_done(false),
	_frame( 0 ),
	_hiscore(0),
	_old_hiscore( 0 ),
	_score(0),
	_first_level(1),
	_level(0),
	_level_repeat(0),
	_cheatMode( false ),
	_levelMode( false ),
	_cfg( 0 ),
	_speed_right( 0 ),
	_internal_levels( false ),
	_enable_boss_key( false ),
	_focus_out( true ),
	_no_random( false )
{
	int argc( argc_ );
	unsigned argi( 1 );
	while ( argc-- > 1 )
	{
		string arg( argv_[argi++] );
		if ( "--help" == arg )
		{
			printf( "Usage:\n");
			printf( "  %s [level] [levelfile] [options]\n\n", argv_[0] );
			printf( "\tlevel      [1, 10] startlevel\n");
			printf( "\tlevelfile  name of levelfile (for testing a new level)\n");
			printf("Options:\n");
			printf("  -e\tenable Esc as boss key\n" );
			printf("  -i\tplay internal (auto-generated) levels\n" );
			printf("  -p\tdisable pause on focus out\n" );
			printf("  -r\tdisable ramdomness\n" );
			printf("  -s\tstart with sound disabled\n" );
			printf("  -A\"playcmd\"\tspecify audio play command\n" );
			printf("   \te.g. -A\"play -q %%f\"\n" );
			exit(0);
		}

		int level = atoi( arg.c_str() );
		if ( level > 0 && level <= (int)MAX_LEVEL )
		{
			_first_level = level;
			_levelMode = true;
		}
		else if ( arg.find( "-A" ) == 0 )
		{
			PlaySound::instance()->cmd( arg.substr( 2 ) );
		}
		else if ( arg[0] == '-' )
		{
			for ( size_t i = 0; i < arg.size(); i++ )
			{
				switch ( arg[i] )
				{
					case 'e':
						_enable_boss_key = true;
						break;
					case 's':
						PlaySound::instance()->disable();
						break;
					case 'i':
						_internal_levels = true;
						break;
					case 'p':
						_focus_out = false;
						break;
					case 'r':
						_no_random = true;
						break;
				}
			}
		}
		else
		{
			_levelFile = arg;
			printf( "level file: '%s'\n", _levelFile.c_str() );
		}
	}
	if ( !_levelMode )
		_levelFile.erase();

	// set spaceship image as icon
	Fl_RGB_Image icon( (Fl_Pixmap *)_spaceship->image() );
	Fl_Window::default_icon( &icon );

	_level = _first_level;
	_cfg = new Fl_Preferences( Fl_Preferences::USER, "CG", "fltrator" );
	create_terrain();
	position_spaceship();
	_hiscore = _old_hiscore;	// set from previous game!
	Fl::add_timeout( FRAMES, cb_update, this );

	// try to center on current screen
	int X, Y, W, H;
	Fl::screen_xywh( X, Y, W, H );
	position( ( W - w() ) / 2, ( H - h() ) / 2 );

	changeState();
	show();
}

void FltWin::onStateChange( State from_state_ )
//-------------------------------------------------------------------------------
{
	switch ( _state )
	{
		case TITLE:
			onTitleScreen();
			break;
		case SCORE:
			Fl::remove_timeout( cb_demo, this );
			break;
		case LEVEL:
			onNextScreen( from_state_ != PAUSED );
			break;
		case DEMO:
			G_paused = false;
			onDemo();
			break;
		case LEVEL_FAIL:
			PlaySound::instance()->play( "ship_x.wav" );
		case LEVEL_DONE:
		{
			if ( _score > _hiscore )
				_hiscore = _score;
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

void FltWin::changeState( State toState_/* = NEXT_STATE*/ )
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
				G_paused = false;
				_state = _levelMode ? LEVEL : TITLE;
				break;

			case PAUSED:
				if ( G_paused )
				{
					Fl::add_timeout( 0.5, cb_paused, this );
					return;
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
		if ( _state != toState_ )
			_state = toState_;
	}
	if ( _state != state )
		onStateChange( state );
}

void FltWin::add_score( unsigned score_ )
//-------------------------------------------------------------------------------
{
	if ( _state != DEMO )
		_score += score_;
}

void FltWin::position_spaceship()
//-------------------------------------------------------------------------------
{
	int X = _spaceship->w() + 10;
	int ground = T[X].ground_level();
	int sky = T[X].sky_level();
	int Y = ground + ( h() - ground - sky ) / 2;
//	printf("position_spaceship(): X=%d Y=%d h()-Y =%d sky=%d\n", X, Y, h() - Y, sky );
	_spaceship->cx( X );
	_spaceship->cy( h() - Y );
}

void FltWin::addScrolloutZone()
//-------------------------------------------------------------------------------
{
	T.push_back( T.back() );
	T.back().object( 0 ); // ensure there are no objects in scrollout zone!
	for ( int i = 0; i < w() / 2 - 1; i++ )
		T.push_back( T.back() );
}

bool FltWin::collision( Object& o_, int w_, int h_ )
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

	uchar *screen = fl_read_image( 0, X, Y, W, H );

	int d = 3;
	unsigned char r, g, b;
	bool collided = false;

	// background color r/g/b
	unsigned char  R, G, B;
	Fl::get_color( BG_COLOR, R, G, B );
//	printf("check for BG-Color %X/%X/%X\n", R, G, B );

#ifdef DEBUG
	int xc = 0;
	int yc = 0;
#endif
	for ( int y = 0; y < H; y++ )
	{
		long index = y * W * d; // + (x * d));    // X/Y -> buf index
		for ( int x = 0; x < W; x++ )
		{
			r = *(screen + index + 0);
			g = *(screen + index + 1);
			b = *(screen + index + 2);

			if ( !o_.isTransparent(x + xoff, y + yoff) && !(r == R && g == G && b == B) )
			{
				collided = true;
#ifdef DEBUG
				xc = x;
				yc = y;
#endif
				break;
			}
			if ( collided ) break;
			index += d;
		}
	}
	delete[] screen;
	if ( collided )
	{
#ifdef DEBUG
		printf("collision at %d + %d / %d + %d !\n", o_.x(), xc, o_.y(), yc );
		printf("object %dx%d xoff=%d yoff=%d\n", o_.w(), o_.h(), xoff, yoff);
		printf("X=%d, Y=%d, W=%d, H=%d\n", X, Y, W, H);
#endif
	}
	return collided;
} // collision

void FltWin::create_terrain()
//-------------------------------------------------------------------------------
{
	assert( _level > 0 && _level <= MAX_LEVEL );

	// set level randomness
	if ( _no_random )
		srandom( _level * 10000 );	// always the same sequences/objects
	else
		srandom( time( 0 ) );	// random sequences/objects

	_cheatMode = getenv( "FLTRATOR_CHEATMODE" );

	int hs;
	_cfg->get( "hiscore", hs, 0 );
	_old_hiscore = hs;
//	printf( "old hiscore = %u\n", _old_hiscore );
	char name[11];
	_cfg->get( "name", name, "", sizeof(name) - 1 );
	_username = name;

	T.clear();
	if ( ( !_internal_levels || _levelFile.size() )
	       && loadLevel( _level, _levelFile ) )	// try to load level from landscape file
	{
		assert( (int)T.size() >= w() );
		addScrolloutZone();
		return;
	}

	bool sky = ( _level % 2 || _level >= MAX_LEVEL - 1 ) && _level != 1;

//	printf("create terrain(): _level: %u _level_repeat: %u, sky = %d\n",
//		_level, _level_repeat, sky );
	switch ( _level % 4 )
	{
		case 0:
			BG_COLOR = FL_YELLOW;
			LS_COLOR = fl_rgb_color(0x61,0x2f,0x04);	// brownish
			break;
		case 1:
			BG_COLOR = FL_BLUE;
			LS_COLOR = FL_GREEN;
			break;
		case 2:
			BG_COLOR = FL_GRAY;
			LS_COLOR = FL_RED;
			break;
		case 3:
			BG_COLOR = FL_CYAN;
			LS_COLOR = FL_DARK_GREEN;
			break;
	}
	if ( sky )
	{
		Fl_Color temp = BG_COLOR;
		BG_COLOR = LS_COLOR;
		LS_COLOR = temp;
		LS_COLOR = fl_darker( LS_COLOR );
		BG_COLOR = fl_lighter( BG_COLOR );
	}
	else if ( _level > 5 )
	{
		BG_COLOR = fl_darker( BG_COLOR );
		LS_COLOR = fl_darker( LS_COLOR );
	}
	SKY_COLOR = LS_COLOR;

	int range = (h() * (sky ? 2 : 3)) / 5;
	int bott = h() / 5;
	for ( size_t i = 0; i < 15; i++ )
	{
		int r = i ? range : range / 2;
		int peak = random() % r  + 20;
		int j;
		for ( j = 0; j < peak; j++ )
			T.push_back( TerrainPoint( j + bott ) );
		int eben = random() % ( w() / 2 );
		if ( random() % 3 == 0 )
			eben = random() % 10;
		for ( j = 0; j < eben; j++ )
			T.push_back( T.back() );
		for ( j = peak; j >= 0; j-- )
			T.push_back( TerrainPoint( j + bott ) );
		/*int */eben = random() % ( w() / 2 );
		for ( j = 0; j < eben; j++ )
			T.push_back( T.back() );
	}

	if ( sky )
	{
		// sky
		range = h() / 5;
		int top =  h() / 12;
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
//				printf("sky: %d free: %d ground: %d top: %d:\n", sky, free, ground, top);
				assert( sky  >= -1 && sky < h() );
			}
			T[i].sky_level( sky );
		}
	}

	// setup object positions
	int min_dist = 150 - _level * 5;
	int max_dist = 300 - _level * 5;
	if ( min_dist < _rocket.w() )
		min_dist = _rocket.w();
	if ( max_dist < _rocket.w() * 3 )
		max_dist = _rocket.w() * 3;

	int last_x = random() % max_dist + ( _level * min_dist * 2 ) / 3;	// first rocket farther away!
	while ( last_x < (int)T.size() )
	{
		if ( h() - T[last_x].ground_level() > ( 100 - (int)_level * 10 ) /*&& random() % 2*/ )
		{
			switch ( random() % _level ) // 0-9
			{
				case 0:
				case 1:
				case 2:
				case 3:
					T[last_x].object( O_ROCKET );
					break;
				case 4:
				case 5:
				case 6:
					if ( sky )
						T[last_x].object( random()%2 ? O_DROP : O_BADY );
					else
						T[last_x].object( O_BADY );
					break;
				case 7:
				case 8:
					T[last_x].object( O_BADY );
					break;
				case 9:
					if ( random() % 4 == 0 )
						T[last_x].object( O_CUMULUS );
					break;
			}
		}
		last_x += random() % max_dist + min_dist;
	}
	addScrolloutZone();
}

void FltWin::draw_objects( bool pre_ )
//-------------------------------------------------------------------------------
{
	if ( pre_ )
	{
		draw_missiles();
		draw_bombs();
		draw_rockets();
		draw_drops();
		draw_badies();
	}
	else
	{
		draw_cumuluses();
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

void FltWin::draw_bombs()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Bombs.size(); i++ )
	{
		Bombs[i]->draw();
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

void FltWin::draw_drops()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Drops.size(); i++ )
	{
		Drops[i]->draw();
	}
}

void FltWin::draw_badies()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Badies.size(); i++ )
	{
		Badies[i]->draw();
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

void FltWin::check_hits()
//-------------------------------------------------------------------------------
{
	check_missile_hits();
	check_bomb_hits();
	check_rocket_hits();
	check_drop_hits();
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
			if ( !((*r)->exploding()) &&
			     ((*m)->rect().intersects( (*r)->rect() ) ||
			     (*m)->rect().inside( (*r)->rect())) )
			{
				// rocket hit by missile
				PlaySound::instance()->play( "missile_hit.wav" );
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

		vector<Bady *>::iterator b = Badies.begin();
		for ( ; b != Badies.end(); )
		{
			if ( (*m)->rect().inside( (*b)->rect()) )
			{
				// bady hit by missile
				(*b)->hit();
				if ( (*b)->hits() >= 5 )
				{
					PlaySound::instance()->play( "bady_hit.wav" );
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
				PlaySound::instance()->play( "drop_hit.wav" );
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

void FltWin::check_bomb_hits()
//-------------------------------------------------------------------------------
{
	vector <Bomb *>::iterator b = Bombs.begin();
	for ( ; b != Bombs.end(); )
	{
		vector<Rocket *>::iterator r = Rockets.begin();
		for ( ; r != Rockets.end(); )
		{
			if ( !((*r)->exploding()) &&
			     ((*b)->rect().intersects( (*r)->rect() ) ||
			     (*b)->rect().inside( (*r)->rect())) )
			{
				// rocket hit by bomb
				PlaySound::instance()->play( "bomb_x.wav" );
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

		++b;
	}
}

void FltWin::check_rocket_hits()
//-------------------------------------------------------------------------------
{
	vector<Rocket *>::iterator r = Rockets.begin();
	for ( ; r != Rockets.end(); )
	{
		if ( (*r)->lifted() && (*r)->rect().inside( _spaceship->rect() ) )
		{
			// rocket hit by spaceship
//			delete *r;
//			r = Rockets.erase(r);
			(*r)->crash();
		}
//		else
			++r;
	}
}

void FltWin::check_drop_hits()
//-------------------------------------------------------------------------------
{
	vector<Drop *>::iterator d = Drops.begin();
	for ( ; d != Drops.end(); )
	{
		if ( (*d)->dropped() && (*d)->rect().inside( _spaceship->rect() ) )
		{
			// drop 'hit' by spaceship
//			printf("drop fully hit spaceship\n");
			redraw();
			Fl::wait();	// Hack: wait for collision detection, otherwise there will be none!
			delete *d;
			d = Drops.erase(d);
			continue;
		}
		else if ( (*d)->dropped() && (*d)->rect().intersects( _spaceship->rect() ) )
		{
			// drop touches spaceship
//			printf("drop only partially hit spaceship\n");
			if ( (*d)->x() > _spaceship->x() )
				(*d)->x( (*d)->x() + 10 );
			else
				(*d)->x( (*d)->x() - 10 );
			++d;
		}
		else
			++d;
	}
}

void FltWin::update_missiles()
//-------------------------------------------------------------------------------
{
//	printf( "#%u missiles\n", Missiles.size());
	for ( size_t i = 0; i < Missiles.size(); i++ )
	{
		bool gone = Missiles[i]->x() > w() ||
		            Missiles[i]->y() > h() - T[_xoff + Missiles[i]->x() + Missiles[i]->w()].ground_level();
		if ( gone )
			{
//			printf( " gone one missile from #%d\n", Missiles.size());
			Missile *m = Missiles[i];
			Missiles.erase( Missiles.begin() + i );
			delete m;
			i--;
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
//			printf( " gone one bomb from #%d\n", Missiles.size());
			Bomb *b = Bombs[i];
			Bombs.erase( Bombs.begin() + i );
			delete b;
			i--;
		}
	}
}

void FltWin::create_objects()
//-------------------------------------------------------------------------------
{
	// Only consider scrolled part!
	// Plus 150 pixel (=half width of broadest object) in advance in order to
	// make appearance of new objects smooth.
	// (Note: we can safely access + 150 pixel because of addScrolloutZone()).
	for ( int i = (_xoff == 0 ? 0 : w() - DX); i < w() + 150 ; i++ )
	{
		unsigned int o = T[_xoff + i].object();
		if ( o & O_ROCKET && i - _rocket.w() / 2 < w() )
		{
			Rocket *r = new Rocket( i, h() - T[_xoff + i].ground_level() - _rocket.h() / 2 );
			Rockets.push_back( r );
			o &= ~O_ROCKET;
#ifdef DEBUG
			printf("#rockets: %lu\n", (unsigned long)Rockets.size() );
#endif
		}
		if ( o & O_DROP && i - _drop.w() / 2 < w() )
		{
			Drop *d = new Drop( i, T[_xoff + i].sky_level() + _drop.h() / 2 );
			Drops.push_back( d );
			o &= ~O_DROP;
#ifdef DEBUG
			printf("#drops: %lu\n", (unsigned long)Drops.size() );
#endif
		}

		if ( o & O_BADY && i - _bady.w() / 2 < w() )
		{
			bool turn( random() % 3 == 0 );
			int X = turn ?
			      h() - T[_xoff + i].ground_level() - _bady.h() / 2 :
			      T[_xoff + i].sky_level() + _bady.h() / 2;
			Bady *b = new Bady( i, X );
			o &= ~O_BADY;
			if ( turn )
				b->turn();
			Badies.push_back( b );
#ifdef DEBUG
			printf("#badies: %lu\n", (unsigned long)Badies.size() );
#endif
		}

		if ( o & O_CUMULUS && i - _cumulus.w() / 2 < w() )
		{
			bool turn( random() % 3 == 0 );
			int X = turn ?
			      h() - T[_xoff + i].ground_level() - _cumulus.h() / 2 :
			      T[_xoff + i].sky_level() + _cumulus.h() / 2;
			Cumulus *c = new Cumulus( i, X );
			o &= ~O_CUMULUS;
			if ( turn )
				c->turn();
			Cumuluses.push_back( c );
#ifdef DEBUG
			printf("#cumuluses: %lu\n", (unsigned long)Cumuluses.size() );
#endif
		}
		T[_xoff + i].object( o );
	}
}

bool FltWin::loadLevel( int level_, const string levelFileName_/* = ""*/ )
//-------------------------------------------------------------------------------
{
	string levelFileName( levelFileName_ );
	if ( levelFileName_.empty() )
	{
		ostringstream os;
		os << "L_" << level_ << ".txt";
		levelFileName = levelPath( os.str() );
	}
	ifstream f( levelFileName.c_str() );
	if ( !f.is_open() )
		return false;
//	printf( "reading level %d from %s\n", level_, levelFileName.c_str() );
	unsigned long flags;
	f >> flags;
	f >> flags;
	f >> BG_COLOR;
	f >> LS_COLOR;
	f >> SKY_COLOR;

	while ( f.good() )	// access data is ignored!
	{
		int s, g;
		int obj;
		f >> s;
		f >> g;
		f >> obj;
		if ( f.good() )
			T.push_back( TerrainPoint( g, s, obj ) );
	}
	return true;
}

void FltWin::update_objects()
//-------------------------------------------------------------------------------
{
	update_missiles();
	update_bombs();
	update_rockets();
	update_drops();
	update_badies();
	update_cumuluses();
}

void FltWin::update_drops()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Drops.size(); i++ )
	{
		if ( !paused() )
			Drops[i]->x( Drops[i]->x() - DX);
		int bottom = h() - T[_xoff + Drops[i]->x()].ground_level();
		if ( 0 == bottom  ) bottom += Drops[i]->h();	// glide out completely if no ground
		bool gone = Drops[i]->y() + Drops[i]->h() / 2 > bottom || Drops[i]->x() < -Drops[i]->w();
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one drop from #%lu\n", (unsigned long)Drops.size());
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
			int start_dist = 400 - _level * 20 - random() % 200;
			if ( start_dist < 50 )
			start_dist = 50;
			int dist = Drops[i]->x() - _spaceship->x(); // center distance S<->R
			if ( dist <= 0 || dist >= start_dist ) continue;
//			printf("start drop #%ld, dist=%d \n", i, dist);
			// really start drop?
			int each_n = MAX_LEVEL - _level + 1; // L1: 10 ... L10: 1
			if ( _level < 5 )
				each_n /= 3;
			if ( each_n <= 0 )
				each_n = 1;
			int rd = random() % each_n; // L1: 0..9  L10: 0
//			printf("each_n = %d, rd = %d\n", each_n, rd );
			if ( rd == 0 )
			{
				Drops[i]->start( 4 +_level / 2 );
				assert( Drops[i]->dropped() );
				continue;
			}
			Drops[i]->nostart( true );
		}
	}
}

void FltWin::update_badies()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Badies.size(); i++ )
	{
		if ( !paused() )
			Badies[i]->x( Badies[i]->x() - DX);

		int top = T[_xoff + Badies[i]->x() + Badies[i]->w() / 2].sky_level();
		int bottom = h() - T[_xoff + Badies[i]->x() + Badies[i]->w() / 2].ground_level();
		bool gone = Badies[i]->x() < -Badies[i]->w();
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one bady from #%lu\n", (unsigned long)Badies.size());
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
				Badies[i]->start( _level );
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

void FltWin::update_cumuluses()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Cumuluses.size(); i++ )
	{
		if ( !paused() )
			Cumuluses[i]->x( Cumuluses[i]->x() - DX);

		int top = T[_xoff + Cumuluses[i]->x() + Cumuluses[i]->w() / 2].sky_level();
		int bottom = h() - T[_xoff + Cumuluses[i]->x() + Cumuluses[i]->w() / 2].ground_level();
		bool gone = Cumuluses[i]->x() < -Cumuluses[i]->w();
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one cumulus from #%lu\n", (unsigned long)Cumuluses.size());
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
				Cumuluses[i]->start( _level );
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

void FltWin::update_rockets()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Rockets.size(); i++ )
	{
		if ( !paused() )
			Rockets[i]->x( Rockets[i]->x() - DX);
		int top = T[_xoff + Rockets[i]->x()].sky_level();
		if ( -1 == top  ) top -= Rockets[i]->h();	// glide out completely if no sky
		bool gone = Rockets[i]->exploded() || Rockets[i]->x() < -Rockets[i]->w();
		// NOTE: when there's no sky, explosion is not visible, but still occurs,
		//       so checking exploded() suffices as end state for all rockets.
		if ( gone )
		{
#ifdef DEBUG
			printf( " gone one rocket from #%lu\n", (unsigned long)Rockets.size());
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
			int start_dist = 400 - _level * 20 - random() % 200;
			if ( start_dist < 50 )
				start_dist = 50;
			int dist = Rockets[i]->x() - _spaceship->x(); // center distance S<->R
			if ( dist <= 0 || dist >= start_dist ) continue;
			assert( !Rockets[i]->lifted() );
//			printf("start rocket #%ld, dist=%d \n", i, dist);
			// really start rocket?
			int each_n = MAX_LEVEL - _level + 1; // L1: 10 ... L10: 1
			if ( _level < 5 )
				each_n /= 3;
			if ( each_n <= 0 )
				each_n = 1;
			int rd = random() % each_n; // L1: 0..9  L10: 0
//			printf("each_n = %d, rd = %d\n", each_n, rd );
			if ( rd == 0 )
			{
				Rockets[i]->start( 4 +_level / 2 );
				assert( Rockets[i]->lifted() );
				continue;
			}
			Rockets[i]->nostart( true );
		}
	}
}

int FltWin::drawText( int x_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const
//-------------------------------------------------------------------------------
{
	char buf[200];
	va_list argp;
	va_start( argp, c_ );
	vsnprintf( buf, sizeof(buf), text_, argp );
	va_end( argp );
	fl_font( FL_HELVETICA_BOLD_ITALIC, sz_ );
	Fl_Color cc = fl_contrast( FL_WHITE, c_ );
	fl_color( cc );
	int X = x_;
	if ( x_ < 0 )
	{
		// centered
		int dx, dy, W, H = 0;
		fl_text_extents( buf, dx, dy, W, H );
		X = ( w() - W ) / 2;
	}

	fl_draw( buf, strlen(buf), X + 2, y_ + 2 );
	fl_color( c_ );
	fl_draw( buf, strlen(buf), X, y_ );
	return X;
}

void FltWin::draw_score()
//-------------------------------------------------------------------------------
{
	if ( _state == DEMO )
	{
		drawText( -1, h() - 30, "*** DEMO - Hit space to cancel ***", 30, FL_WHITE );
		return;
	}

	fl_font( FL_HELVETICA, 30 );
	fl_color( FL_WHITE );

	char buf[50];
	snprintf( buf, sizeof(buf), "L %u/%u Score: %06u", _level,
		MAX_LEVEL_REPEAT - _level_repeat, _score );
	fl_draw( buf, strlen(buf), 20, h() - 30 );
	snprintf( buf, sizeof(buf), "Hiscore: %06u", _old_hiscore );
	fl_draw( buf, strlen(buf), w() - 280, h() - 30 );
	snprintf( buf, sizeof(buf), "%d %%", (int)(((float)_xoff + /*2.0 * */ w()) / (float)T.size() * 100.) );
	fl_draw( buf, strlen(buf), w() / 2 - 20 , h() - 30 );

	if ( G_paused )
	{
		drawText( -1, 50, "** PAUSED **", 50, FL_WHITE );
	}
	else if ( _done )
	{
		if ( _level == MAX_LEVEL && ( !_levelMode || _cheatMode ) )
		{
			drawText( -1, 60, "** MISSION COMPLETE! **", 50, FL_WHITE );
			drawText( -1, 150, "You bravely mastered all challenges", 30, FL_WHITE );
			drawText( -1, 190, "and deserve to be called a REAL HERO.", 30, FL_WHITE );
			drawText( -1, 230, "Well done! Take some rest now, or ...", 30, FL_WHITE );
			drawText( -1, 400, "** hit space to restart **", 40, FL_YELLOW );
			// fireworks...
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
			drawText( -1, 50, "LEVEL FINISHED!", 50, FL_WHITE );
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
	fl_draw_box( FL_FLAT_BOX, 0, 0, w(), h(), FL_BLACK );
	drawText( -1, 120, "CONGRATULATION!", 70, FL_RED );
	drawText( -1, 200, "You topped the hiscore:", 40, FL_RED );
	int x = drawText( -1, 300, "old hiscore .......... %05lu", 30, FL_WHITE, _old_hiscore );
	drawText( x, 340, "new hiscore ......... %05lu", 30, FL_WHITE, _hiscore );

	if ( _input.size() > 10 )
		_input.erase( 10 );
	string name( _input );
	_username = name;
	_cfg->set( "name", name.c_str() );
	int hs( _hiscore );
	_cfg->set( "hiscore", hs );
	string filler( 10 - name.size(), '_' );
	string temp( name + filler );
	string inpfield;
	for ( size_t i = 0; i < temp.size(); i++ )
	{
		inpfield.push_back( temp[i] );
		inpfield.push_back( ' ' );
	}

	drawText( -1, 420, "Enter your name:   %s", 32, FL_GREEN, inpfield.c_str() );
	drawText( -1, h() - 50, "** hit space to continue **", 40, FL_YELLOW );
}

void FltWin::draw_title()
//-------------------------------------------------------------------------------
{
	static Fl_Image *bgImage = 0;
	static int _flip = 0;

	if ( !G_paused )
	{
		// speccy loading stripes
		Fl_Color c[] = { FL_GREEN, FL_BLUE, FL_YELLOW };
		int ci = random() % 3;
		for ( int y = 0; y < h(); y += 4 )
		{
			fl_color( c[ci] );
			if ( y <= 20 || y >= h() - 20 )
			{
				fl_line ( 0, y , w(), y );
				fl_line ( 0, y+1 , w(), y+1 );
				fl_line ( 0, y+2 , w(), y+2 );
				fl_line ( 0, y+3 , w(), y+3 );
			}
			else
			{
				fl_line ( 0, y, 40, y ); fl_line ( w()-40, y, w(), y );
				fl_line ( 0, y+1, 40, y+1 ); fl_line ( w()-40, y+1, w(), y+1 );
				fl_line ( 0, y+2, 40, y+2 ); fl_line ( w()-40, y+2, w(), y+2 );
				fl_line ( 0, y+3, 40, y+3 ); fl_line ( w()-40, y+3, w(), y+3 );
			}
			ci++;
			ci %= 3;
		}
	}

	if ( _frame % (unsigned)(1.0/FRAMES) != 1 )	// don't hog the cpu
		return;

	if ( G_paused )
		fl_draw_box( FL_FLAT_BOX, 0, 0, w(), h(), fl_rgb_color( 0, 0, 0 ) );
	else
		fl_draw_box( FL_FLAT_BOX, 40, 20, w() - 80, h() - 40, fl_rgb_color( 32, 32, 32 ) );
	if ( _spaceship && _spaceship->image() && !bgImage )
	{
		Fl_Image::RGB_scaling( FL_RGB_SCALING_BILINEAR );
		Fl_RGB_Image *rgb = new Fl_RGB_Image( (Fl_Pixmap *)_spaceship->image() );
		bgImage = rgb->copy( w() - 120, h() - 80 );
		delete rgb;
	}
	if ( bgImage )
		bgImage->draw( 60, 40 );

	drawText( -1, 120, "FL-TRATOR", 80, FL_RED );

	if ( /*!_xoff &&*/ _hiscore && _flip++ % 6 >= 3 )
	{
		drawText( -1, 330, "Hiscore", 35, FL_GREEN );
		drawText( -1, 380, "%u .......... %s", 30, FL_WHITE, _hiscore, _username.c_str() );
	}
	else
	{
		int x = drawText( -1, 260, "q/a .......... up/down", 30, FL_WHITE );
		drawText( x, 300, "o/p .......... left/right", 30, FL_WHITE );
		drawText( x, 340, "p ............. fire missile", 30, FL_WHITE );
		drawText( x, 380, "space ...... drop bomb", 30, FL_WHITE );
		drawText( x, 420, "7-9 .......... hold game", 30, FL_WHITE );
		drawText( x, 460, "s ............. toggle sound", 30, FL_WHITE );
	}
	if ( G_paused )
		drawText( -1, h() - 50, "** PAUSED **", 40, FL_YELLOW );
	else
		drawText( -1, h() - 50, "** hit space to start **", 40, FL_YELLOW );
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

	// draw bg
	fl_draw_box( FL_FLAT_BOX, 0, 0, w(), h(), BG_COLOR );

	// draw landscacpe
	for ( int i = 0; i < w(); i++ )
	{
		fl_color( SKY_COLOR );
		fl_yxline( i, -1, T[_xoff + i].sky_level() );
			fl_color( LS_COLOR );
		fl_yxline( i, h() - T[_xoff + i].ground_level(), h() );

#ifdef DRAW_LS_OUTLINE
		fl_color( FL_BLACK );
		fl_point( i, T[_xoff + i].sky_level() );
		fl_point( i, h() - T[_xoff + i].ground_level() );
#endif
	}

	draw_score();

	draw_objects( true );	// objects for collision check

	if ( !paused() && !_done && _state != DEMO )
		_collision = collision( *_spaceship, w(), h() );

	if ( _collision && _cheatMode )
	{
		if ( _xoff % 20 == 0 )
			PlaySound::instance()->play( "boink.wav" );
		_collision = false;
	}

	if ( _state != DEMO )
	{
		if ( !paused() || _frame % (FPS / 2) < FPS / 4 || _collision )
			_spaceship->draw();
	}

	draw_objects( false );	// objects AFTER collision check (=without collision)

	if ( _collision )
		_spaceship->draw_collision();
}

void FltWin::onGotFocus()
//-------------------------------------------------------------------------------
{
	if ( !_focus_out )
		return;

//	printf(" onGotFocus()\n" );
	if ( _state == TITLE || _state == DEMO )
	{
		G_paused = false;
		_state = START;
		changeState( TITLE );
	}
}

void FltWin::onLostFocus()
//-------------------------------------------------------------------------------
{
	if ( !_focus_out )
		return;

//	printf(" onLostFocus()\n" );
	G_paused = true;
	if ( _state == TITLE || _state == DEMO )
	{
		_state = START;	// re-change also to TITLE
		changeState( TITLE );
	}
}

int FltWin::handle( int e_ )
//-------------------------------------------------------------------------------
{
	static bool ignore_space = false;
	static int repeated_right = -1;
	if ( FL_FOCUS == e_ )
	{
//		printf( "FL_FOCUS\n");
		onGotFocus();
		return Inherited::handle( e_ );
	}
	if ( FL_UNFOCUS == e_ )
	{
//		printf( "FL_UNFOCUS\n");
		onLostFocus();
		return Inherited::handle( e_ );
	}
	int c = Fl::event_key();
	if ( ( e_ == FL_KEYDOWN || e_ == FL_KEYUP ) && c == FL_Escape )
		// get rid of ecape key closing window (except in boss mode)
		return _enable_boss_key ? Inherited::handle( e_ ) : 1;

	if ( e_ == FL_KEYUP && ( 's' ==  c || 'S' == c ) )
			PlaySound::instance()->disable( !PlaySound::instance()->disabled() );

	if ( _state == TITLE || _state == SCORE || _state == DEMO || _done )
	{
		if ( e_ == FL_KEYUP && c >= '0' && c < 'z' )
		{
			_input += c;
		}
		if ( e_ == FL_KEYUP && c == FL_BackSpace && _input.size() )
		{
			_input.erase( _input.size() - 1 );
		}
		if ( e_ == FL_KEYDOWN && c == ' ' )
		{
			ignore_space = true;
			onActionKey();
		}
		return Inherited::handle( e_ );
	}
	if ( e_ == FL_KEYDOWN )
	{
		if ( G_paused )
		{
			G_paused = false;
		}
//		printf("keydown '%c'\n", c >= 32 ? c : '?');
		if ( 'O' == c || 'o' == c )
			_left = true;
		if ( 'P' == c || 'p' == c )
		{
			repeated_right++;
			_right = true;
		}
		if ( 'Q' == c || 'q' == c )
			_up = true;
		if ( 'A' == c || 'a' == c )
			_down = true;
		return 1;
	}
	if ( e_ == FL_KEYUP )
	{
//		printf("keyup '%c'\n", c >= 32 ? c : '?');
		if ( 'O' == c || 'o' == c )
			_left = false;
		if ( 'P' == c || 'p' == c )
		{
			_right = false;
			_speed_right = 0;
			if ( repeated_right <= 0 && Missiles.size() < 5 && !paused() )
			{
				Missile *m = new Missile( _spaceship->x() + _spaceship->w() + 20,
					_spaceship->y() + _spaceship->h() / 2 );
				Missiles.push_back( m );
				Missiles.back()->start();
			}
			repeated_right = -1;
		}
		if ( ' ' == c )
		{
			if ( ignore_space )
			{
				ignore_space = false;
				return 1;
			}
			if	( _bomb_lock || paused() )
				return 1;
			if ( Bombs.size() < 5 )
			{
				Bomb *b = new Bomb( _spaceship->x() + _spaceship->w() / 2,
					_spaceship->y() + _spaceship->h() );
				Bombs.push_back( b );
				Bombs.back()->start( _speed_right );
				_bomb_lock = true;
				Fl::add_timeout( 0.5, cb_bomb_unlock, this );
			}
		}
		if ( 'Q' == c || 'q' == c )
			_up = false;
		if ( 'A' == c || 'a' == c )
			_down = false;
		if ( '7' <= c && '9' >= c )
		{
			if ( !_done && !_collision )
			{
				G_paused = !G_paused;
			}
		}
		return 1;
	}
	return Inherited::handle( e_ );
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
	((FltWin *)d_)->onUpdate();
	Fl::repeat_timeout( FRAMES, cb_update, d_ );
}

void FltWin::onActionKey()
//-------------------------------------------------------------------------------
{
//	printf( "onActionKey\n");
	Fl::remove_timeout( cb_demo, this );
	Fl::remove_timeout( cb_paused, this );
	if ( _state == SCORE )
	{
		_cfg->flush();
		_old_hiscore = _hiscore;
		printf( "hiscore saved.\n");
	}

	changeState();
}

void FltWin::onDemo()
//-------------------------------------------------------------------------------
{
//	printf( "starting demo...\n");
	_state = DEMO;
	onNextScreen( true );
}

void FltWin::onNextScreen( bool fromBegin_/* =  false*/ )
//-------------------------------------------------------------------------------
{
	delete _spaceship;
	_spaceship = 0;

	_xoff = 0;
	_collision = false;
	for ( size_t i = 0; i < Rockets.size(); i++ )
		delete Rockets[i];
	Rockets.clear();
	for ( size_t i = 0; i < Drops.size(); i++ )
		delete Drops[i];
	Drops.clear();
	for ( size_t i = 0; i < Badies.size(); i++ )
		delete Badies[i];
	Badies.clear();
	for ( size_t i = 0; i < Cumuluses.size(); i++ )
		delete Cumuluses[i];
	Cumuluses.clear();

	bool show_scores = false;

	if ( fromBegin_ )
	{
		_level = _first_level;
		_level_repeat = 0;
		_score = 0;
	}
	else
	{
		show_scores = _hiscore > _old_hiscore;

		if ( _done || _state == DEMO )
		{
			_level++;
			_level_repeat = 0;

			if ( _level > MAX_LEVEL )
			{
				_level = _first_level; // restart with level 1
				_score = 0;
				onTitleScreen();
			}
		}
		else
		{
			_level_repeat++;
			if ( _level_repeat > MAX_LEVEL_REPEAT )
			{
				_level = _first_level;
				_level_repeat = 0;
				_score = 0;
				onTitleScreen();
			}
		}
	}

	_spaceship = new Spaceship( w() / 2, 40, w(), h() );	// before create_terrain()!

	create_terrain();
	position_spaceship();

	_done = false;
	G_paused = false;
	_left = _right = _up = _down = false;

	show_scores &= _level == 1 && _level_repeat == 0;
	if ( show_scores )
	{
		_input = _username;
		changeState( SCORE );
	}
}

void FltWin::onTitleScreen()
//-------------------------------------------------------------------------------
{
	if ( _levelMode )
		return;
	_state = TITLE;
	_frame = 0;
	Fl::remove_timeout( cb_demo, this );
	if ( !G_paused )
		Fl::add_timeout( 20.0, cb_demo, this );
}

void FltWin::onUpdate()
//-------------------------------------------------------------------------------
{
	const unsigned SCORE_STEP = (int)( 200. / (double)DX ) * DX;
	if ( _state == TITLE || _state == SCORE || G_paused )
	{
		redraw();
		return;
	}
	create_objects();

	update_objects();

	if ( _state != DEMO )
		check_hits();

	if ( paused() )
	{
		redraw();
		return;
	}
	if ( _xoff % SCORE_STEP == 0 )
		add_score( 1 );

	if ( _left && _xoff + w() + w() / 2 < (int)T.size() ) // no retreat at end!
		_spaceship->left();
	if ( _right )
	{
		int ox = _spaceship->x();
		_spaceship->right();
		if ( ox != _spaceship->x() )+
			_speed_right++;
	}
	if ( _up )
		_spaceship->up();
	if ( _down  )
		_spaceship->down();

	redraw();
	if ( _collision )
		changeState( LEVEL_FAIL );
	else if ( _done )
	{
		if ( _state == DEMO )	// TODO: handle this via state
		{
			onNextScreen();	// ... but for now, just skip pause in demo mode
			return;	// do not increment _xoff now!
		}
		else
			changeState( LEVEL_DONE );
	}
//	if ( _state != LEVEL && _state != DEMO )
//		return;

	_xoff += DX;
	if ( _xoff + /*2 * */w() >= (int)T.size() - 1 )
		_done = true;
//	printf( "_xoff = %u / %d  %d\n", _xoff, (int)T.size(), _done );
}

void FltWin::bombUnlock()
//-------------------------------------------------------------------------------
{
	_bomb_lock = false;
}

//-------------------------------------------------------------------------------
int main( int argc_, const char *argv_[] )
//-------------------------------------------------------------------------------
{
	fl_register_images();
	FltWin fltrator( argc_, argv_ );
	return Fl::run();
}
