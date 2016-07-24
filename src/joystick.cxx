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
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>

#include "joystick.H"

Joystick::Joystick() :
	_fd( -1 ),
	_numberOfButtons( 0 ),
	_numberOfAxis( 0 ),
	_x_axis( 0 ),
	_y_axis( 1 )
//-------------------------------------------------------------------------------
{
}

Joystick::~Joystick()
//-------------------------------------------------------------------------------
{
	close();
}

int Joystick::open( const char *device_/* = NULL*/ )
//-------------------------------------------------------------------------------
{
#if defined __linux__
	if ( _fd < 0 )
	{
		std::string device( device_ ? device_ : JOYSTICK1_DEVICE_NAME );
		_fd = ::open( device.c_str(), O_RDONLY | O_NONBLOCK );
		if ( _fd >= 0 )
			_device = device;
	}
#endif
	return _fd;
}

void Joystick::close()
//-------------------------------------------------------------------------------
{
	if ( _fd >= 0 )
		::close( _fd );
	_fd = -1;
	_device.erase();
}

int Joystick::update()
//-------------------------------------------------------------------------------
{
	int bytes = read( _fd, &_event, sizeof( _event ) );
	if ( bytes == sizeof( _event ) )
	{
		storeStatus();
		if ( ( _event.type & ~JS_EVENT_INIT ) == _event.type )
			onEvent();
		return 1;
	}

	return -1;
}

void Joystick::storeStatus()
//-------------------------------------------------------------------------------
{
	Event& jse = _event;
	// type:  1 = button
	//        2 = axis
	// value: 0/1 for button up/down
	//        -32767-32768 axis value
	// number: 0...n #button (for type 1)
	//         0=x-axis (left/right)
	//         1=y-axis (up/down)
	//         2=z-axis
	if ( jse.type == JS_EVENT_BUTTON )
	{
		unsigned int button = jse.number;
		if ( button < JS_MAX_BUTTONS )
			_state.button[button] = jse.value;
	}
	else if ( jse.type == JS_EVENT_AXIS )
	{
		unsigned int axis = jse.number;
		if ( axis < JS_MAX_AXIS )
			_state.axis[axis] = jse.value;
	}
	// try to figure out real number of buttons/axis
	else if ( jse.type == ( JS_EVENT_BUTTON | JS_EVENT_INIT ) )
	{
		unsigned int button = jse.number;
		if ( button < JS_MAX_BUTTONS )
		{
			if ( button + 1 > _numberOfButtons )
				_numberOfButtons = button + 1;
		}
	}
	else if ( jse.type == ( JS_EVENT_AXIS | JS_EVENT_INIT ) )
	{
		unsigned int axis = jse.number;
		if ( axis < JS_MAX_AXIS )
		{
			if ( axis + 1 > _numberOfAxis )
				_numberOfAxis = axis + 1;
		}
	}
}
