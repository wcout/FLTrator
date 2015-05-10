#include<stdio.h>
#ifdef WIN32
int main() { return -1; }
#else
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<X11/Xlib.h>
#include<X11/extensions/Xrandr.h>

Display                 *dpy;
Window                  root;
int                     num_sizes;
XRRScreenSize           *xrrs;
XRRScreenConfiguration  *conf;
short                   possible_frequencies[64][64];
short                   original_rate;
Rotation                original_rotation;
SizeID                  original_size_id;

int main(int argc, char *argv[])
{
	//
	//     CONNECT TO X-SERVER, GET ROOT WINDOW ID
	//
	dpy    = XOpenDisplay(NULL);
	root   = RootWindow(dpy, 0);
	//
	//     GET POSSIBLE SCREEN RESOLUTIONS
	//
	xrrs   = XRRSizes(dpy, 0, &num_sizes);
	//
	//     LOOP THROUGH ALL POSSIBLE RESOLUTIONS,
	//     GETTING THE SELECTABLE DISPLAY FREQUENCIES
	//
	for (int i = 0; i < num_sizes; i ++)
	{
		short   *rates;
		int     num_rates;

		printf("\n\t%2i : %4i x %4i   (%4imm x%4imm ) ", i, xrrs[i].width, xrrs[i].height, xrrs[i].mwidth, xrrs[i].mheight);

		rates = XRRRates(dpy, 0, i, &num_rates);

		for(int j = 0; j < num_rates; j ++) {
			possible_frequencies[i][j] = rates[j];
			printf("%4i ", rates[j]);
		}
	}

	printf("\n");
	//
	//     GET CURRENT RESOLUTION AND FREQUENCY
	//
	conf                   = XRRGetScreenInfo(dpy, root);
	original_rate          = XRRConfigCurrentRate(conf);
	original_size_id       = XRRConfigCurrentConfiguration(conf, &original_rotation);

	printf("\n\tCURRENT SIZE ID  : %i\n", original_size_id);
	printf("\tCURRENT ROTATION : %i \n", original_rotation);
	printf("\tCURRENT RATE     : %i Hz\n\n", original_rate);

	//
	//     CHANGE RESOLUTION
	//
	if ( argc > 1 )
	{
		int mode = -1;
		// given by WxH ?
		const char *del = strchr( argv[1], 'x' );
		if ( del )
		{
			// yes, search modes
			int width = atoi( argv[1] );
			int height = atoi( del + 1 );
			for ( int i = 0; i < num_sizes; i++ )
			{
				if ( xrrs[i].width == width && xrrs[i].height == height )
				{
					printf( "mode %i\n", i );
					mode = i;
					break;
				}
			}
		}
		else
		{
			// given by mode
			mode = atoi( argv[1] );
		}
		if ( mode >= 0 && mode < num_sizes )
		{
			printf("\tCHANGED TO %i x %i PIXELS, %i Hz\n\n", xrrs[mode].width, xrrs[mode].height, possible_frequencies[mode][0]);
			XRRSetScreenConfigAndRate(dpy, conf, root, mode, RR_Rotate_0, possible_frequencies[mode][0], CurrentTime);
		}
	}

#if 0
	{
		//
		//     SLEEP A WHILE
		//
		usleep(6000000);
		//
		//     RESTORE ORIGINAL CONFIGURATION
		//
		printf("\tRESTORING %i x %i PIXELS, %i Hz\n\n", xrrs[original_size_id].width, xrrs[original_size_id].height, original_rate);
		XRRSetScreenConfigAndRate(dpy, conf, root, original_size_id, original_rotation, original_rate, CurrentTime);
	}
#endif
	//
	//     EXIT
	//
	XCloseDisplay(dpy);
	exit( original_size_id );
}
//
//      gcc -o xrandr_change_screen_res xrandr_change_screen_res.cxx -lX11 -lXrandr -lstdc++
//
#endif
