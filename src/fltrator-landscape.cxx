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
#include <unistd.h>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_GIF_Image.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/filename.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

#include <cassert>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace std;

static const unsigned MAX_LEVEL = 10;
static const int MAX_SCREENS = 10;
static const int SCREEN_W = 800;
static const int SCREEN_H = 600;


enum ObjectType
{
   O_ROCKET = 1,
   O_DROP = 2,
   O_BADY = 4,
   O_CUMULUS = 8,
   O_RADAR = 16,
	O_PHASER = 32,
   O_COLOR_CHANGE = 64
};

static const char *HelpText = \
"         === Landscape Editor Help ===\n\n \
Move view with Left/Right/Pg Up/Pg Down/Home/End.\n \
Double click in preview to jump that position.\n \
Toggle mode PLACE/EDIT with key 'm'.\n\n \
In EDIT mode draw the ground by dragging mouse\n \
with left button or the sky with right button.\n \
Set 'Scroll Lock' to move the view automatically.\n \
Use 'Shift' to set single points.\n \
Use 'Ctrl' to draw horizontal lines.\n\n \
Double click on landscape to select colors for\n \
ground/land/sky (with 'Ctrl': outline colors).\n \
Show/hide a magnifier window with 'z'.\n \
Use key 'o' to change outline width.\n\n \
In PLACE mode select object type to place with\n \
'1'-'9' and place/remove objects with double click.\n\n \
When closing the window all changes will be saved.\n \
Press ESC if you want to exit *without* saving.";

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

static LevelPath imgPath( "images" );

static string mkLevelName( int level_ )
//--------------------------------------------------------------------------
{
	ostringstream os;
	os << "L_" << level_ << ".txt";
	return os.str();
}

//--------------------------------------------------------------------------
class Object
//--------------------------------------------------------------------------
{
public:
	Object( int id_, const string& imageName_, const string& name_ = "" ) :
		_id( id_ ),
		_image( 0 ),
		_name( name_ ),
		_w( 0 ),
		_h( 0 )
	{
		image( imageName_ );
	}
	void image( const string& imageName_ )
	{
		if ( _image )
			delete _image;
		_image = 0;
		_imageName = imageName_;
		if ( imageName_.size() )
		{
			_image = new Fl_GIF_Image( imgPath.get( imageName_ ).c_str() );
			if ( _image->w() == 0 && _image->h() == 0 )
				cerr << "Invalid image '" << imageName_ << "'" << endl;
			_w = _image->w();
			_h = _image->h();
			const char *p = strstr( imageName_.c_str(), "_" );
			if ( p && isdigit( p[1] ) )
			{
				p++;
				int frames = atoi( p );
				if ( frames )
				{
					_w = _image->w() / frames;
				}
			}
		}
	}
	string name() const { return _name; }
	string imageName() const { return _imageName; }
	const int id() const { return _id; }
	Fl_Image *image() const { return _image; }
	void w( int w_ ) { _w = w_; }
	void h( int h_ ) { _h = h_; }
	int w() const { return _w; }
	int h() const { return _h; }
private:
	int _id;
	Fl_Image *_image;
	string _name;
	string _imageName;
	int _w;
	int _h;
};

//--------------------------------------------------------------------------
class Objects : public vector<Object>
//--------------------------------------------------------------------------
{
public:
	Object* find( int id_ )
	{
		for ( size_t i = 0; i < size(); i++ )
			if ( at(i).id() == id_ )
				return &at(i);
		return 0;
	}
};


Objects objects;

//--------------------------------------------------------------------------
class LSPoint
//--------------------------------------------------------------------------
{
public:
	LSPoint() :
		sky(-1),
		ground(0),
		object(0)
	{}
	LSPoint( int ground_, int sky_ = -1, int object_ = 0 ) :
		sky(sky_),
		ground(ground_),
		object(object_)
	{}
	int sky;
	int ground;
	int object;
	Fl_Color bg_color;
	Fl_Color ground_color;
	Fl_Color sky_color;
};

//-------------------------------------------------------------------------------
class Terrain : public vector<LSPoint>
//-------------------------------------------------------------------------------
{
	typedef vector<LSPoint> Inherited;
public:
	Terrain() :
		Inherited(),
		bg_color( FL_BLUE ),
		ground_color( FL_GREEN ),
		sky_color( FL_DARK_GREEN ),
		outline_width( 0 ),
		outline_color_sky( FL_BLACK ),
		outline_color_ground( FL_BLACK )
	{}
public:
	Fl_Color bg_color;
	Fl_Color ground_color;
	Fl_Color sky_color;
	unsigned outline_width;
	Fl_Color outline_color_sky;
	Fl_Color outline_color_ground;
};

