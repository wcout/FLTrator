//
// Fl_ZXAttrEffect
//
// Copyright 2016 Christian Grabner.
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
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <cstdlib>
#include <cstdarg>
#include <cmath>
#include <vector>

static Fl_RGB_Image *make_alpha_box( Fl_Color c_, int w_, int h_, uchar alpha_ )
{
	size_t size = 4 * w_ * h_;
	uchar *image = new uchar[size];
	uchar *p = image;
	uchar *e = p + size;
	uchar r, g, b;
	Fl::get_color( c_, r, g, b );
	while ( p < e )
	{
		*p++ = r;
		*p++ = g;
		*p++ = b;
		*p++ = alpha_;
	}
	Fl_RGB_Image *rgb = new Fl_RGB_Image( image, w_, h_, 4 );
	rgb->alloc_array = 1;
	return rgb;
}

class Fl_ZXAttrEffect : public Fl_Double_Window
{
	typedef Fl_Double_Window Inherited;
	static const double POLL_DELAY;
public:
	Fl_ZXAttrEffect( Fl_Window &baseWin_,
	                 double duration_ = 0.5/*sec*/ ) :
		Inherited( 0, 0, baseWin_.w(), baseWin_.h() ),
		_baseWin( &baseWin_ ),
		_max_count( ceil( duration_ / POLL_DELAY ) ),
		_screen( 0 ),
		_bgImage( 0 )
	{
		DBG( "Fl_ZXAttrEffect" );
		color( FL_BLACK );
		make_attr();

		_screen = fl_read_image( 0, 0, 0, baseWin_.w(), baseWin_.h() );
		if ( _screen )
		{
			Fl_RGB_Image *rgb = new Fl_RGB_Image( _screen, baseWin_.w(), baseWin_.h() );
			rgb->alloc_array = 1;
			_bgImage = rgb;
		}

		srandom( time( 0 ) );
		end();

		baseWin_.insert ( *this, 0 );
		DBG( "before show" );
		show();
		DBG( "after show" );

		Fl::add_timeout( POLL_DELAY, cb_poll, this );
	}
	~Fl_ZXAttrEffect()
	{
		for ( size_t i = 0; i < _attr.size(); i++ )
			delete _attr[i];
		_attr.clear();
		delete _bgImage;
		terminate();
	}
protected:
	virtual void draw()
	{
		Inherited::draw();
		if ( _bgImage )
			_bgImage->draw( 0, 0 );
		if ( _attr.empty() )
			return;
		int x = 0;
		int y = 0;
		while ( y < h() )
		{
			while ( x < w() )
			{
				int n = random() % _attr.size();
				_attr[n]->draw( x, y );
				x += _attr[n]->w();
			}
			y += _attr[0]->h();
			x = 0;
		}
	}
	int handle( int e_ )
	{
		DBG( "handle " << e_ << " key: 0x" << hex << Fl::event_key() << dec );
		return Inherited::handle( e_ );
	} // handle
private:
	void terminate()
	{
		DBG( "terminate" );
		hide();
		Fl::remove_timeout( cb_poll, this );
		if ( _baseWin )
		{
			_baseWin->remove( *this );
			_baseWin = 0;
		}
	}
	void onUpdate()
	{
		if ( --_max_count <= 0 )
		{
			terminate();
			return;
		}
		redraw();
	}
	static void cb_poll( void *d_ )
	{
		DBG( "cb_poll" );
		((Fl_ZXAttrEffect *)d_)->onUpdate();
		Fl::repeat_timeout( POLL_DELAY, cb_poll, d_ );
	}
	void make_attr()
	{
		static Fl_Color zx_colors[] = { FL_BLACK, FL_WHITE, FL_YELLOW, FL_RED, FL_BLUE, FL_CYAN, FL_MAGENTA, FL_GREEN };
		static const int nbrOfItems = sizeof( zx_colors ) / sizeof( zx_colors[0] );
		for ( int i = 0; i < nbrOfItems; i++ )
		{
			static const int W = _baseWin->w() / 32;
			static const int H = _baseWin->h() / 24;
			_attr.push_back( make_alpha_box( zx_colors[i], W, H, /*alpha=*/180  ) );
		}
	}
private:
	Fl_Window *_baseWin;
	int _max_count;
	std::vector<Fl_Image *> _attr;
	const uchar *_screen;
	Fl_Image *_bgImage;
};

/*static*/
const double Fl_ZXAttrEffect::POLL_DELAY = 0.1;
