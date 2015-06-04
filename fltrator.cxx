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
#else
#include <sys/time.h>
#endif

// fallback Windows native
#ifndef R_OK
#define R_OK	4
#endif
#define _access access

static const unsigned MAX_LEVEL = 10;
static const unsigned  MAX_LEVEL_REPEAT = 3;

static const unsigned SCREEN_W = 800;
static const unsigned SCREEN_H = 600;

#if HAVE_SLOW_CPU
#undef USE_FLTK_RUN
#define USE_FLTK_RUN 1
#define DEFAULT_FPS 40
#else
#define DEFAULT_FPS 200
#endif

#ifdef WIN32
#if USE_FLTK_RUN
static const unsigned FPS = 64;
#else
static const unsigned FPS = DEFAULT_FPS;
#endif
#else
static const unsigned FPS = DEFAULT_FPS;
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
	O_RADAR = 16,
	O_COLOR_CHANGE = 64,

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

static string mkPath( const string& dir_, const string& file_ )
//-------------------------------------------------------------------------------
{
	string dir( dir_ );
	if ( dir.size() && dir[ dir.size() - 1 ] != '/' )
		dir.push_back( '/' );
	if ( access( file_.c_str(), R_OK ) == 0 )
		return file_;
	string localPath( dir + file_ );
	if ( access( localPath.c_str(), R_OK ) == 0 )
		return localPath;
	return homeDir() + dir + file_;
}

static string levelPath( const string& file_ )
//-------------------------------------------------------------------------------
{
	return mkPath( "levels", file_ );
}

static string wavPath( const string& file_ )
//-------------------------------------------------------------------------------
{
	return mkPath( "wav", file_ );
}

static string imgPath( const string& file_ )
//-------------------------------------------------------------------------------
{
	return mkPath( "images", file_ );
}

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
	return min_ + random() % ( max_ - min_ + 1 );
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
class Cfg : public Fl_Preferences
//-------------------------------------------------------------------------------
{
typedef Fl_Preferences Inherited;
	struct User
	{
		User() : score(0), level(0), completed(0) {}
		string name;
		int score;
		int level;
		int completed;
	};
public:
	Cfg( const char *vendor_, const char *appl_ ) :
		Inherited( USER, vendor_, appl_ )
	{
		load();
	}
	void load()
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
//			printf(" entry: '%s' %d %d %d\n", e.name.c_str(), e.score, e.level, e.completed );
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
	void write( const string& user_,
		int score_ = -1,
		int level_ = -1,
		bool completed_ = false )
	{
		set( "last_user", user_.c_str() );
		Fl_Preferences user( this, user_.c_str() );
		if ( score_ >= 0 )
			user.set( "score", (int)score_ );
		if ( level_ >= 0 )
			user.set( "level", (int)level_ );
		if ( completed_ )
		{
			int completed;
			user.get( "completed", completed, 0 );
			completed++;
			user.set( "completed", completed );
		}
		load();
	}
	const User& read( const string& user_ )
	{
		load();
		for ( size_t i = 0; i < _users.size(); i++ )
		{
			if ( _users[i].name == user_ )
				return _users.at( i );
		}
		Fl_Preferences user( this, user_.c_str() );
		write( user_, 0, 0 );
		return read( user_ );
	}
	bool remove( const string& user_ )
	{
		bool ret = deleteGroup( user_.c_str() ) != 0;
		load();
		return ret;
	}
	size_t non_zero_scores() const
	{
		for ( size_t i = 0; i < _users.size(); i++ )
			if ( !_users[i].score )
				return i;
		return _users.size();
	}
	const User& best() const
	{
		static const User zero;
		return _users.size() ? _users[0] : zero;
	}
	unsigned hiscore() const { return best().score; }
	const vector<User>& users() const { return _users; }
	const string& last_user() const { return _last_user; }
private:
	vector<User> _users;
	string _last_user;
};

//-------------------------------------------------------------------------------
class Audio
//-------------------------------------------------------------------------------
{
public:
	static Audio *instance();
	bool play( const char *file_ );
	bool disabled() const { return _disabled; }
	bool enabled() const { return !_disabled; }
	void disable( bool disable_ = true ) { _disabled = disable_; }
	void cmd( const string& cmd_ ) { _cmd = cmd_; }
private:
	Audio() :
		_disabled( false ) {}
private:
	bool _disabled;
	string _cmd;
};

//-------------------------------------------------------------------------------
// class Audio
//-------------------------------------------------------------------------------
/*static*/
Audio *Audio::instance()
//-------------------------------------------------------------------------------
{
	static Audio *inst = 0;
	if ( !inst )
		inst = new Audio();
	return inst;
}

bool Audio::play( const char *file_ )
//-------------------------------------------------------------------------------
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
//		printf( "cmd: %s\n", cmd.c_str() );
#ifdef WIN32
//		::PlaySound( wavPath( file_ ).c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NOSTOP );
		if ( cmd.empty() )
			cmd = "playsound " + wavPath( file_ );
		STARTUPINFO si = { 0 };
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi;
		ret = !CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
		CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
#elif __APPLE__
		if ( cmd.empty() )
			 cmd = "play -q " + wavPath( file_ ) + " &";
//		printf( "cmd: %s\n", cmd.c_str() );
		ret = system( cmd.c_str() );
#else
		// linux
		if ( cmd.empty() )
			cmd = "aplay -q -N " + wavPath( file_ ) + " &";
//		printf( "cmd: %s\n", cmd.c_str() );
		ret = system( cmd.c_str() );