//--------------------------------------------------------------------------
class LS
//--------------------------------------------------------------------------
{
public:
	LS( size_t W_, const char *f_ = 0, bool *loaded_ = 0 ) :
		_flags( 0 ),
		_xoff( 0 )
	{
		if ( loaded_ ) *loaded_ = false;
		// Note: When W_ = 0 and a file is read in
		// it's size is used and there is no expansion.
		size_t W = W_ ? W_ : MAX_SCREENS * SCREEN_W;
		if ( f_ )
		{
			ifstream f( f_ );
			if ( f.is_open() )
			{
				cout << "reading from " << f_ << endl;
				if ( loaded_ ) *loaded_ = true;
				unsigned long version;
				f >> version;
				f >> _flags;
				if ( version )
				{
					// new format
					f >> _ls.outline_width;
					f >> _ls.outline_color_ground;
					f >> _ls.outline_color_sky;
				}
				f >> _ls.bg_color;
				f >> _ls.ground_color;
				f >> _ls.sky_color;

				while ( f.good() )
				{
					int s, g;
					int o;
					f >> s;
					f >> g;
					f >> o;
					if ( !f.good() ) break;
					if ( size() < W ) // access data is ignored!
						_ls.push_back( LSPoint( g, s, o ) );
					if ( o & O_COLOR_CHANGE )
					{
						// fetch extra data for color change object
						Fl_Color bg_color, ground_color, sky_color;
						f >> bg_color;
						f >> ground_color;
						f >> sky_color;
						if ( f.good() && size() <= W )
						{
							_ls.back().bg_color = bg_color;
							_ls.back().ground_color = ground_color;
							_ls.back().sky_color = sky_color;
						}
					}
				}
				f.clear(); 	// reset for reading of ini section
				string line;
				while ( getline( f, line ) )
				{
					_ini.push_back( line );
				}
			}
		}
		// If it's new file (nothing could be read) or
		// if W_ is specified, then it is expanded to W_
		if ( ( size() && W_ ) || !size() )	// expand to W_
		{
			while ( size() < W )
				_ls.push_back( LSPoint() );
		}
	}
	void setGround( int x_, int g_ )
	{
		if ( x_ >= 0 && x_ < (int)size() )
			_ls[ x_ ].ground = g_;
	}
	void setSky( int x_, int s_ )
	{
		if ( x_ >= 0 && x_ < (int)size() )
			_ls[ x_ ].sky = s_;
	}
	void setRocket( int x_, bool set_ )
	{
		if ( x_ >= 0 && x_ < (int)size() )
		{
			if ( set_ )
				_ls[ x_ ].object = _ls[ x_ ].object | O_ROCKET;
			else
				_ls[ x_ ].object = _ls[ x_ ].object & ~O_ROCKET;
		}
	}
	void setObject( int id_, int x_, bool set_ )
	{
		if ( x_ >= 0 && x_ < (int)size() )
		{
			if ( set_ )
			{
				_ls[ x_ ].object = _ls[ x_ ].object | id_;
				if ( id_ == O_COLOR_CHANGE )
				{
					_ls[ x_ ].bg_color = _ls.bg_color;
					_ls[ x_ ].ground_color = _ls.ground_color;
					_ls[ x_ ].sky_color = _ls.sky_color;
				}
			}
			else
				_ls[ x_ ].object = _ls[ x_ ].object & ~id_;
		}
	}
	string iniSection() const
	{
		ostringstream os;
		for ( size_t i = 0; i < _ini.size(); i++ )
			os << _ini[ i ] << endl;
		return os.str();
	}
	LSPoint& point( int x_ ) { return _ls[ x_ ]; }
	int ground( int x_ ) const { return _ls[ x_ ].ground; }
	int sky( int x_ ) const { return _ls[ x_ ].sky; }
	bool hasObject( int obj_, int x_ ) const { return _ls[ x_ ].object & obj_; }
	bool hasRocket( int x_ ) const { return _ls[ x_ ].object & O_ROCKET; }
	bool hasDrop( int x_ ) const { return _ls[ x_ ].object & O_DROP; }
	bool hasBady( int x_ ) const { return _ls[ x_ ].object & O_BADY; }
	bool hasCumulus( int x_ ) const { return _ls[ x_ ].object & O_CUMULUS; }
	bool hasRadar( int x_ ) const { return _ls[ x_ ].object & O_RADAR; }
	bool hasPhaser( int x_ ) const { return _ls[ x_ ].object & O_PHASER; }
	int object( int x_ ) const { return _ls[ x_ ].object; }
	size_t size() const { return _ls.size(); }
	Fl_Color bg_color() const { return _ls.bg_color; }
	Fl_Color ground_color() const { return _ls.ground_color; }
	Fl_Color sky_color() const { return _ls.sky_color; }
	void bg_color( Fl_Color bg_color_ ) { _ls.bg_color = bg_color_; }
	void ground_color( Fl_Color ground_color_ ) { _ls.ground_color = ground_color_; }
	void sky_color( Fl_Color sky_color_ ) { _ls.sky_color = sky_color_; }
	unsigned outline_width() const { return _ls.outline_width; }
	void outline_width( unsigned outline_width_ ) { _ls.outline_width = outline_width_; }
	Fl_Color outline_color_ground() const { return _ls.outline_color_ground; }
	Fl_Color outline_color_sky() const { return _ls.outline_color_sky; }
	void outline_color_ground( Fl_Color outline_color_ground_ )
	{ _ls.outline_color_ground = outline_color_ground_ ; }
	void outline_color_sky( Fl_Color outline_color_sky_ )
	{ _ls.outline_color_sky = outline_color_sky_ ; }
	const Terrain& terrain() const { return _ls; }
	unsigned long flags() const { return _flags; }
	void xoff( int xoff_ ) { _xoff = xoff_; }
	int xoff() const { return _xoff; }
private:
	Terrain _ls;
	unsigned long _flags;
	int _xoff;
	vector<string> _ini;
};

//--------------------------------------------------------------------------
class PreviewWindow : public Fl_Double_Window
//--------------------------------------------------------------------------
{
	typedef Fl_Double_Window Inherited;
public:
	static const double Scale;
	PreviewWindow( int H_, LS *ls_ );
private:
	void draw();
	int handle( int e_ );
private:
	LS * _ls;
};

//--------------------------------------------------------------------------
class ZoomWindow : public Fl_Double_Window
//--------------------------------------------------------------------------
{
	typedef Fl_Double_Window Inherited;
public:
	ZoomWindow( size_t sz_ );
	int handle( int e_ );
	void setPoint( int x_, int y_, Fl_Color c_ );
	int sz() const { return _sz; }
	void close()
	{
		_hidden = false;
		Inherited::hide();
	}
	void show()
	{
		Inherited::show();
		_hidden = false;
	}
	void hide()
	{
		if ( shown() )
		{
			_hidden = true;
			Inherited::hide();
		}
	}
	void draw()
	{
		Inherited::draw();
		fl_rect( 0, 0, w(), h(), FL_BLACK );
		fl_rect( 1, 1, w() - 2, h() - 2, FL_WHITE );
	}
	bool hidden() const { return _hidden; }
private:
	int _sz;
	bool _hidden;
};

//--------------------------------------------------------------------------
class Slider : public Fl_Hor_Slider
//--------------------------------------------------------------------------
{
	typedef Fl_Hor_Slider Inherited;
public:
	Slider( int x_, int y_, int w_, int h_, int P_, int S_ ) :
		Inherited( x_, y_, w_, h_ )
	{
		reset( P_, S_ );
	}
	void reset( int P_, int S_ )
	{
		bounds( 0, S_ - P_ );
		step( P_ / 8 );
//		slider_size( 40. * 8. / (double)( S_ - P_ ) );
		slider_size( P_ / (double)( S_ - P_ ) );
		value( 0 );
	}
};

