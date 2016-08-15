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
#ifndef __FL_JOYSTICK_H__
#define __FL_JOYSTICK_H__

#include "joystick.H"

// Events genereted
#define FL_JOY_BUTTON_DOWN 9000
#define FL_JOY_BUTTON_UP   9001
#define FL_JOY_AXIS        9002

class Fl_Joystick : public Joystick
{
typedef Joystick Inherited;
public:
	Fl_Joystick();
	~Fl_Joystick();
	bool attach( const char *device_ = NULL );
	bool isAttached() const { return fd() >= 0; }
	bool detach();
	virtual void onEvent();
private:
	static void cb_data_ready( int fd_, void *d_ );
};

#endif // __FL_JOYSTICK_H__
