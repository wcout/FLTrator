//
// Fireworks demo
//
// Copyright 2023 Christian Grabner.
//
//
// *Not* official part of FLTrator. Only here as example, how to use the
// 'Fl_Fireworks' widget from Fl_Fireworks.H.
//
// Needs to be compiled manually ('make fireworks' or use fltk-config).
//
#include <iostream>
//#define DBG(a) std::cout << a << std::endl
#define DBG(a)

// The 'Fl_Fireworks' widget
#include "Fl_Fireworks.H"

#include <cstdlib>
#include <sstream>
#include <string>

static double Scale = 1.;

static bool playSound( const char *wav_ )
{
	int ret = 0;
#ifndef WIN32
	std::ostringstream os;
	os << "aplay -q -N " << wav_ << " &";
	ret = system( os.str().c_str() );
#endif
	return ret == 0;
}

//-------------------------------------------------------------------------------
class Fireworks : public Fl_Fireworks
//-------------------------------------------------------------------------------
{
typedef Fl_Fireworks Inherited;
public:
	Fireworks( Fl_Window& win_ ) : Inherited( win_, 120./*seconds*/ )
	{
		_orig_bg = color();
		callback( cb_fireworks );
	}
	static void cb_fireworks( Fl_Widget *wgt_, long d_ )
	{
		Fireworks *f = (Fireworks *)wgt_;
		switch ( d_ )
		{
			case -1:
				// onUpdate() called - do own updates
				f->color(f->_orig_bg); // reset color
				break;
			case 1:
				playSound( "wav/firework_start.wav" );
				break;
			case 2:
			{
				f->color(fl_lighter(f->_orig_bg)); // "flash"
				playSound( "wav/firework_explode.wav" );
				break;
			}
		}
	}
	virtual double scale_x() const { return Scale; }
	virtual double scale_y() const { return Scale; }
private:
	Fl_Color _orig_bg;
};

#include <FL/Fl_Double_Window.H>
#include <FL/Fl.H>

//-------------------------------------------------------------------------------
int main( int argc_, const char *argv_[] )
//-------------------------------------------------------------------------------
{
	Fl_Double_Window win( 800, 600, "Fireworks demo" );
	win.resizable(win);
	win.show();
	for ( int i = 1; i < argc_; i++ )
	{
		std::string arg( argv_[i] );
		if ( arg.find( 'f' ) != std::string::npos )
			win.fullscreen();
		if ( arg.find( 's' ) != std::string::npos )
			Scale++;
	}
	Fireworks fireworks( win  );
	while (win.shown() && win.children())
		Fl::wait();
	return 0;
}