//--------------------------------------------------------------------------
class LSEditor : public Fl_Double_Window
//--------------------------------------------------------------------------
{
	typedef Fl_Double_Window Inherited;
public:
	enum LE_MODE
	{
	   EDIT_LANDSCAPE = 1,
	   PLACE_OBJECTS = 2
	};
	LSEditor( int argc_ = 0, const char *argv_[] = 0 );
	void hide();
	void draw();
	int handle( int e_ );
	void gotoXPos( size_t xpos_ );
	void placeObject( int obj_, int x_, int y_ );
	void reloadImages();
	void redraw();
	void save();
	int searchObject( int obj_, int x_, int y_ );
	bool get_nearest_color_change_marker( int x_,
		Fl_Color& sky_color_, Fl_Color& bg_color_, Fl_Color& ground_color_ );
	bool set_nearest_color_change_marker( int x_,
		Fl_Color& sky_color_, Fl_Color& bg_color_, Fl_Color& ground_color_ );
	void selectColor( int x_, int y_ );
	void setTitle();
	void xoff( int xoff_ ) { _xoff = xoff_; }
	int xoff() const { return _xoff; }
	void onLoad( int level_ = 0 );
	void onSave() { save(); }
	void onQuit() { _dont_save = true; hide(); }
	void onSaveAs();
	void onXoff();
	bool changed() const { return _changed; }
	bool dont_save() const { return _dont_save; }
	void update_zoom( int x_, int y_ );
private:
	static void load_cb( Fl_Widget *o_, void *d_ )
		{ ((LSEditor *)d_)->onLoad( atoi( ((Fl_Menu_Bar *)o_)->mvalue()->label() ) ); }
	static void save_as_cb( Fl_Widget *o_, void *d_ ) { ((LSEditor *)d_)->onSaveAs(); }
	static void save_cb( Fl_Widget *o_, void *d_ ) { ((LSEditor *)d_)->onSave();}
	static void quit_cb( Fl_Widget *o_, void *d_ ) { ((LSEditor *)d_)->onQuit();}
	static void xoff_cb( Fl_Widget *o_, void *d_ ) { ((LSEditor *)d_)->onXoff();}
private:
	LS *_ls;
	int _xoff;
	string _name;
	PreviewWindow *_preview;
	ZoomWindow *_zoom;
	LE_MODE _mode;
	Fl_Image *_rocket;
	Fl_Image *_drop;
	Fl_Image *_bady;
	Fl_Image *_cumulus;
	Fl_Image *_radar;
	Fl_Image *_phaser;
	int _objType;
	bool _scrollLock;
	int _curr_x;
	int _curr_y;
	bool _changed;
	Fl_Menu_Bar *_menu;
	Slider *_slider;
	bool _menu_shown;
	bool _dont_save;
};


//--------------------------------------------------------------------------
class ZoomDot : public Fl_Box
//--------------------------------------------------------------------------
{
	typedef Fl_Box Inherited;
public:
	ZoomDot( int x_, int y_, int w_, int h_ ) :
		Inherited( x_, y_, w_, h_ )
	{
		box( FL_FLAT_BOX );
	}
	virtual void draw()
	{
		Inherited::draw();
		if ( box() == FL_BORDER_BOX )
		{
			fl_color( fl_contrast( FL_BLACK, color() ) );
			fl_rect( x(), y(), w(), h() );
		}
	}
};

//--------------------------------------------------------------------------
// class ZoomWindow : public Fl_Double_Window
//--------------------------------------------------------------------------
ZoomWindow::ZoomWindow( size_t sz_ ) :
//--------------------------------------------------------------------------
	Inherited( 20 * sz_ + 4, 20 * sz_ + 4, "Zoom" ),
	_sz( sz_ ),
	_hidden( false )
{
	int W = w() / sz_;
	for ( int x = 0; x < _sz; x++ )
	{
		for ( int y = 0; y < _sz; y++ )
		{
			Fl_Box * b = new ZoomDot( x * W + 2, y * W + 2, W, W );
			if ( x == _sz / 2 && y == _sz / 2 )
				b->box( FL_BORDER_BOX );
		}
	}
	end();
	border( 0 );
	set_non_modal();
	set_menu_window();
//	show();
}

void ZoomWindow::setPoint( int x_, int y_, Fl_Color c_ )
//--------------------------------------------------------------------------
{
	child( x_ * _sz + y_ )->color( c_ );
	color( fl_contrast( FL_BLACK, child(0)->color() ) );	// keep border in contrast!
}

int ZoomWindow::handle( int e_ )
//--------------------------------------------------------------------------
{
	int key = Fl::event_key();
	if ( e_ == FL_KEYDOWN  && ( key == FL_Escape  || key == 'z' ) )
	{
		close();
		return 1;
	}
	return Inherited::handle( e_ );
}

//--------------------------------------------------------------------------
// class PreviewWindow : public Fl_Double_Window
//--------------------------------------------------------------------------
/*static const*/
const double PreviewWindow::Scale = 10.;

PreviewWindow::PreviewWindow( int H_, LS *ls_ ) :
//--------------------------------------------------------------------------
	Inherited( (int)( ls_->size() / Scale ), (int)( H_ / Scale ), "Preview" ),
	_ls( ls_ )
{
	ostringstream os;
	os << label() << " 1 : " << (int)Scale;
	copy_label( os.str().c_str() );
	end();
	show();
}

