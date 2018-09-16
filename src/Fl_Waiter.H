//
//  FLTK-Interface for a Hires-Timer under WIN32 platform.
//
//  Needed for timer rates above 64 Hz and for improved accuracy in general.
//  Also implemented for Linux (where normal FLTK timers are sufficient)
//  to have a uniform API across both platforms.
//
//  wcout 2018/03/30
//
//  This class was tested on a real WIN7 machine and ran very precise with
//  a frame rate of 256 Hz using about 25% CPU.
//
//  Usage example:
//
//    const int FPS = 200; // desired frame rate
//    static Fl_Waiter waiter;
//    waiter.FPS( FPS );
//    while ( waiter.wait() )
//    {
//       update(); // update UI/data
//    }
//
#ifndef __FL_WAITER_H__
#define __FL_WAITER_H__

#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <time.h>
#include <sys/time.h>
#include <unistd.h> // defines _POSIX_MONOTONIC_CLOCK (if available)
#endif

#include <FL/Fl.H>

//-------------------------------------------------------------------------------
class Fl_Waiter
//-------------------------------------------------------------------------------
{
#ifdef _WIN32
	#define FLTK_WAIT_DELAY 0
	#define LARGE_INT LARGE_INTEGER
#else
	// NOTE: wait delay 0 does not behave well (cpu usage goes up to 100%)!
	//       A very small delay value suffices to work around this.
	#define FLTK_WAIT_DELAY 0.0001
	#define LARGE_INT unsigned long
#endif

public:
	Fl_Waiter() :
		_elapsedMicroSeconds( 0 ),
		_fltkWaitDelay( FLTK_WAIT_DELAY ),
		_FPS( 40 ),
		_ready( true )
	{
#ifdef _WIN32
		_ready = QueryPerformanceFrequency( &_frequency ) != 0;
		QueryPerformanceCounter( &_startTime );
#else
#ifdef _POSIX_MONOTONIC_CLOCK
#ifdef LOG
		LOG( "Waiter: using clock_gettime()" );
#endif
		struct timespec ts;
		_ready = clock_gettime( CLOCK_MONOTONIC, &ts ) == 0;
		_startTime = ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
#else
#ifdef LOG
		LOG( "Waiter: using gettimeofday()" );
#endif
		struct timeval tv;
		_ready = gettimeofday( &tv, NULL ) == 0;
		_startTime = tv.tv_sec * 1000000 + tv.tv_usec;
#endif // _POSIX_MONOTONIC_CLOCK
#endif // _WIN32

		_endTime = _startTime;
		if ( !_ready )
		{
#ifdef LOG
			LOG( "Waiter: Could not initialise HiRes timer API" );
#endif
		}
	}

	unsigned int wait( unsigned int FPS_ = 0 )
	{
		unsigned int elapsedMicroSeconds = 0;
		unsigned int delayMicroSeconds = 1000000 / ( FPS_ ? FPS_ : _FPS );
		unsigned int fltkWaitDelayMicroSeconds = _fltkWaitDelay > 0 ? _fltkWaitDelay * 1000000 : 0;

		while ( elapsedMicroSeconds < delayMicroSeconds && Fl::first_window() )
		{
			double fltkWaitDelay = _fltkWaitDelay;
			if ( fltkWaitDelayMicroSeconds )
			{
				unsigned int remainMicroSeconds = delayMicroSeconds - elapsedMicroSeconds;
				if ( remainMicroSeconds < fltkWaitDelayMicroSeconds )
			   	fltkWaitDelay = (double)remainMicroSeconds / 1000000;
			}
			Fl::wait( fltkWaitDelay );
#ifdef _WIN32
			if ( !_ready ) // QueryPerformance API not available ==> don't wait
				break;

			QueryPerformanceCounter( &_endTime );
			LARGE_INT elapsedTime;
			elapsedTime.QuadPart = _endTime.QuadPart - _startTime.QuadPart;

			// convert to microseconds
			elapsedTime.QuadPart *= 1000000;
			elapsedTime.QuadPart /= _frequency.QuadPart;
			elapsedMicroSeconds = elapsedTime.QuadPart;
#else
#ifdef _POSIX_MONOTONIC_CLOCK
			struct timespec ts;
			clock_gettime( CLOCK_MONOTONIC, &ts );
			_endTime = ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
#else
			struct timeval tv;
			gettimeofday( &tv, NULL );
			_endTime = tv.tv_sec * 1000000 + tv.tv_usec;
#endif // _POSIX_MONOTONIC_CLOCK

			elapsedMicroSeconds = _endTime - _startTime;
#endif // _WIN32
		}

		_startTime = _endTime;
		_elapsedMicroSeconds = elapsedMicroSeconds;
		return _elapsedMicroSeconds;
	}

	unsigned int elapsedMicroSeconds() const { return _elapsedMicroSeconds; }
 	double fltkWaitDelay() const { return _fltkWaitDelay; }
	void fltkWaitDelay( double fltkWaitDelay_ ) { _fltkWaitDelay = fltkWaitDelay_; }
	unsigned int FPS() const { return _FPS; }
	void FPS( unsigned int FPS_ ) { _FPS = FPS_; }
	bool ready() const { return _ready; }

private:
	LARGE_INT _startTime;
	LARGE_INT _endTime;
#ifdef _WIN32
	LARGE_INT _frequency;
#endif
	unsigned int _elapsedMicroSeconds;
	double _fltkWaitDelay;
	unsigned int _FPS;
	bool _ready;
};

#endif // __FL_WAITER_H__