#endif
	}
	return !_disabled && !ret;
}

//-------------------------------------------------------------------------------
class Object
//-------------------------------------------------------------------------------
{
public:
	Object( ObjectType o_, int x_, int y_, const char *image_ = 0, int w_ = 0, int h_ = 0 );
	virtual ~Object();
	void animate();
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
	void explode();
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
}

void Object::animate()
//-------------------------------------------------------------------------------
{
	if ( G_paused ) return;
	_ox += _w;
	if ( _ox >= _image->w() )
		_ox = 0;
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
	Fl_Shared_Image *image = Fl_Shared_Image::get( imgPath( image_ ).c_str() );
	if ( image && image->count() )
	{
		if ( _image )
			_image->release();
		_image = image;
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

/*virtual*/
void Object::draw()
//-------------------------------------------------------------------------------
{
	if ( !_exploded )
		_image->draw( x(), y(), _w, _h, _ox, 0 );
#ifdef DEBUG
	fl_color(FL_YELLOW);
	Rect r(rect());
	fl_rect(r.x(),r.y(),r.w(),r.h());
	fl_point( cx(), cy() );
#endif
	 if ( _exploding || _hit )
		draw_collision();
}

void Object::draw_collision() const
//-------------------------------------------------------------------------------
{
	int pts = w() * h() / 20;
	for ( int i = 0; i < pts; i++ )
	{
		int X = random() % w();
		int Y = random() % h();
		if ( !isTransparent( X + _ox, Y ) )
		{
			fl_rectf( x() + X - 2 , y() + Y - 2 , 4, 4,
				( random() % 2 ? FL_RED : FL_YELLOW ) );
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

void Object::explode()
//-------------------------------------------------------------------------------
{
	_explode( 0.05 );
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
	Rocket( int x_= 0, int y_ = 0 ) :
		Inherited( O_ROCKET, x_, y_, "rocket.gif" )
	{
	}
	bool lifted() const { return started(); }
	virtual const char *start_sound() const { return "launch.wav"; }
	virtual const char* image_started() const { return "rocket_launched.gif"; }
	void update()
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
	virtual const char *start_sound() const { return "drop.wav"; }
	virtual bool onHit()
	{
		// TODO: other image?
		return false;
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
		_up( false )	// default zuerst nach unten
	{
	}
	bool moving() const { return started(); }
	virtual const char *start_sound() const { return "bady.wav"; }
	void turn() { _up = !_up; }
	bool turned() const { return _up; }
	virtual bool onHit()
	{
		if ( hits() >= 4 )
			image( "bady_hit.gif" );
		return hits() > 4;
	}
	void update()
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
	virtual const char *start_sound() const { return "missile.wav"; }
	void update()
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
#ifdef DEBUG
		fl_color(FL_YELLOW);
		Rect r(rect());
		fl_rect(r.x(),r.y(),r.w(),r.h());
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
	virtual const char *start_sound() const { return "bomb_f.wav"; }
	void update()
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
class AnimText : public Object
//-------------------------------------------------------------------------------
{
// Note: This is just a quick impl. for showing an amimated level name
//       and should probably be made more flexible. Also the text drawing
//       could be combined with the FltWin::drawText().
typedef Object Inherited;
public:
	AnimText( int x_, int y_, int w_, const char *text_, Fl_Color color_ = FL_YELLOW ) :
		Inherited( (ObjectType)0, x_, y_, 0, w_ ),
		_text( text_ ),
		_color( color_ ),
		_sz( 10 ),
		_up( true ),
		_hold( 0 ),
		_done ( false )
	{
		update();
	}
	void draw()
	{
		if ( _done )
			return;
		fl_font( FL_HELVETICA_BOLD_ITALIC, _sz );
		int W = 0;
		int H = 0;
		fl_measure( _text.c_str(), W, H );
		fl_color( FL_BLACK );
		fl_draw( _text.c_str(), _text.size(), _x + _w / 2 - W / 2 + 2, _y + 2 );
		fl_color( _color );
		fl_draw( _text.c_str(), _text.size(), _x + _w / 2 - W / 2, _y );
		if ( W > _w - 30 )
			_hold = 1;
	}
	void update()
	{
		if ( G_paused ) return;
		Inherited::update();
		if ( _up )
		{
			if ( !_hold )
			{
				_sz++;
				if ( _sz > 30 )
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
			if ( _sz < 10 )
			{
				_done = true;
				return;
			}
		}
	}
	bool done() const { return _done; }
	virtual double timeout() const { return started() ? 0.02 : 0.02; }
private:
	string _text;
	Fl_Color _color;
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
public:
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
	void init()
	{
		bg_color = FL_BLUE;
		ground_color = FL_GREEN;
		sky_color = FL_DARK_GREEN;
		ls_outline_width = 0;
		outline_color_sky = FL_BLACK;
		outline_color_ground = FL_BLACK;
		flags = 0;
	}
public:
	Fl_Color bg_color;
	Fl_Color ground_color;
	Fl_Color sky_color;
	unsigned  ls_outline_width;
	Fl_Color outline_color_sky;
	Fl_Color outline_color_ground;
	unsigned long flags;
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
private:
	void add_score( unsigned score_ );
	void position_spaceship();
	void addScrollinZone();
	void addScrolloutZone();
	void create_terrain();
	void create_level();
	void draw_objects( bool pre_ );
	void draw_missiles();
	void draw_bombs();
	void draw_rockets();
	void draw_radars();
	void draw_drops();
	void draw_badies();
	void draw_cumuluses();
	void changeState( State toState_ = NEXT_STATE );
	void check_hits();
	void check_missile_hits();
	void check_bomb_hits();
	void check_rocket_hits();
	void check_drop_hits();
	bool iniValue( size_t level_, const string& id_,
	               int &value_, int min_, int max_, int default_ );
	bool iniValue( size_t level_, const string& id_,
	               string& value_, size_t max_, const string& default_ );
	void init_parameter();
	bool loadLevel( int level_, const string levelFileName_ = "" );
	void update_missiles();
	void update_bombs();
	bool collision( Object& o_, int w_, int h_ );
	void create_objects();
	void delete_objects();
	void update_objects();
	void update_drops();
	void update_badies();
	void update_cumuluses();
	void update_rockets();
	void update_radars();
	int drawText( int x_, int y_, const char *text_, size_t sz_, Fl_Color c_, ... ) const;
	void draw_score();
	void draw_scores();
	void draw_title();
	void draw();
	int handle( int e_ );
	void keyClick() const;
	void onActionKey();
	void onCollision();
	void onDemo();
	void onLostFocus();
	void onGotFocus();
	void onNextScreen( bool fromBegin_ = false );
	void onTitleScreen();
	void onStateChange( State from_state_ );
	void onUpdate();
	void onUpdateDemo();
	void setUser();
	void resetUser();
	void toggleUser( bool prev_ = false );
	bool paused() const { return _state == PAUSED; }
	void bombUnlock();
	static void cb_bomb_unlock( void *d_ );
	static void cb_demo( void *d_ );
	static void cb_paused( void *d_ );
	static void cb_update( void *d_ );
	State state() const { return _state; }
protected:
	Terrain T;
	vector<Missile *> Missiles;
	vector<Bomb *> Bombs;
	vector<Rocket *> Rockets;
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

	int _cumulus_min_start_speed;
	int _cumulus_max_start_speed;
private:
	State _state;
	int _xoff;
	int _ship_delay;
	int _ship_voff;
	int _ship_y;
	bool _left, _right, _up, _down;
	Spaceship *_spaceship;
	Rocket _rocket;
	Radar _radar;
	Drop _drop;
	Bady _bady;
	Cumulus _cumulus;
	bool _bomb_lock;
	bool _collision;	// level ended with collision
	bool _done;			// level ended by finishing
	unsigned _frame;
	unsigned _score;
	unsigned _old_score;
	unsigned _initial_score;
	unsigned _hiscore;
	string _hiscore_user;
	unsigned _first_level;
	unsigned _level;
	unsigned _done_level;
	unsigned _level_repeat;
	bool _cheatMode;
	bool _trainMode;	// started with level specification
	string _levelFile;
	string _input;
	string _username;
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
};

//-------------------------------------------------------------------------------
// class FltWin : public Fl_Double_Window
//-------------------------------------------------------------------------------
FltWin::FltWin( int argc_/* = 0*/, const char *argv_[]/* = 0*/ ) :
	Inherited( SCREEN_W, SCREEN_H, "FL-Trator v1.4" ),
	_state( START ),
	_xoff( 0 ),
	_left( false), _right( false ), _up( false ), _down( false ),
	_spaceship( new Spaceship( w() / 2, 40, w(), h() ) ),
	_bomb_lock( false),
	_collision( false ),
	_done(false),
	_frame( 0 ),
	_score(0),
	_old_score(0),
	_initial_score(0),
	_hiscore(0),
	_first_level(1),
	_level(0),
	_done_level( 0 ),
	_level_repeat(0),
	_cheatMode( false ),
	_trainMode( false ),
	_cfg( 0 ),
	_speed_right( 0 ),
	_internal_levels( false ),
	_enable_boss_key( false ),
	_focus_out( true ),
	_no_random( false ),
	_hide_cursor( false ),
	_title( label() ),
	_anim_text( 0 )
{
	int argc( argc_ );
	unsigned argi( 1 );
	bool reset_user( false );
	string unknown_option;
	string arg;
	bool usage( false );
	bool full_screen( false );
	while ( argc-- > 1 )
	{
		if ( unknown_option.size() )
			break;
		string arg( argv_[argi++] );
		if ( "--help" == arg || "-h" == arg )
		{
			usage = true;
			break;
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
			_username = arg.substr( 2, 10 );
		}
		else if ( arg[0] == '-' )
		{
			for ( size_t i = 1; i < arg.size(); i++ )
			{
				switch ( arg[i] )
				{
					case 'c':
						_hide_cursor = true;
						break;
					case 'e':
						_enable_boss_key = true;
						break;
					case 'f':
						full_screen = true;
						break;
					case 'n':
						reset_user = true;
						break;
					case 's':
						Audio::instance()->disable();
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
					default:
						unknown_option += arg[i];
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
	if ( unknown_option.size() )
	{
		printf( "Invalid option: '%s'\n\n", unknown_option.c_str() );
		usage = true;
	}
	if ( usage )
	{
		printf( "Usage:\n");
		printf( "  %s [level] [levelfile] [options]\n\n", argv_[0] );
		printf( "\tlevel      [1, 10] startlevel\n");
		printf( "\tlevelfile  name of levelfile (for testing a new level)\n");
		printf("Options:\n");
		printf("  -c\thide mouse cursor in window\n" );
		printf("  -e\tenable Esc as boss key\n" );
		printf("  -f\tenable full screen mode\n" );
		printf("  -h\tdisplay this help\n" );
		printf("  -i\tplay internal (auto-generated) levels\n" );
		printf("  -n\treset user (only if startet with -U)\n" );
		printf("  -p\tdisable pause on focus out\n" );
		printf("  -r\tdisable ramdomness\n" );
		printf("  -s\tstart with sound disabled\n" );
		printf("  -A\"playcmd\"\tspecify audio play command\n" );
		printf("   \te.g. -A\"play -q %%f\"\n" );
		printf("  -Uusername\tstart as user 'username'\n" );
		exit(0);
	}

	if ( !_trainMode )
		_levelFile.erase();

	// set spaceship image as icon
	Fl_RGB_Image icon( (Fl_Pixmap *)_spaceship->image() );
	Fl_Window::default_icon( &icon );
//	if ( full_screen )
//		resizable( this );

	string cfgName( "fltrator" );
	if ( _internal_levels )
		cfgName += "_internal";	// don't mix internal levels with real levels
	_cfg = new Cfg( "CG", cfgName.c_str() );
	if ( _username.empty() )
		_username = _cfg->last_user();

	if ( _username.empty() )
		_username = DEFAULT_USER;

	if ( !_trainMode && !reset_user )
		setUser();

	_level = _first_level;
	_hiscore = _cfg->hiscore();
	_hiscore_user = _cfg->best().name;
#if USE_FLTK_RUN
	Fl::add_timeout( FRAMES, cb_update, this );
#endif
	if ( !full_screen )
	{
		// try to center on current screen
		int X, Y, W, H;
		Fl::screen_xywh( X, Y, W, H );
		position( ( W - w() ) / 2, ( H - h() ) / 2 );
	}

	changeState();
	show();
	if ( full_screen )
		fullscreen();

	if ( _hide_cursor )
	{
		// hide mouse cursor
		cursor( FL_CURSOR_NONE, (Fl_Color)0, (Fl_Color)0 );
		default_cursor( FL_CURSOR_NONE, (Fl_Color)0, (Fl_Color)0 );
	}
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
			Audio::instance()->play( "ship_x.wav" );
		case LEVEL_DONE:
		{
			if ( _state == LEVEL_DONE )
			{
				Audio::instance()->play ( "done.wav" );
				if ( _score > _old_score )
					_old_score = _score;
				_done_level = _level;
				_first_level = _done_level + 1;
				if ( _first_level > MAX_LEVEL )
					_first_level = 1;
				if ( !_trainMode )
				{
					_cfg->write( _username, _score, _done_level, _done_level == MAX_LEVEL );
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
				_state = _trainMode ? LEVEL : TITLE;
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
//	printf( "position_spaceship(): X=%d Y=%d h()-Y =%d sky=%d\n", X, Y, h() - Y, sky );
	_spaceship->cx( X );
	_spaceship->cy( h() - Y );
}

void FltWin::addScrollinZone()
//-------------------------------------------------------------------------------
{
	T.insert( T.begin(), T[0] );
	T[0].object( 0 ); // ensure there are no objects in scrollin zone!
	for ( int i = 0; i < w() / 2; i++ )
		T.insert( T.begin(), T[0] );
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
	Fl::get_color( T.bg_color, R, G, B );
//	printf( "check for BG-Color %X/%X/%X\n", R, G, B );

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
		printf( "collision at %d + %d / %d + %d !\n", o_.x(), xc, o_.y(), yc  );
		printf( "object %dx%d xoff=%d yoff=%d\n", o_.w(), o_.h(), xoff, yoff );
		printf( "X=%d, Y=%d, W=%d, H=%d\n", X, Y, W, H );
#endif
	}
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
	iniValue( _level, "level_name", _level_name, 35, "" );
	ostringstream os;
	os << "Level " << _level;
	if ( _level_name.size() )
		os << ": " <<_level_name;
	os << " - " << _title;
	copy_label( os.str().c_str() );

	iniValue( _level, "rocket_start_prob", _rocket_start_prob, 0, 100,
		45. + 35. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ); // 45% - 80%
	iniValue( _level, "rocket_radar_start_prob", _rocket_radar_start_prob, 0, 100,
		60. + 40. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ); // 60% - 100%
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

	iniValue( _level, "drop_start_prob", _drop_start_prob, 0, 100,
		45. + 30. / ( MAX_LEVEL - 1 ) * ( _level - 1 ) ); // 45% - 75%
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
//	printf( "_bady_min_start_speed: %d\n", _bady_min_start_speed );
//	printf( "_bady_max_start_speed: %d\n", _bady_max_start_speed );

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
//	printf( "_cumulus_min_start_speed: %d\n", _cumulus_min_start_speed );
//	printf( "_cumulus_max_start_speed: %d\n", _cumulus_max_start_speed );
}

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
	_score = _old_score;

	_ini.clear();
	T.clear();
	if ( ( !_internal_levels || _levelFile.size() )
	       && loadLevel( _level, _levelFile ) )	// try to load level from landscape file
	{
		assert( (int)T.size() >= w() );
		if ( !( T.flags & Terrain::NO_SCROLLIN_ZONE ) )
			addScrollinZone();
		if ( !( T.flags & Terrain::NO_SCROLLOUT_ZONE ) )
			addScrolloutZone();
	}
	else
	{
		// internal level
		create_level();
	}

	// initialise the objects parameters (eventuall read from level file)
	init_parameter();
}

void FltWin::create_level()
//-------------------------------------------------------------------------------
{
	bool sky = ( _level % 2 || _level >= MAX_LEVEL - 1 ) && _level != 1;

//	printf( "create level(): _level: %u _level_repeat: %u, sky = %d\n",
//		_level, _level_repeat, sky );
	switch ( _level % 4 )
	{
		case 0:
			T.bg_color = fl_rgb_color( 97, 47, 4 );	// brownish
			T.ground_color = fl_rgb_color( 241, 132, 0 ); // orange
			T.ls_outline_width = 4;
			T.outline_color_ground = FL_BLACK;
			break;
		case 1:
			T.bg_color = FL_BLUE;
			T.ground_color = FL_GREEN;
			break;
		case 2:
			T.bg_color = FL_GRAY;
			T.ground_color = FL_RED;
			T.ls_outline_width = _level / 2;
			T.outline_color_ground = FL_WHITE;
			T.outline_color_sky = FL_WHITE;
			break;
		case 3:
			T.bg_color = FL_CYAN;
			T.ground_color = FL_DARK_GREEN;
			T.ls_outline_width = _level > 5 ? 4 : 1;
			T.outline_color_ground = FL_CYAN;
			T.outline_color_sky = FL_CYAN;
			break;
	}
	if ( sky )
	{
		Fl_Color temp = T.bg_color;
		T.bg_color = T.ground_color;
		T.ground_color = temp;
		T.ground_color = fl_darker( T.ground_color );
		T.bg_color = fl_lighter( T.bg_color );
	}
	else if ( _level > 5 )
	{
		T.bg_color = fl_darker( T.bg_color );
		T.ground_color = fl_darker( T.ground_color );
	}
	T.sky_color = T.ground_color;

	int range = (h() * (sky ? 2 : 3)) / 5;
	int bott = h() / 5;
	for ( size_t i = 0; i < 15; i++ )
	{
		int r = i ? range : range / 2;
		int peak = random() % r  + 20;
		int j;
		for ( j = 0; j < peak; j++ )
			T.push_back( TerrainPoint( j + bott ) );
		int flat = random() % ( w() / 2 );
		if ( random() % 3 == 0 )
			flat = random() % 10;
		for ( j = 0; j < flat; j++ )
			T.push_back( T.back() );
		for ( j = peak; j >= 0; j-- )
			T.push_back( TerrainPoint( j + bott ) );
		/*int */flat = random() % ( w() / 2 );
		for ( j = 0; j < flat; j++ )
			T.push_back( T.back() );
	}

	if ( sky )
	{
		// sky
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

	// setup object positions
	int min_dist = 100 - _level * 5;
	int max_dist = 200 - _level * 5;
	if ( min_dist < _rocket.w() )
		min_dist = _rocket.w();
	if ( max_dist < _rocket.w() * 3 )
		max_dist = _rocket.w() * 3;

	int last_x = random() % max_dist + ( _level * min_dist * 2 ) / 3;	// first object farther away!
	while ( last_x < (int)T.size() )
	{
		if ( h() - T[last_x].ground_level() > ( 100 - (int)_level * 10 ) /*&& random() % 2*/ )
		{
			bool flat( true );
			for ( int i = -5; i < 5; i++ )
				if ( T[last_x].ground_level() != T[last_x + i].ground_level() )
					flat = false;
			switch ( random() % _level ) // 0-9
			{
				case 0:
				case 1:
					if ( flat && random()%3 == 0 )
						T[last_x].object( O_RADAR );
					else
						T[last_x].object( O_ROCKET );
					break;
				case 2:
				case 3:
					if ( sky && random()%3 ==  0)
						T[last_x].object( O_DROP );
					else if ( flat && random()%2 == 0 )
						T[last_x].object( O_RADAR );
					else
						T[last_x].object( O_ROCKET );
					break;
				case 4:
				case 5:
				case 6:
					if ( sky )
						T[last_x].object( random()%2 ? O_DROP : O_ROCKET );
					else
						T[last_x].object( random()%3 ? O_BADY : O_ROCKET );
					break;
				case 7:
				case 8:
					break;
				case 9:
					if ( random() % 4 == 0 )
						T[last_x].object( O_CUMULUS );
					else
						T[last_x].object( O_BADY );
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
		draw_radars();
		draw_drops();
		draw_badies();
	}
	else
	{
		draw_cumuluses();
		if ( _anim_text )
			_anim_text->draw();
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

void FltWin::draw_radars()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Radars.size(); i++ )
	{
		Radars[i]->draw();
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
				Audio::instance()->play( "missile_hit.wav" );
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

		vector<Radar *>::iterator ra = Radars.begin();
		for ( ; ra != Radars.end(); )
		{
			if ( !((*ra)->exploding()) &&
			     ((*m)->rect().intersects( (*ra)->rect() ) ||
			     (*m)->rect().inside( (*ra)->rect())) )
			{
				// radar hit by missile
				if ( (*ra)->hit() )	// takes 2 missile hits to succeed!
				{
					Audio::instance()->play( "missile_hit.wav" );
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
					Audio::instance()->play( "bady_hit.wav" );
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
				Audio::instance()->play( "drop_hit.wav" );
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
				Audio::instance()->play( "bomb_x.wav" );
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

		vector<Radar *>::iterator ra = Radars.begin();
		for ( ; ra != Radars.end(); )
		{
			if ( !((*ra)->exploding()) &&
			     ((*b)->rect().intersects( (*ra)->rect() ) ||
			     (*b)->rect().inside( (*ra)->rect())) )
			{
				// radar hit by bomb
				Audio::instance()->play( "bomb_x.wav" );
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

void FltWin::check_rocket_hits()
//-------------------------------------------------------------------------------
{
	vector<Rocket *>::iterator r = Rockets.begin();
	for ( ; r != Rockets.end(); )
	{
		if ( (*r)->lifted() && (*r)->rect().inside( _spaceship->rect() ) )
		{
			// rocket hit by spaceship
			(*r)->crash();
		}
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
			// drop maybe hit spaceship - is within it's bounding box
//			printf( "drop inside spaceship rectangle\n" );
			if ( collision( *(*d), w(), h() ) )
			{
//				printf( "drop fully hit spaceship\n" );
				delete *d;
				d = Drops.erase(d);
				_collision = true;
				onCollision();
				continue;
			}
		}
		else if ( (*d)->dropped() && (*d)->rect().intersects( _spaceship->rect() ) )
		{
			// drop touches spaceship
//			printf( "drop only partially hit spaceship\n" );
			if ( (*d)->x() > _spaceship->x() )	// front of ship
				(*d)->x( (*d)->x() + 10 );
#if 0
// removed deflection on rear of ship (looks awkward!)
			else // rear of ship
				(*d)->x( (*d)->x() - 10 );
#endif
			++d;
		}
		else
			++d;
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
			int start_dist = _rocket_max_start_dist - _level * 20 -
			                 random() % _rocket_var_start_dist;
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
		if ( o & O_DROP && i - _drop.w() / 2 < w() )
		{
			Drop *d = new Drop( i, T[_xoff + i].sky_level() + _drop.h() / 2 );
			int start_dist = _drop_max_start_dist - _level * 20 -
			                 random() % _drop_var_start_dist;
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
			printf( "#badies: %lu\n", (unsigned long)Badies.size() );
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
	f >> T.bg_color;
	f >> T.ground_color;
	f >> T.sky_color;

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

void FltWin::update_objects()
//-------------------------------------------------------------------------------
{
	update_missiles();
	update_bombs();
	update_rockets();
	update_radars();
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
			int dist = Drops[i]->x() - _spaceship->x(); // center distance S<->R
			int start_dist = (int)Drops[i]->data1();	// 400 - _level * 20 - random() % 200;
			if ( dist <= 0 || dist >= start_dist ) continue;
//			printf( "start drop #%ld, start_dist=%d?\n", i, start_dist );
			// really start drop?
			int drop_prob = _drop_start_prob;
//			printf( "drop_prob: %d\n", drop_prob );
			if ( random() % 100 < drop_prob )
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
			int dist = Rockets[i]->x() - _spaceship->x(); // center distance S<->R
			int start_dist = (int)Rockets[i]->data1();	// 400 - _level * 20 - random() % 200;
			if ( dist <= 0 || dist >= start_dist ) continue;
//			printf( "start rocket #%ld, start_dist=%d?\n", i, start_dist );
			// really start rocket?
			int lift_prob = _rocket_start_prob;
//			printf( "lift prob: %d\n", lift_prob );
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
			if ( random() % 100 < lift_prob )
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

void FltWin::update_radars()
//-------------------------------------------------------------------------------
{
	for ( size_t i = 0; i < Radars.size(); i++ )
	{
		if ( !paused() )
			Radars[i]->x( Radars[i]->x() - DX);
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
	if ( _trainMode )
	{
		drawText( w() - 110, 20, "TRAINER", 20, FL_RED );
	}

	fl_font( FL_HELVETICA, 30 );
	fl_color( FL_WHITE );

	char buf[50];
	int n = snprintf( buf, sizeof(buf), "L %u/%u Score: %06u", _level,
		MAX_LEVEL_REPEAT - _level_repeat, _score );
	fl_draw( buf, n, 20, h() - 30 );
	n = snprintf( buf, sizeof(buf), "Hiscore: %06u", _hiscore );
	fl_draw( buf, n, w() - 280, h() - 30 );
	n = snprintf( buf, sizeof(buf), "%d %%",
			(int)( (float)_xoff / (float)( T.size() - w() ) * 100. ) );
	fl_draw( buf, n, w() / 2 - 20 , h() - 30 );

	if ( G_paused )
	{
		drawText( -1, 50, "** PAUSED **", 50, FL_WHITE );
	}
	else if ( _done )
	{
		if ( _level == MAX_LEVEL && ( !_trainMode || _cheatMode ) )
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
	drawText( x, Y, "new hiscore ......... %06u", 30, FL_WHITE, _score );

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

	if ( _username == DEFAULT_USER )
		drawText( -1, 420, "Enter your name:   %s", 32, FL_GREEN, inpfield.c_str() );
	else
		drawText( -1, 420, "Score goes to:   %s", 32, FL_GREEN, _username.c_str() );
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
		fl_rectf( 0, 0, w(), h(), fl_rgb_color( 0, 0, 0 ) );
	else
		fl_rectf( 40, 20, w() - 80, h() - 40, fl_rgb_color( 32, 32, 32 ) );
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

	if ( /*!_xoff &&*/ _cfg->non_zero_scores() && _flip++ % 10 >= 7 )
	{
		size_t n = _cfg->non_zero_scores();
		if ( n > 6 )
			n = 6;
		int y = 330 - n / 2 * 40;
		drawText( -1, y, "Hiscores", 35, FL_GREEN );
		y += 50;
		int x = -1;
		for ( size_t i = 0; i < n; i++ )
		{
			string name( _cfg->users()[i].name );
			x = drawText( x, y, "%05u .......... %s", 30,
				_cfg->users()[i].completed ? FL_YELLOW : FL_WHITE,
				_cfg->users()[i].score, name.empty() ? DEFAULT_USER : name.c_str() );
			y += 40;
		}
	}
	else
	{
		drawText( -1, 210, "%s (%u)", 30, FL_GREEN,
			_username.empty() ? "N.N." : _username.c_str(), _first_level );
		int x = drawText( -1, 260, "q/a .......... up/down", 30, FL_WHITE );
		drawText( x, 300, "o/p .......... left/right", 30, FL_WHITE );
		drawText( x, 340, "p ............. fire missile", 30, FL_WHITE );
		drawText( x, 380, "space ...... drop bomb", 30, FL_WHITE );
		drawText( x, 420, "7-9 .......... hold game", 30, FL_WHITE );
		drawText( x, 460, "s ............. %s sound", 30, FL_WHITE,
			Audio::instance()->enabled() ? "disable" : "enable"  );
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
	for ( int i = 0; i < w(); i++ )
	{
		fl_color( T.sky_color );
		fl_yxline( i, -1, T[_xoff + i].sky_level() );
		fl_color( T.ground_color );
		fl_yxline( i, h() - T[_xoff + i].ground_level(), h() );
	}

	// draw outline
	if ( T.ls_outline_width )
	{
		fl_line_style( FL_SOLID, T.ls_outline_width );
		for ( int i = 0; i < w(); i++ )
		{
			if ( T[_xoff + i].sky_level() >= 0 )
			{
				fl_color( T.outline_color_sky );
				fl_line ( i - 1, T[_xoff + i - 1].sky_level(), i + 1, T[_xoff + i + 1].sky_level() );
			}
			fl_color( T.outline_color_ground );
			fl_line( i - 1, h() - T[_xoff + i - 1].ground_level(), i + 1,
				h() - T[_xoff + i + 1].ground_level() );
		}
		fl_line_style( 0 );
	}

	draw_objects( true );	// objects for collision check

	if ( !G_paused && !paused() && !_done && _state != DEMO )
		_collision |= collision( *_spaceship, w(), h() );

	if ( _collision )
		onCollision();

	if ( _state != DEMO )
	{
		if ( !paused() || _frame % (FPS / 2) < FPS / 4 || _collision )
			_spaceship->draw();
	}

	draw_objects( false );	// objects AFTER collision check (=without collision)

	draw_score();

	if ( _collision )
		_spaceship->draw_collision();
}

void FltWin::onCollision()
//-------------------------------------------------------------------------------
{
	if ( _cheatMode )
	{
		if ( _xoff % 20 == 0 )
			Audio::instance()->play( "boink.wav" );
		_collision = false;
	}
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

void FltWin::setUser()
//-------------------------------------------------------------------------------
{
	_initial_score = _cfg->read( _username ).score;
	_old_score = _initial_score;
//	printf( "old score = %u\n", _old_score );
	_first_level = _cfg->read( _username ).level + 1;
	if ( _first_level > MAX_LEVEL )
		_first_level = 1;
	_level = _first_level;
}

void FltWin::resetUser()
//-------------------------------------------------------------------------------
{
	_score = 0;
	_initial_score = 0;
	_old_score = 0;
	_first_level = 1;
	_level = 1;
	_cfg->write( _username, _score, _level );
	_cfg->load();
	keyClick();
	onTitleScreen();	// immediate display + reset demo timer
}

void FltWin::keyClick() const
//-------------------------------------------------------------------------------
{
	Audio::instance()->play( "drop.wav" );
}

void FltWin::toggleUser( bool prev_/* = false */ )
//-------------------------------------------------------------------------------
{
	int found = -1;
	for ( size_t i = 0; i < _cfg->users().size(); i++ )
	{
		if ( _username == _cfg->users()[i].name )
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
		_username = _cfg->users()[found].name;
		setUser();

		keyClick();
		onTitleScreen();	// immediate display + reset demo timer
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
		// get rid of escape key closing window (except in boss mode and title screen)
		return ( _enable_boss_key || _state == TITLE ) ? Inherited::handle( e_ ) : 1;

	if ( e_ == FL_KEYUP && ( _state != SCORE ) && ( 's' == c ) )
	{
		if ( _state == TITLE )
			keyClick();
		Audio::instance()->disable( !Audio::instance()->disabled() );
		if ( _state == TITLE )
		{
			keyClick();
			onTitleScreen();	// update display
		}
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
		if ( _state == TITLE && e_ == FL_KEYUP && ( 'q' == c || 'o' == c ) )
			toggleUser( true );
		if ( _state == TITLE && e_ == FL_KEYUP && ( 'a' == c || 'p' == c ) )
			toggleUser();
		if ( _state == TITLE && e_ == FL_KEYUP && ( 'r' == c ) )
			resetUser();
		if ( e_ == FL_KEYDOWN && c == ' ' )
		{
			G_paused = false;
			ignore_space = true;
			keyClick();
			onActionKey();
		}
		return Inherited::handle( e_ );
	}
	if ( e_ == FL_KEYDOWN )
	{
		G_paused = false;
//		printf( "keydown '%c'\n", c >= 32 ? c : '?' );
		if ( 'o' == c )
			_left = true;
		if ( 'p' == c )
		{
			repeated_right++;
			_right = true;
		}
		if ( 'q' == c )
			_up = true;
		if ( 'a' == c )
			_down = true;
		return 1;
	}
	if ( e_ == FL_KEYUP )
	{
//		printf( "keyup '%c'\n", c >= 32 ? c : '?' );
		if ( 'o' == c )
			_left = false;
		if ( 'p' == c )
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
		if ( 'q' == c )
			_up = false;
		if ( 'a' == c )
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
	FltWin *f = (FltWin *)d_;
	f->state() == FltWin::DEMO ? f->onUpdateDemo() : f->onUpdate();
#if USE_FLTK_RUN
	Fl::repeat_timeout( FRAMES, cb_update, d_ );
#endif
}

void FltWin::onActionKey()
//-------------------------------------------------------------------------------
{
//	printf( "onActionKey\n" );
	Fl::remove_timeout( cb_demo, this );
	Fl::remove_timeout( cb_paused, this );
	if ( _state == SCORE )
	{
		if ( _input.empty() )
			_input = DEFAULT_USER;
		if ( _username == DEFAULT_USER && _input != DEFAULT_USER )
			_cfg->remove( _username );	// remove default user
		_username = _input;
//		printf( "_username: '%s'\n", _username.c_str() );
//		printf( "_input: '%s'\n", _input.c_str() );
//		printf( "_score: '%u'\n", _score );
		_cfg->write( _username, _score, _done_level );
		_cfg->flush();
		_hiscore = _cfg->hiscore();
		_hiscore_user = _cfg->best().name;
		_old_score = _score;
		_initial_score = _score;
		_first_level = _done_level + 1;
		if ( _first_level > MAX_LEVEL )
			_first_level = 1;
		printf( "score saved.\n" );
	}

	changeState();
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

	delete_objects();

	_xoff = 0;
	_collision = false;

	bool show_scores = false;

	if ( fromBegin_ )
	{
		_level = _state == DEMO ? 1 : _first_level;
		_level_repeat = 0;
		_score = _initial_score;
	}
	else
	{
		show_scores = _old_score > _hiscore && !_done;

		if ( _done || _state == DEMO )
		{
			_level++;
			_level_repeat = 0;

			if ( _level > MAX_LEVEL )
			{
				_level = _first_level; // restart with level 1
				_score = _initial_score;
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
//				_score = _initial_score;
				onTitleScreen();
			}
		}
	}

	_spaceship = new Spaceship( w() / 2, 40, w(), h() );	// before create_terrain()!

	create_terrain();
	position_spaceship();

	if ( !_level_repeat )
		(_anim_text = new AnimText( 0, 40, w(), _level_name.c_str() ))->start();

	_done = false;
	G_paused = false;
	_left = _right = _up = _down = false;

	show_scores &= _level == _first_level && _level_repeat == 0;
	if ( show_scores )
	{
		_input = _username == DEFAULT_USER ? "" : _username;
		changeState( SCORE );
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
	if ( !G_paused )
		Fl::add_timeout( 20.0, cb_demo, this );
}

void FltWin::onUpdateDemo()
//-------------------------------------------------------------------------------
{
	create_objects();

	update_objects();

	redraw();

	if ( _done )
	{
		onNextScreen();	// ... but for now, just skip pause in demo mode
		return;	// do not increment _xoff now!
	}

	_xoff += DX;
	if ( _xoff + /*2 * */w() >= (int)T.size() - 1 )
		_done = true;
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
		if ( ox != _spaceship->x() )
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
		changeState( LEVEL_DONE );

	_xoff += DX;
	if ( _xoff + /*2 * */w() >= (int)T.size() - 1 )
		_done = true;
}

void FltWin::bombUnlock()
//-------------------------------------------------------------------------------
{
	_bomb_lock = false;
}

int FltWin::run()
//-------------------------------------------------------------------------------
{
#if USE_FLTK_RUN
//	printf( "Using Fl::run()\n" );
	return Fl::run();
#else

#ifndef WIN32
	unsigned long startTime;
	unsigned long endTime;
#ifdef _POSIX_MONOTONIC_CLOCK
//	printf( "Using clock_gettime()\n" );
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	startTime = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
#else
//	printf( "Using gettimeofday()\n" );
	struct timeval tv;
	gettimeofday( &tv, NULL );
	startTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif

#else // ifndef WIN32

	LARGE_INTEGER startTime, endTime, elapsedMicroSeconds;
	LARGE_INTEGER frequency;

	SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

	QueryPerformanceFrequency( &frequency );
	QueryPerformanceCounter( &startTime );
#endif // ifndef WIN32

	unsigned delayMilliSeconds = 1000 / FPS;

	while ( Fl::first_window() )
	{
		unsigned elapsedMilliSeconds = 0;

		while ( elapsedMilliSeconds < delayMilliSeconds && Fl::first_window() )
		{
#ifndef WIN32
			Fl::wait( 0.0005 );
//			Fl::wait( 0.001 );

#ifdef _POSIX_MONOTONIC_CLOCK
			clock_gettime( CLOCK_MONOTONIC, &ts );
			endTime = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
#else
			gettimeofday( &tv, NULL );
			endTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
			elapsedMilliSeconds = endTime - startTime;
#else // ifndef WIN32

			Fl::wait( 0.0 );

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
		_state == DEMO ? onUpdateDemo() : onUpdate();
	}
	return 0;
#endif
}

//-------------------------------------------------------------------------------
int main( int argc_, const char *argv_[] )
//-------------------------------------------------------------------------------
{
	fl_register_images();
	FltWin fltrator( argc_, argv_ );
	return fltrator.run();
}