void PreviewWindow::draw()
//--------------------------------------------------------------------------
{
	Inherited::draw();
	fl_color( _ls->bg_color() );
	fl_rectf( 0, 0, w(), h() );

	fl_color( _ls->sky_color());
	for ( int i = 0; i < (int)_ls->size(); i++ )
	{
		int S = _ls->sky( i );
		fl_line( (int)((double)i / Scale), -1,
		         (int)((double)i / Scale), (int((double)S / Scale)) - 1 );	// TODO: -1 ??
	}

	fl_color( _ls->ground_color() );
	for ( int i = 0; i < (int)_ls->size(); i++ )
	{
		int G = _ls->ground( i );
		fl_line( (int)((double)i / Scale), h() - int((double)G / Scale),
		         (int)((double)i / Scale), h() );
	}

	// draw outline
	if ( _ls->outline_width() )
	{
		fl_color( _ls->outline_color_sky() );
		fl_line_style( FL_SOLID, 1 );
		for ( int i = 1; i < (int)_ls->size() - 1; i++ )
		{
			int S0 = _ls->sky( i - 1 );
			int S1 = _ls->sky( i + 1 );
			fl_line( (int)( (double)(i - 1) / Scale ),
			         (int)( (double)S0 / Scale ),
			         (int)( (double)(i + 1) / Scale ),
			         (int)( (double)S1 / Scale ) );
		}
		fl_color( _ls->outline_color_ground() );
#ifdef WIN32
		fl_line_style( FL_SOLID, 1 );
#endif
		for ( int i = 1; i < (int)_ls->size() - 1; i++ )
		{
			int G0 = _ls->ground( i - 1 );
			int G1 = _ls->ground( i + 1 );
			fl_line( (int)( (double)(i - 1) / Scale ),
			         h() - (int)( (double)G0 / Scale ),
			         (int)( (double)(i + 1) / Scale ),
			         h() - (int)( (double)G1 / Scale ) );
		}
		fl_line_style( 0 );
	}
	// draw visible window hint
	fl_color( FL_WHITE );
	fl_line_style( FL_DOT );
	fl_rect( (int)( (double)_ls->xoff() / Scale ), 0,
	         (int)( (double)(SCREEN_W - 1) / Scale ), h() - 1 );
	fl_line_style( 0 );
}

int PreviewWindow::handle( int e_ )
//--------------------------------------------------------------------------
{
	int x = Fl::event_x();
//	int y = Fl::event_y();
	int mode = Fl::event_clicks() > 0;
	int c = Fl::event_key();
	if ( ( e_ == FL_KEYDOWN || e_ == FL_KEYUP ) && c == FL_Escape )
		// get rid of ecape key closing window
		return 1;

	switch ( e_ )
	{
		case FL_PUSH:
			if ( mode )
			{
				Fl_Window *win = Fl::first_window();
				while ( win && !dynamic_cast<LSEditor *>( win ) )
					win = Fl::next_window( win );
				if ( win )
					((LSEditor *)win)->gotoXPos( (int)(x * Scale) );
			}
			break;
	}
	return Inherited::handle( e_ );
}

//--------------------------------------------------------------------------
// class LSEditor : public Fl_Double_Window
//--------------------------------------------------------------------------
LSEditor::LSEditor( int argc_/* = 0*/, const char *argv_[]/* = 0*/ ) :
	Inherited( SCREEN_W, SCREEN_H + 20, "LSEditor" ),
	_xoff( 0 ),
	_preview( 0 ),
	_zoom( 0 ),
	_mode( EDIT_LANDSCAPE ),
	_rocket( 0 ),
	_drop( 0 ),
	_bady( 0 ),
	_cumulus( 0 ),
	_objType( 1 ),
	_scrollLock( false ),
	_curr_x( 0 ),
	_curr_y( 0 ),
	_changed( false ),
	_menu( 0 ),
	_slider( 0 ),
	_menu_shown( true ),
	_dont_save( false )
//--------------------------------------------------------------------------
{
	int argc( argc_ );
	unsigned argi( 1 );
	int level = 0;
	int screens = 0;
	string levelFile;
	while ( argc-- > 1 )
	{
		string arg( argv_[argi++] );
		if ( "--help" == arg )
		{
			cout << "Usage:" << endl;
			cout << "  " << argv_[0] << " [level] [levelfile] [options]" << endl << endl;
			cout << "              Defaults are _ls.txt -s " << MAX_SCREENS << endl << endl;
			cout << "Options:" << endl;
			cout << "  -p          start in object place mode" << endl;
			cout << "  -s screens  number of screens" << endl << endl;
			cout << "Note: Specifying screens with an existing file" << endl;
			cout << "will expand/shrink the file to the given value!" << endl;
			exit( 0 );
		}

		int maybe_level = atoi( arg.c_str() );
		if ( maybe_level > 0 && maybe_level <= (int)MAX_LEVEL )
		{
			level = maybe_level;
		}
		else if ( arg[0] == '-' )
		{
			for ( size_t i = 1; i < arg.size(); i++ )
			{
				switch ( arg[i] )
				{
					case 'p':
						_mode = PLACE_OBJECTS;
						break;
					case 's':
						if ( argc > 1 )
						{
							string s( argv_[ argi++ ] );
							argc--;
							screens = atoi( s.c_str() );
							if ( screens < 2 || screens > MAX_SCREENS )
							{
								cerr << "Invalid screens parameter: " << screens <<
								   " [2-" << MAX_SCREENS << " allowed]" << endl;
								exit( -1 );
							}
						}
						break;
					default:
						cerr << "Invalid option -" << arg[i] << endl;
						exit( -1 );
				}
			}
		}
		else
		{
			levelFile = arg;
			cout << "level file: '" << levelFile.c_str() << "'" << endl;
		}
	}
	if ( level && levelFile.size() )
	{
		cerr << "Supply either a level number or a level file!" << endl;
		exit( -1 );
	}
	if ( level )
	{
		imgPath.level( level );
		_name = levelPath( mkLevelName( level ) );
	}
	else
	{
		_name = levelFile;
	}

	printf("screens: %d\n", screens );
	bool loaded;
	if ( _name.empty() )
		_name = "_ls.txt";
	string name( _name.size() ? _name.c_str() : "_ls.txt" );
	_ls = new LS( screens * w(), name.c_str(), &loaded );

	if ( _mode == PLACE_OBJECTS && !loaded )
	{
		cerr <<  "Can't load '" << name.c_str() << "'" << endl;
		exit( -1 );
	}

	objects.push_back( Object( O_ROCKET, "rocket.gif", "Rocket" ) );
	objects.push_back( Object( O_DROP, "drop.gif", "Drop" ) );
	objects.push_back( Object( O_BADY, "bady.gif", "Badguy" ) );
	objects.push_back( Object( O_CUMULUS, "cumulus.gif", "Good Cloud" ) );
	objects.push_back( Object( O_RADAR, "radar_00014_0200.gif", "Radar" ) );
	objects.push_back( Object( O_PHASER, "phaser.gif", "Phaser" ) );
	objects.push_back( Object( O_COLOR_CHANGE, string(), "Color Change Marker" ) );
	objects.back().w( 3 );
	objects.back().h( h() );
	_rocket = (Fl_Image *)objects.find( O_ROCKET )->image();
	_drop = (Fl_Image *)objects.find( O_DROP )->image();
	_bady = (Fl_Image *)objects.find( O_BADY )->image();
	_cumulus = (Fl_Image *)objects.find( O_CUMULUS )->image();
	_radar = (Fl_Image *)objects.find( O_RADAR )->image();
	_phaser = (Fl_Image *)objects.find( O_PHASER )->image();

	if ( _mode == PLACE_OBJECTS )
	{
		if ( !_rocket || !_drop || !_bady || !_cumulus || !_radar || !_phaser )
		{
			cerr << "Can't load all object images!" << endl;
			exit( -1 );
		}
	}

	color( FL_BLUE );

	static Fl_Menu_Item items[] =
	{
		{ "File", 0, 0, 0, FL_SUBMENU },
			{ "Load level..", 0, load_cb, this, 0 },
			{ "Load #level..", 0, load_cb, this, FL_SUBMENU },
				{"1", 0, load_cb, this },
				{"2", 0, load_cb, this },
				{"3", 0, load_cb, this },
				{"4", 0, load_cb, this },
				{"5", 0, load_cb, this },
				{"6", 0, load_cb, this },
				{"7", 0, load_cb, this },
				{"8", 0, load_cb, this },
				{"9", 0, load_cb, this },
				{"10", 0, load_cb, this },
				{0},
			{ "Save level as..", 0, save_as_cb, this, 0 },
			{ "Save", 0, save_cb, this, FL_MENU_DIVIDER },
			{ "Exit without saving", 0, quit_cb, this, 0 },
			{ 0 },
		{ 0 }
	};
	_menu = new Fl_Sys_Menu_Bar( 0, 0, 45, 20, "&File" );
	_menu->box( FL_FLAT_BOX );
	_menu->menu( items );
	_menu->tooltip( "Hide/show this menu with F10" );

	_slider = new Slider( 0, SCREEN_H, w(), h() - SCREEN_H, w(), _ls->size() );
	_slider->callback( xoff_cb, this );
	_slider->visible_focus( 0 );
	end();
	setTitle();
	_preview = new PreviewWindow( SCREEN_H, _ls );
	show();
	_zoom = new ZoomWindow( 11 );
}

