#ifdef WIN32
#include <cstdio>
static DEVMODE devPrevMode;
static BOOL    bPrevSet = FALSE;

BOOL SetScreenResolution(INT nWidth, INT nHeight)
{
	DBG( "SetScreenResolution " << nWidth << " x " << nHeight );
	DEVMODE devMode;

	ZeroMemory(&devMode, sizeof(DEVMODE));
	devMode.dmSize = sizeof(DEVMODE);

	if(!EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode))
		return FALSE;

	CopyMemory(&devPrevMode, &devMode, sizeof(DEVMODE));
	DBG( "prevMode: " <<  devPrevMode.dmPelsWidth << " x " << devPrevMode.dmPelsHeight );
	devMode.dmPelsHeight = nHeight;
	bPrevSet             = TRUE;
	devMode.dmPelsWidth  = nWidth;
	devMode.dmPelsHeight = nHeight;
	return (ChangeDisplaySettingsEx(NULL, &devMode, NULL, 0, NULL) == DISP_CHANGE_SUCCESSFUL);
}

BOOL RestoreScreenResolution(VOID)
{
	if(bPrevSet)
	{
		DBG( "RestoreScreenResolution" );
		if (ChangeDisplaySettingsEx(NULL, &devPrevMode, NULL, 0, NULL) == DISP_CHANGE_SUCCESSFUL)
		{
			bPrevSet = FALSE;
			return TRUE;
		}
	}
	return FALSE;
}
#elif HAVE_XRANDR
#include <cstdlib>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
static int prevMode = -1;

static bool xrandr_set_screen( int mode_, int W_ = 0 , int H_ = 0 )
{
	bool success = false;

	// CONNECT TO X-SERVER, GET ROOT WINDOW ID
	Display *dpy  = XOpenDisplay( NULL );
	if ( !dpy )
		return false;
	Window root = RootWindow( dpy, 0 );
	if ( !root )
		return false;

	// GET POSSIBLE SCREEN RESOLUTIONS
	int num_sizes = 0;
	XRRScreenSize *xrrs = XRRSizes( dpy, 0, &num_sizes );

	// LOOP THROUGH ALL POSSIBLE RESOLUTIONS,
	// GETTING THE SELECTABLE DISPLAY FREQUENCIES
	short possible_frequencies[64][64];
	for ( int i = 0; i < num_sizes; i++ )
	{
		short *rates;
		int num_rates;

		rates = XRRRates( dpy, 0, i, &num_rates );
		for ( int j = 0; j < num_rates; j++ )
		{
			possible_frequencies[i][j] = rates[j];
		}
	}

	// GET CURRENT RESOLUTION AND FREQUENCY
	XRRScreenConfiguration *conf = XRRGetScreenInfo( dpy, root );
	if ( !conf )
		return false;

	Rotation original_rotation;
	SizeID original_size_id = XRRConfigCurrentConfiguration( conf, &original_rotation );
	if (prevMode < 0 )
		prevMode = (int)original_size_id;

	// CHANGE RESOLUTION
	int mode( mode_ );
	if ( mode < 0 && W_ && H_ )
	{
		for ( int i = 0; i < num_sizes; i++ )
		{
			if ( xrrs[i].width == W_ && xrrs[i].height == H_ )
			{
				mode = i;
				break;
			}
		}
	}
	if ( mode >= 0 && mode < num_sizes )
	{
		success = !XRRSetScreenConfigAndRate( dpy, conf, root, mode, RR_Rotate_0,
			possible_frequencies[mode][0], CurrentTime );
	}

	// EXIT
	XRRFreeScreenConfigInfo( conf );
	XCloseDisplay( dpy );
	return success;
}
#endif // HAVE_XRANDR

static bool setScreenResolution( int W_, int H_ )
{
#ifdef WIN32
	return SetScreenResolution( W_, H_ );
#elif HAVE_XRANDR
	return xrandr_set_screen( -1, W_, H_ );
#else
	return false;
#endif
}

static bool restoreScreenResolution()
{
#ifdef WIN32
	return RestoreScreenResolution();
#elif HAVE_XRANDR
	if ( prevMode != -1 )
	{
		bool success = xrandr_set_screen( prevMode );
		if ( success )
			prevMode = -1;
		return success;
	}
	return false;
#else
	return false;
#endif
}
