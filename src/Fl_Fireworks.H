//
// Fl_Fireworks
//
// Copyright 2015-2023 Christian Grabner.
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
#include <ctime> // time()
#include <vector>

class Fl_Fireworks : public Fl_Double_Window
{
	typedef Fl_Double_Window Inherited;

public:
	static double POLL_DELAY;
	static double REDRAW_DELAY;

private:

	class Fl_Firework_Particle
	{
	public:
			Fl_Firework_Particle( double end_r_, double angle_,
			                      double speed_, Fl_Color color_ = FL_WHITE,
			                      unsigned len_ = 10, bool multicolor_ = false ) :
				start_r( 1. ), end_r( end_r_ ), angle( angle_ ),
				speed( speed_ ), color( color_ ), len( len_ ), multicolor( multicolor_ ),
				valid( false ), dot( false ),
				zf( double( 1 + random() % 1000 ) / 1000. ) // simple z-axis simulation
			{
				r = start_r + random() % std::min( len, 5 );
				dot = len <= 5;
				DBG( "particle " << end_r << ", " << angle << ", " <<  speed );
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
			double zf;
	};

	class Fl_Firework_Rocket : public Fl_Box
	{
		typedef Fl_Box Inherited;
	public:
		Fl_Firework_Rocket( int x_, int y_, int w_, int h_, Fl_Color c_ ) :
			Inherited( x_, y_, w_, h_ ),
			_r( 0 ),
			_X( -1 ),
			_Y( -1 ),
			_exploded( false ),
			_started( false ),
			_explosion_type( - 1 ),
			_done( false )
		{
			DBG( "Fl_Firework_Rocket " <<  this );
			box( FL_NO_BOX );
			color( c_ );
			_explode_color = explode_color( c_ );
			init();
		}
		void update()
		{
			DBG( "Fl_Firework_Rocket::update() " << this  << " r="<<  _r << " (" <<  _explosion_radius << ")" );
			if ( y() >= -h() && !_started )
			{
				_started = true;
				window()->do_callback( window(), 1 );
			}
			y( y() + 2 * _dy );
			if ( y() > _explode_h )	// explode height reached?
			{
				if ( _r % 8 == 0 && _r <= 64 && !_exploded )
				{
					explode( _explosion_type );
					((Fl_Fireworks *)window())->explodeColor( _explode_color );
					if ( !_r )
						window()->do_callback( window(), 2 );
				}
				if ( _r < _explosion_radius )
					_r += 4;
				else
					_exploded = true;
			}
			if ( _exploded && _done )
			{
				init();
			}
			x( x() + 2 *_dx );
			if ( x() < 0 )
				x( parent()->w() );
			if ( x() > parent()->w() )
				x( 0 );
			update_particles( _X + w() / 2, parent()->h() - _Y + _r / 3, _particles );
			_done = _exploded && _particles.empty();
		} // update
	bool done() const { return _done; }
	private:
		static Fl_Color explode_color( Fl_Color color_ )
		{
			unsigned char r, g, b;
			Fl::get_color( color_, r, g, b );
			while ( r < 220 && g < 220 && b < 220 )
			{
				color_ = fl_lighter( color_ );
				Fl::get_color( color_, r, g, b );
			}
			return color_;
		}
		static bool update_particles( double center_x_, double center_y_, std::vector<Fl_Firework_Particle>& p_ )
		{
			DBG( "ENTER update_particles " << p_.size() );
			bool done( true );
			for ( size_t i = 0; i < p_.size(); i++ )
			{
				Fl_Firework_Particle& p = p_[i];
				if ( p.r < p.end_r && p.speed > 0.4 )
				{
					p.beg_x = p.zf * ( p.r * cos( p.angle * M_PI / 180.0 ) ) + center_x_;
					p.beg_y = p.zf * ( p.r * sin( p.angle * M_PI / 180.0 ) ) + center_y_;
					double f = ( p.end_r - p.r ) / ( p.end_r - p.start_r );
					int mask = 256. * f;
					if ( !mask )
						mask++;
					if ( p.multicolor )
						p.curr_color = fl_rgb_color( random() % mask, random() % mask, random() % mask );
					else
						p.curr_color = p.color;

					unsigned len = (double)p.len * f;
					p.end_x = p.zf * ( ( p.r + len ) * cos( p.angle * M_PI / 180.0 ) ) + center_x_;
					p.end_y = p.zf * ( ( p.r + len ) * sin( p.angle * M_PI / 180.0 ) ) + center_y_;

					p.valid = true;
					if ( p.r < p.end_r / 2 )
					{
						if ( p.speed > 0.4 )
							p.speed += p.dot ? -0.2 : 0.2;
					}
					else
					{
						if ( p.speed > 0.4 )
							p.speed -= p.dot ? -0.4 : 0.2;
					}
					p.r += p.speed;
					done = false;
				}
				else
				{
					p_.erase( p_.begin() + i );
					i--;
				}
			}
			DBG( "LEAVE update_particles " <<  p_.size() );
			return done;
		}
		void draw_particles()
		{
			for ( size_t i = 0; i < _particles.size(); i++ )
			{
				Fl_Firework_Particle& p = _particles[i];
				if ( !p.valid )
					continue;

				fl_color( p.curr_color );

				if ( p.dot )
				{
					fl_pie( p.beg_x, p.beg_y, ceil( scale_y() * p.len ),
				           ceil( scale_y() * p.len ), 0., 360. );
				}
				else
				{
					fl_line_style( FL_SOLID, ceil( scale_y() * 2 ) );
					fl_line( p.beg_x, p.beg_y, p.end_x, p.end_y );
					fl_line_style( 0 );
				}
			}
		}
		void draw()
		{
			DBG( "Fl_Firework_Rocket::draw() " << this << " r=" << _r );
			if ( _r < 10 )
			{
				double sx( scale_x() < 0.6 ? 0.6 : scale_x() > 1.5 ? 1.5 : scale_x() );
				double sy( scale_y() < 0.6 ? 0.6 : scale_y() > 1.5 ? 1.5 : scale_y() );
				fl_push_matrix();
				fl_translate( x(), parent()->h() - y() );
				fl_rotate( ( 90. - _angle ) + 270. );
				fl_color( color() );
				fl_begin_polygon();

				fl_vertex( sx * ( w() / 2 ), 0 );
				fl_vertex( 0, sy * h() );
				fl_vertex( sx * w(), sy * h()  );

				fl_end_polygon();
				fl_pop_matrix();
			}
			else if ( _X == -1 )
			{
				_X = x();
				_Y = y();
			}
			else
			{
				// draw explosion
				draw_particles();
			}
		} // draw
		void init()
		{
			_particles.clear();
			if ( y() > -h() )
			{
				int sh = 20 * scale_y();
				y( -h() * ( ( random() % sh ) + sh ) );
			}
			_r = 0;
			_X = -1;
			_Y = -1;
			_dy = ( random() % 3 ) + 3;	// 3-5
			_dx = ( random() % ( _dy - 2 ) ) + 1;
			_dy = ceil( scale_y() * _dy );
			_dx = ceil( scale_x() * _dx );
			_explosion_radius = scale_y() * h() * ( 6 + random() % 4 );
			_exploded = false;
			_started = false;
			_done = false;
			_multicolor = h() >= 50;
			_explode_h = parent()->h() - parent()->h() / 5 - random() % ( parent()->h() / 4 );
			double r = atan( (double)_dx / (double)_dy ) * 180. / M_PI;
			_angle = r;
			DBG( this << " _explode_h: " << _explode_h << " _angle: " << _angle
		             << ", dx: " << _dx << " dy: " << _dy );
		}
		void explode( int explosion_type_ = -1 )
		{
			_explosion_type = explosion_type_ < 0 ? ( random() % 4 == 0 ) : explosion_type_;
			bool dot = _explosion_type == 1;
			int particles = explosion_type_ <  0 ? ( w() * h() * ( 2 * dot + 1 ) ) / 16 :
				( w() * h() * ( 2 * dot + 1 ) ) / 32;
			DBG( "create " << particles << " particles, dot=" <<  dot );
			for ( int i = 0; i < particles; i++ )
			{
				int speed = dot ? random() % 10 + 1 : random() % 5 + 3;
				_particles.push_back( Fl_Firework_Particle( _explosion_radius * ( dot + 1 ),
					random() % 360,
					speed,
					_explode_color,
					( dot ? random() % 5 + 1 : speed * 10 ), _multicolor ) );
			}
		}
		double scale_x() const { return ((Fl_Fireworks *)window())->scale_x(); }
		double scale_y() const { return ((Fl_Fireworks *)window())->scale_y(); }