void LSEditor::hide()
//--------------------------------------------------------------------------
{
	if ( _preview )
		_preview->hide();
	if ( _zoom )
		_zoom->close();
	Inherited::hide();
}

void LSEditor::draw()
//--------------------------------------------------------------------------
{
	Inherited::draw();
	int H = SCREEN_H;

	Fl_Color bg_color( _ls->bg_color() );
	Fl_Color sky_color( _ls->sky_color() );
	Fl_Color ground_color( _ls->ground_color() );
	get_nearest_color_change_marker( _xoff, sky_color, bg_color, ground_color );

	fl_color( bg_color );
	fl_rectf( 0, 0, w(), H );
	fl_color( sky_color );

	for ( int i = _xoff; i < _xoff + w(); i++ )
	{
		int S = _ls->sky( i );
		fl_line( i - _xoff, -1, i - _xoff, S  - 1 );	// TODO: -1 ??
	}

	fl_color( ground_color );
	for ( int i = _xoff; i < _xoff + w(); i++ )
	{
		int G = H - _ls->ground( i );
		fl_line( i - _xoff, G, i - _xoff, H );
	}

	// draw outline
	if ( _ls->outline_width() )
	{
		fl_color( _ls->outline_color_sky() );
		fl_line_style( FL_SOLID, _ls->outline_width() );
		for ( int i = ( _xoff ? _xoff : 1 ); i < _xoff + w() - 1; i++ )
			fl_line( i - _xoff - 1, _ls->sky( i - 1 ),
		            i - _xoff + 1, _ls->sky( i + 1 ) );

		fl_color( _ls->outline_color_ground() );
#ifdef WIN32
		fl_line_style( FL_SOLID, _ls->outline_width() );
#endif
		for ( int i = ( _xoff ? _xoff : 1 ); i < _xoff + w() - 1; i++ )
			fl_line( i - _xoff - 1, H - _ls->ground( i  - 1 ),
		            i - _xoff + 1, H - _ls->ground( i  + 1 ) );
		fl_line_style( 0 );
	}

	for ( int i = _xoff; i < _xoff + w(); i++ )
	{
		if ( _ls->hasRocket( i ) )
		{
			int G = H - _ls->ground( i );
			if ( _rocket )
				_rocket->draw( i - _xoff - _rocket->w() / 2, G - _rocket->h() );
		}
		if ( _ls->hasDrop( i ) )
		{
			int S = _ls->sky( i );
			if ( _drop )
				_drop->draw( i - _xoff - _drop->w() / 2, S  );
		}
		if ( _ls->hasBady( i ) )
		{
			int S = _ls->sky( i );
			if ( _bady )
				_bady->draw( i - _xoff - _bady->w() / 2, S  );
		}
		if ( _ls->hasCumulus( i ) )
		{
			int S = _ls->sky( i );
			if ( _cumulus )
				_cumulus->draw( i - _xoff - _cumulus->w() / 2, S  );
		}
		if ( _ls->hasRadar( i ) )
		{
			int G = H - _ls->ground( i );
			Object *o = objects.find( O_RADAR );
			if ( o )
				o->image()->draw( i - _xoff - o->w() / 2, G - o->h(), o->w(), o->h() );
		}
		if ( _ls->hasPhaser( i ) )
		{
			int G = H - _ls->ground( i );
			Object *o = objects.find( O_PHASER );
			if ( o )
				o->image()->draw( i - _xoff - o->w() / 2, G - o->h(), o->w(), o->h() );
		}
		if ( _ls->hasObject( O_COLOR_CHANGE, i ) )
		{
			fl_color( FL_RED );
			fl_line_style( FL_DOT, 4 ); // set after color (WIN32)!
			fl_line( i - _xoff, 0, i - _xoff, H );
			fl_line_style( 0 );
		}
	}
#if 0
	// temp. deavtivated till needed (keyboard drawing)
	if ( _mode == EDIT_LANDSCAPE )
	{
		fl_color( FL_RED );
		fl_circle( _curr_x, _curr_y, 2 );
		fl_line( _curr_x - 5, _curr_y - 5, _curr_x + 6, _curr_y + 6 );
		fl_line( _curr_x + 5, _curr_y - 5, _curr_x - 6, _curr_y + 6 );
	}
#endif
	Inherited::draw_children();
}

