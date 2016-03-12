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
#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

#define JOYSTICK1_DEVICE_NAME "/dev/input/js0"

#define JS_EVENT_BUTTON         0x01    /* button pressed/released */
#define JS_EVENT_AXIS           0x02    /* joystick moved */
#define JS_EVENT_INIT           0x80    /* initial state of device */

#define JS_MAX_BUTTONS          10
#define JS_MAX_AXIS             6

#define AXIS_THRESHOLD          8192

#include <string>

class Joystick
{
public:

	struct Event
	{
		Event() : time( 0 ), value( 0 ), type( 0 ), number( 0 ) {}
		unsigned int time;    /* event timestamp in ms */
		short value;          /* value */
		unsigned char type;   /* event type */
		unsigned char number; /* #button or #axis 0... */
	};

	struct State
	{
		State()
		{
			for ( int i = 0; i < JS_MAX_BUTTONS; i++ )
				button[i] = 0;
			for ( int i = 0; i < JS_MAX_AXIS; i++ )
				axis[i] = 0;
		}
		int button[ JS_MAX_BUTTONS ];
		int axis[ JS_MAX_AXIS];
	};

	Joystick();
	~Joystick();
	int open(  const std::string& device_ = JOYSTICK1_DEVICE_NAME );
	void close();
	virtual void onEvent() {}
	int update();
	bool left() const { return state().axis[_x_axis] < -AXIS_THRESHOLD; }
	bool right() const { return state().axis[_x_axis] > AXIS_THRESHOLD; }
	bool up() const { return state().axis[_y_axis] < -AXIS_THRESHOLD; }
	bool down() const { return state().axis[_y_axis] > AXIS_THRESHOLD; }
	const State& state() const { return _state; }
	const Event& event() const { return _event; }
	const string device() const { return _device; }
	const unsigned int numberOfButtons() const
		{ return _numberOfButtons ? _numberOfButtons : JS_MAX_BUTTONS; }
	const unsigned int numberOfAxis() const
		{ return _numberOfAxis ? _numberOfAxis : JS_MAX_AXIS; }
	void x_axis( unsigned int x_axis_ ) { _x_axis = x_axis_; }
	void y_axis( unsigned int y_axis_ ) { _x_axis = y_axis_; }
	int fd() const { return _fd; }
private:
	void storeStatus();
private:
	std::string _device;
	int _fd;
	State _state;
	Event _event;
	unsigned int _numberOfButtons;
	unsigned int _numberOfAxis;
	unsigned int _x_axis;
	unsigned int _y_axis;
};

#endif // __JOYSTICK_H__
