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
#include "fl_joystick.H"

#include <FL/Fl.H>

//-------------------------------------------------------------------------------
// class Fl_Joystick : public Joystick
//-------------------------------------------------------------------------------
Fl_Joystick::Fl_Joystick()
{
#if 0
	open();
	if ( fd() >= 0 )
		Fl::add_fd( fd(), FL_READ, cb_data_ready, this );
#endif
}

bool Fl_Joystick::attach( const char *device_/* = NULL*/ )
//-------------------------------------------------------------------------------
{
	if ( fd() < 0 )
	{
		open( device_ );
		if ( fd() >= 0 )
			Fl::add_fd( fd(), FL_READ, cb_data_ready, this );
	}
	return fd() >= 0;
}

bool Fl_Joystick::detach()
//-------------------------------------------------------------------------------
{
	// NOTE: currently only one device can be open
	if ( fd() >= 0 )
	{
		Fl::remove_fd( fd() );
		close();
	}
	return fd() < 0;
}

/*virtual*/
void Fl_Joystick::onEvent()
//-------------------------------------------------------------------------------
{
	const Joystick::Event& jse = Inherited::event();
	int event( 0 );
	if ( jse.type == JS_EVENT_BUTTON )
		event = jse.value ? FL_JOY_BUTTON_DOWN : FL_JOY_BUTTON_UP;
	else if ( jse.type == JS_EVENT_AXIS )
		event = FL_JOY_AXIS;
	if ( event )
	{
		Fl_Widget *focus = Fl::focus();
		if ( focus )
			focus->handle( event );
	}
}

/*static*/
void Fl_Joystick::cb_data_ready( int fd_, void *d_ )
//-------------------------------------------------------------------------------
{
	((Fl_Joystick *)d_)->update();
}

Fl_Joystick::~Fl_Joystick()
//-------------------------------------------------------------------------------
{
	if ( fd() >= 0 )
		Fl::remove_fd( fd(), FL_READ );
}