void LSEditor::update_zoom( int x_, int y_ )
//--------------------------------------------------------------------------
{
//	printf( "%d/%d\n", x_, y_ );
	if ( !_zoom )
		return;
	if ( _zoom->hidden() )
		_zoom->show();
	if ( !_zoom->shown() )
		return;

	const int sz = _zoom->sz();
	int X(  x_ - sz / 2 );
	int Y(  y_ - sz / 2 );
	int H = sz;
	int W = sz;
	make_current();
	const uchar *screen = fl_read_image( 0, X, Y, W, H );
	if ( !screen )
		return;

	const int d = 3;
	unsigned char r, g, b;

	for ( int y = 0; y < H; y++ ) // lines
	{
		long index = y * W * d; // line start offset
		for ( int x = 0; x < W; x++ )
		{
			r = *(screen + index + 0);
			g = *(screen + index + 1);
			b = *(screen + index + 2);

//			printf( "%02X%02X%02X ", r, g, b );
			_zoom->setPoint( x, y, fl_rgb_color( r, g, b ) );

			index += d;
		}
//		printf( "\n");
	}
	_zoom->position( this->x() + x_ + 30, this->y() + y_ + 30 ),
//	_zoom->show();
	      _zoom->redraw();
	delete[] screen;
}

int LSEditor::handle( int e_ )
//--------------------------------------------------------------------------
{
	static int xo = -1;
	static bool ground = true;
	static int snap_to_y = -1;
	int x = Fl::event_x();
	int y = Fl::event_y();
	int mode = Fl::event_clicks() > 0;

	if ( y >= SCREEN_H && e_ != FL_LEAVE && e_ != FL_ENTER )
		return Inherited::handle( e_ );

	switch ( e_ )
	{
		case FL_KEYDOWN:
		{
			int key = Fl::event_key();
//			printf( "key: %X (%c)\n", key, key );
			if ( 0xffbe == key ) // F1
			{
				fl_alert( "%s", HelpText );
			}
			if ( 0xffc7 == key ) // F10
			{
				if ( _menu_shown )
					_menu->hide();
				else
					_menu->show();
				_menu_shown = !_menu_shown;
				redraw();
			}
			if ( 'm' == key )
			{
				if ( _mode == EDIT_LANDSCAPE && _rocket && _drop && _bady && _cumulus )
					_mode = PLACE_OBJECTS;
				else
					_mode = EDIT_LANDSCAPE;
				setTitle();
			}

			if ( 'o' == key )
			{
				unsigned ow = _ls->outline_width();
				ow++;
				ow %= 5;
				_ls->outline_width( ow );
				redraw();
			}

			if ( 'z' == key )
			{
				if ( _zoom )
				{
					if ( _zoom->shown() )
						_zoom->close();
					else
					{
						_zoom->show();
						update_zoom( x, y );
					}
				}
			}

			if ( key == FL_Scroll_Lock )
			{
				_scrollLock = !_scrollLock;
				printf( "_scrollLock: %d\n", _scrollLock );
				setTitle();
			}

			if ( key >= '1' && key <= '9' )
			{
				_objType = key - '0';
				setTitle();
			}

			if ( FL_Left == key )
			{
				_xoff -= w() / 8;
				if ( _xoff < 0 )
					_xoff = 0;
				redraw();
			}

			if ( FL_Right == key )
			{
				_xoff += w() / 8;
				if ( _xoff + w() > (int)_ls->size() )
					_xoff = _ls->size() - w();
				redraw();
			}

			if ( FL_Page_Down == key )
			{
				_xoff -= w();
				if ( _xoff < 0 )
					_xoff = 0;
				redraw();
			}

			if ( FL_Page_Up == key )
			{
				_xoff += w();
				if ( _xoff + w() > (int)_ls->size() )
					_xoff = _ls->size() - w();
				redraw();
			}

			if ( FL_Home == key )
			{
				_xoff = 0;
				redraw();
			}

			if ( FL_End == key )
			{
				_xoff = _ls->size() - w();
				redraw();
			}

			setTitle();
			break;
		}

		case FL_LEAVE:
			if ( _zoom && _zoom->shown() )
			{
				_zoom->hide();
			}
			break;

		case FL_ENTER:
			if ( _zoom && _zoom->hidden() )
				_zoom->show();
			break;

		case FL_PUSH:
			xo = -1;
			ground = ( Fl::event_button() == FL_LEFT_MOUSE );
			snap_to_y = Fl::event_ctrl() ? y : -1;
			if ( mode )
			{
				// double click
				if ( _mode == EDIT_LANDSCAPE )
					selectColor( x, y );
				else
				{
					switch ( _objType )
					{
						case 2:
							placeObject( O_DROP, x, y );
							break;
						case 3:
							placeObject( O_BADY, x, y );
							break;
						case 4:
							placeObject( O_CUMULUS, x, y );
							break;
						case 5:
							placeObject( O_RADAR, x, y );
							break;
						case 6:
							placeObject( O_PHASER, x, y );
							break;
						case 9:
							placeObject( O_COLOR_CHANGE, x, y );
							break;
						default:
							placeObject( O_ROCKET, x, y );
					}
				}
				return 1;
			}
			else
			{
				// single click
				if ( _mode == EDIT_LANDSCAPE )
				{
					_curr_x = x;
					_curr_y = y;
					if ( Fl::event_shift() )
					{
						// pixel mode for fine adjustments
						if ( ground )
							_ls->setGround( _xoff + x, SCREEN_H - y );
						else
							_ls->setSky( _xoff + x, y );

						redraw();
					}
				}
			}
			break;


		case FL_MOVE:
		case FL_DRAG:
			update_zoom( x, y );
			if ( e_ == FL_MOVE || _mode != EDIT_LANDSCAPE )
				return 0;
			if ( mode )
			{
//				fprintf( stderr, "double click and drag ignored\n");
				return 0;
			}
//			printf( "%s %d/%d xo=%d mode:%d\n", (e_==FL_PUSH ? "FL_PUSH" : "FL_DRAG" ), x, y, xo, mode );
//			if ( x >= 0 && x < w() && y >= 0 && y < SCREEN_H ) // because of drag!
			{
				int x0 = x;
				int x1 = x;
				if ( xo != - 1)
				{
					if ( xo < x )
						x0 = xo;
					else
						x1 = xo;
				}
				int Y = snap_to_y != -1 ? snap_to_y : y;
				for ( int X = x0; X < x1; X++ )
				{
					if ( ground )
						_ls->setGround( _xoff + X, SCREEN_H - Y );
					else
						_ls->setSky( _xoff + X, Y );

					_changed = true;
				}
				xo = x;
				if ( _scrollLock || Fl::event_shift() )
				{
					// exp. autoscroll
					int xoff = _xoff;
#if 0
					if ( x > w() - w() / 4 )
						_xoff += w() / 128;
					if ( _xoff + w() >= (int)_ls->size() )
						_xoff = _ls->size() - w();
#else
					if ( x > w() - w() / 2 )
						_xoff += 4;
					if ( _xoff + w() >= (int)_ls->size() )
						_xoff = _ls->size() - w();
#endif
					int scrolled = ( _xoff - xoff );
					xo = x - scrolled;
					setTitle();
				}
				redraw();
			}
			break;
		case FL_RELEASE:
			if ( ( _mode == EDIT_LANDSCAPE ) && _changed )
				setTitle();
			break;
	}
	return Inherited::handle( e_ );
}