	private:
		int _r;
		int _dx;
		int _dy;
		double _angle;
		int _X, _Y;
		int _explode_h;
		int _explosion_radius;
		Fl_Color _explode_color;
		bool _exploded;
		bool _multicolor;
		bool _started;
		std::vector <Fl_Firework_Particle> _particles;
		int _explosion_type;
		bool _done;
	};

public:
	Fl_Fireworks( Fl_Window &baseWin_, double duration_ = 10.0, size_t anz_ = 8 ) :
		Inherited( 0, 0, baseWin_.w(), baseWin_.h() ),
		_baseWin( &baseWin_ ),
		_max_count( ceil( duration_ / POLL_DELAY ) )
	{
		DBG( "Fl_Fireworks" );
		color( FL_BLACK );

		srandom( time( 0 ) );
		for ( size_t i = 0; i < anz_; i++ )
		{
			int W = 12 + random() % 10;
			int H = W + 20 + random() % 20;
			Fl_Color c = fl_rgb_color( random() % 200 + 56,
				random() % 200 + 56,
				random() % 200 + 56 );
			int sh = -H * random() % 3;
			_rockets.push_back( new Fl_Firework_Rocket( (i + 1) * w() / anz_, sh, W, H, c ) );
		}
		end();

		baseWin_.insert ( *this, 0 );
		DBG( "before show" );
		show();
		DBG( "after show" );

		Fl::add_timeout( 0.1, cb_poll, this );
		if ( REDRAW_DELAY )
			Fl::add_timeout( 0.1, cb_redraw, this );
	}
	void explodeColor( Fl_Color c_ ) { _explodeColor = c_; }
	Fl_Color explodeColor() const { return _explodeColor; }
	~Fl_Fireworks()
	{
		terminate();
	}
protected:
	virtual double scale_x() const { return 1.; }
	virtual double scale_y() const { return 1.; }
	void onUpdate()
	{
		if ( --_max_count <= 0 )
		{
			terminate();
			return;
		}
		do_callback( this, -1 );
		for ( size_t i = 0; i < _rockets.size(); i++ )
			_rockets[i]->update();
		if ( !REDRAW_DELAY ) redraw();
	}
	int handle( int e_ )
	{
		DBG( "handle " << e_ << " key: 0x" << std::hex << Fl::event_key()  << std::dec );
		if ( e_ == FL_FOCUS || e_ == FL_UNFOCUS )
			return 1;
		if ( e_ == FL_KEYDOWN || e_ == FL_SHORTCUT || e_ == FL_RELEASE )
		{
			if ( Fl::event_key( FL_Escape ) && _baseWin )
				_baseWin->hide();	// exit whole application
			if ( Fl::event_key( ' ' ) || e_ == FL_RELEASE )
			{
				terminate();	// exit fireworks
				return 1;
			}
		}
		return Inherited::handle( e_ );
	} // handle
private:
	void terminate()
	{
		DBG( "terminate" );
		hide();
		Fl::remove_timeout( cb_poll, this );
		Fl::remove_timeout( cb_redraw, this );
		for ( size_t i = 0; i < _rockets.size(); i++ )
			delete _rockets[i];
		_rockets.clear();
		if ( _baseWin )
		{
			_baseWin->remove( *this );
			_baseWin = 0;
		}
	}
	static void cb_poll( void *d_ )
	{
		DBG( "cb_poll" );
		((Fl_Fireworks *)d_)->onUpdate();
		Fl::repeat_timeout( POLL_DELAY, cb_poll, d_ );
	}
	static void cb_redraw( void *d_ )
	{
		DBG( "cb_redraw" );
		((Fl_Fireworks *)d_)->redraw();
		Fl::repeat_timeout( REDRAW_DELAY, cb_redraw, d_ );
	}
private:
	std::vector <Fl_Firework_Rocket *> _rockets;
	Fl_Window *_baseWin;
	int _max_count;
	Fl_Color _explodeColor;
};

/*static*/
double Fl_Fireworks::POLL_DELAY = 0.02;
/*static*/
double Fl_Fireworks::REDRAW_DELAY = 0.05;