void LSEditor::gotoXPos( size_t xpos_ )
//--------------------------------------------------------------------------
{
	_xoff = xpos_ - w() / 2;
	if ( _xoff < 0 )
		_xoff = 0;
	if ( _xoff + w() > (int)_ls->size() )
		_xoff = _ls->size() - w();
	setTitle();
	redraw();
}

void LSEditor::onLoad( int level_/*= 0*/ )
//--------------------------------------------------------------------------
{
	if ( changed() )
	{
		if ( !Fl::event_ctrl() )
		{
			fl_alert( "There are unsaved changes!\n\n"
			          "Either save them before\n"
			          "or try again with Ctrl-key held down." );
			return;
		}
	}
	string name;
	if ( level_ )
	{
		name = levelPath( mkLevelName( level_ ) );
	}
	if ( name.empty() )
	{
		const char *fname = fl_file_chooser( "Load a level file", "L_*.txt", "", 0 );
		if ( fname )
			name = fname;
	}
	if ( name.empty() )
		return;
	bool loaded( false );
	LS *ls = new LS( 0, name.c_str(), &loaded );
	if ( ls && loaded )
	{
		delete _ls;
		_ls = ls;
		_xoff = 0;
		delete _preview;
		_preview = new PreviewWindow( SCREEN_H, _ls );
		_slider->reset( w(), _ls->size() );
		_name = name;
		if ( level_ )
		{
			imgPath.level( level_ );
			reloadImages();
		}
		_changed = false;
		setTitle();
	}
	redraw();
}

void LSEditor::onSaveAs()
//--------------------------------------------------------------------------
{
	char * name = fl_file_chooser( "Save level as", "L_*.txt", _name.c_str(), 0 );
	if ( !name )
		return;
	_name = name;
	save();
}

void LSEditor::onXoff()
//--------------------------------------------------------------------------
{
	_zoom->hide();
	int xoff = _slider->value();
//	printf( "onXoff xoff = %d _xoff = %d\n", xoff, _xoff );
	if ( xoff < 0 )
		xoff = 0;
	if ( xoff + w() > (int)_ls->size() )
		xoff = _ls->size() - w();
	_xoff = xoff;
	redraw();
	setTitle();
}

void LSEditor::placeObject( int obj_, int x_, int y_ )
//--------------------------------------------------------------------------
{
	int X = _xoff + x_;
	int x = searchObject( obj_, x_, y_ );
	if ( x )
	{
		_ls->setObject( obj_, x - 1, false );
	}
	else
	{
		_ls->setObject( obj_, X, true );
	}
	_changed = true;
	redraw();
}

void LSEditor::reloadImages()
//--------------------------------------------------------------------------
{
	for ( size_t i = 0; i < objects.size();  i++ )
		objects[i].image( objects[i].imageName().c_str() );
	_rocket = (Fl_Image *)objects.find( O_ROCKET )->image();
	_drop = (Fl_Image *)objects.find( O_DROP )->image();
	_bady = (Fl_Image *)objects.find( O_BADY )->image();
	_cumulus = (Fl_Image *)objects.find( O_CUMULUS )->image();
	_radar = (Fl_Image *)objects.find( O_RADAR )->image();
	_phaser = (Fl_Image *)objects.find( O_PHASER )->image();
}

void LSEditor::redraw()
//--------------------------------------------------------------------------
{
	_ls->xoff( _xoff );	// for preview
	_slider->value( _xoff );
	Inherited::redraw();
	if ( _preview )
		_preview->redraw();
	if ( _zoom && !_zoom->hidden() )
		update_zoom( Fl::event_x(), Fl::event_y() );
}

void LSEditor::save()
//--------------------------------------------------------------------------
{
	string name( _name );
	if ( name.empty() )
		name = "_ls.txt";
	cout << "saving to " << name.c_str() << endl;
	ofstream f( name.c_str() );
	f << 1 << endl;	// version
	f << _ls->flags() << endl;	// preserved flags (only hand-editable currently)
	// new format
	f << _ls->outline_width() << endl;
	f << _ls->outline_color_ground() << endl;
	f << _ls->outline_color_sky() << endl;

	f << _ls->bg_color() << endl;
	f << _ls->ground_color() << endl;
	f << _ls->sky_color() << endl;
	for ( size_t i = 0; i < _ls->size(); i++ )
	{
		f << _ls->sky( i ) << " " << _ls->ground( i ) << " " << _ls->object( i );
		if ( _ls->object( i ) & O_COLOR_CHANGE )
		{
			// save extra data for color change object
			f << " " << _ls->point(i).bg_color;
			f << " " << _ls->point(i).ground_color;
			f << " " << _ls->point(i).sky_color;
		}
		f << endl;
	}
	// write ini section
	f << _ls->iniSection();
	_changed = false;
	setTitle();
}

int LSEditor::searchObject( int obj_, int x_, int y_ )
//--------------------------------------------------------------------------
{
	Object *o = objects.find( obj_ );
	assert( o );
	int X = _xoff + x_;
	int W = o->w();
	int x = X - W / 2;
	if ( x < 0 )
	{
		W += x;
		x = 0;
	}
	for ( int i = x; i < x + W; i++ )
	{
		if ( i >= (int)_ls->size() ) break;
		if ( _ls->hasObject( obj_, i ) )
		{
			return i + 1;
		}
	}
	return 0;
}

bool LSEditor::get_nearest_color_change_marker( int x_,
      Fl_Color& sky_color_, Fl_Color& bg_color_, Fl_Color& ground_color_ )
//--------------------------------------------------------------------------
{
	int X( x_ );
	while ( X > 0 )
	{
		if ( _ls->hasObject( O_COLOR_CHANGE, X ) )
		{
			// Hack: access points directly - to much hassle otherwise
			sky_color_ = _ls->point( X ).sky_color;
			bg_color_ = _ls->point( X ).bg_color;
			ground_color_ = _ls->point( X ).ground_color;
			return true;
		}
		--X;
	}
	return false;
}

bool LSEditor::set_nearest_color_change_marker( int x_,
      Fl_Color& sky_color_, Fl_Color& bg_color_, Fl_Color& ground_color_ )
//--------------------------------------------------------------------------
{
	int X( x_ );
	while ( X > 0 )
	{
		if ( _ls->hasObject( O_COLOR_CHANGE, X ) )
		{
			// Hack: access points directly - to much hassle otherwise
			_ls->point( X ).sky_color = sky_color_;
			_ls->point( X ).bg_color = bg_color_;
			_ls->point( X ).ground_color = ground_color_;
			return true;
		}
		--X;
	}
	return false;
}

void LSEditor::selectColor( int x_, int y_ )
//--------------------------------------------------------------------------
{
	int X = _xoff + x_;
	int G = _ls->ground( X );
	int S = _ls->sky( X );
	Fl_Color c;
	enum Mode
	{
	   SKY,
	   GROUND,
	   LS,
	   SKY_OUTLINE,
	   GROUND_OUTLINE
	};
	Mode m;
	string prompt( "Select ");
	Fl_Color sky_color = _ls->sky_color();
	Fl_Color ground_color = _ls->ground_color();
	Fl_Color bg_color = _ls->bg_color();
	bool marker = get_nearest_color_change_marker( X, sky_color, bg_color, ground_color );

	bool alt = Fl::event_ctrl();
	if ( y_ < S )
	{
		c = alt ? _ls->outline_color_sky() : sky_color;
		m = alt ? SKY_OUTLINE : SKY;
		prompt += alt ? "sky outline color" : marker ? "sky color marker" : "sky color";
	}
	else if ( y_ > SCREEN_H - G )
	{
		c = alt ? _ls->outline_color_ground() : ground_color;
		m = alt ? GROUND_OUTLINE : GROUND;
		prompt += alt ? "ground outline color" : marker ? "ground color marker" : "ground color";
	}
	else
	{
		c = bg_color;
		m = LS;
		prompt += marker ? "BG color marker" : "BG color";
	}
	unsigned char r, g, b;
	Fl::get_color( c, r, g, b );
	if ( fl_color_chooser( prompt.c_str(), r, g, b, 1 ) )
	{
		Fl_Color nc = fl_rgb_color( r, g, b );
		if ( m == SKY )
			sky_color =  nc;
		else if ( m == GROUND )
			ground_color = nc;
		else if ( m == SKY_OUTLINE )
			_ls->outline_color_sky( nc );
		else if ( m == GROUND_OUTLINE )
			_ls->outline_color_ground( nc );
		else
			bg_color = nc;
		if ( !set_nearest_color_change_marker( X, sky_color, bg_color, ground_color ) )
		{
			_ls->sky_color( sky_color );
			_ls->ground_color( ground_color );
			_ls->bg_color( bg_color );
		}
		redraw();
	}
}

void LSEditor::setTitle()
//--------------------------------------------------------------------------
{
	string t( label() ? label() : "" );
	size_t pos = t.find( " - " );
	if ( pos != string::npos )
		t.erase( pos );
	ostringstream os;
	string placeInfo;
	int obj;
	switch ( _objType )
	{
		case 2:
			obj = O_DROP;
			break;
		case 3:
			obj = O_BADY;
			break;
		case 4:
			obj = O_CUMULUS;
			break;
		case 5:
			obj = O_RADAR;
			break;
		case 6:
			obj = O_PHASER;
			break;
		case 9:
			obj = O_COLOR_CHANGE;
			break;
		default:
			obj = O_ROCKET;
	}
	placeInfo = _mode == PLACE_OBJECTS ? objects.find( obj )->name() : "";
	string info( _mode == EDIT_LANDSCAPE ? "EDIT" : ( "PLACE " + placeInfo ) );
	string n( _name );
	if( n.size() )
	{
		if ( n.size() > 40 )
		{
			n.erase( 0, n.size() - 40 );
			n.insert( 0, "..");
		}
//		os << n << ( _changed ? "*" : "" ) << "      ";
	}
	os << n << ( _changed ? "*" : "" ) << "      ";
	os << info << "      xpos: " << _xoff << " (Screen " << ( _xoff / w() ) + 1 << "/" << _ls->size() / w() << ")";
	if ( _mode == EDIT_LANDSCAPE && _scrollLock )
		os << "                SCROLL";
	t += " - " + os.str();
	copy_label( t.c_str() );
}

#ifdef WIN32
#include "win32_console.H"
#endif
int main( int argc_, const char *argv_[] )
//--------------------------------------------------------------------------
{
#ifdef WIN32
	Console console;	// output goes to command window (if started from there)
#endif
	LSEditor editor( argc_, argv_ );
	cout << "ready!" << endl;
	Fl::run();
	if ( editor.dont_save() || Fl::event_key() == FL_Escape )
		cout << "not saved." << endl;
	else
		editor.save();
}
