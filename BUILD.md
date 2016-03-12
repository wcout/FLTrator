FLTrator Build/Installation notes
=================================

I developed this game on Linux (Ubuntu 14.04 64 bit).

It is written with **FLTK**, and has **no other dependencies**.

To compile it, you first need FLTK 1.3 (I recommend to use at least version 1.3.3)
or above.

As I have no access to and no knowledge about Apple OSX I can only assume
that the code should compile and run on this platform (as FLTK works well on OSX).

I have successfully tested and run a Mingw compiled version of the program
under Windows XP and Windows 7.

Under Linux the program was tested with various distros, also some older ones.

General
=======

One main goal at development was to use *NO* external libraries whatsoever
(except FLTK of course) to stay simple and clean. So the code should be
really easy to compile.

As this program consists only of one source file/per executable, you could
compile it just with `fltk-config`:

    fltk-config --use-images --compile src/fltrator.cxx

    fltk-config --use-images --compile src/fltrator-landscape.cxx

The excutable can be run from the build folder, no need for an install.

Nevertheless there is a `Makefile` in order to be more flexible.

---

Audio
=====

According to the goal to use no external libraries, the program plays `.wav`
files by using the builtin play tool of the OS. This is `aplay` for Linux
and `play` for Apple (actually I don't know for sure).
Works pretty well - at least on Linux...

If you don't hear sound please make sure, that this program is installed
on your system and plays sounds from the command line.

Under Windows there is no such tool and one cannot output multiple sounds in
background easy (I stayed with the old Windows MM-API - PlaySound(), because
everything else seemed complete overkill). So I made a little executable
`playsound.exe` that is called by fltrator.
It has to be in the path somewhere (or in the same path as fltrator).

It is also possible to specify a play command by command line option (see `--help`).

Under Linux you can also use your own sound-player, when you stick to the
outline given in playsound.cxx.

---

Building under Linux (this should also work for Mac)
====================================================

Either fetch FLTK from [www.fltk.org](http://www.fltk.org/software.php) and compile it yourself or install the
FLTK development package of your distribution (if it is at version 1.3 or higher).

Use FLTK from distribution:
---------------------------

This is the default.

In a terminal change to the fltrator directory and issue:

    ./configure
    make
    make install (optional)


Use downloaded FLTK:
--------------------

Say you downloaded it to `~/fltk-1.3/`:

First configure and compile FLTK.

    cd ~/fltk-1.3

Configure and compile it, usually you only need:

    make

If that succeeds go to the fltrator directory and

    ./configure --with-fltk-dir=~/fltk-1.3
    make
    make install (optional)


*NOTE*: `make install` copies to `~/bin`. Make sure that you have included this
      path in your search path.


Running fullscreen
------------------

Running FLTrator in fullscreen on a big monitor is really a great pleasure!
It also runs faster and more smooth I believe.

So I dug out some instructions how to change resolution under Linux using Xrandr
and and also for Windows and integrated it everything into the program.

Note for Linux: For fullsceeen to work you must have the `libxrandr-dev` package
installed.

[Tested to work under Ubuntu 12.04 and 14.04, Windows 7]

---

General notes about the Windows version
=======================================

As noted already, I am no Windows specialist. However I successfully run some of my
programs written for Linux crosscompiled with 'mingw' under Windows.

FLTrator normally would like to use a 200Hz timer for scrolling the landscape.
This is just the right speed for a smooth scrolling by 1 pixel/frame. No problem
under **ANY** Linux system. But a **BIG** problem for a Windows system, as far as I
experience.

The problem is twofold: FLTK's timer for Windows are restricted to a maximum rate
of ~65 fps 'by design'. So it was necessary to implement an own timer handling
using Windows hires timers and integrate these with FLTK's main loop.

The second problem is, that Windows hires timers seem to be not very reliable
(but maybe it's just my lack of knowledge, how to set up things right).

So the Windows version by default just uses 100 FPS using Windows hi-res timers.
That seems to work...

If you have still performance problems with it, I found 40 FPS to be the ideal
rate in this case. Therefore I have added an option `HAVE_SLOW_CPU` to compile
with a 40Hz timer (using FLTK timers). This makes the scrolling a little jumpy,
but maybe this even adds to the retro feeling...

You can enable the switch in your compile shell with:

    export HAVE_SLOW_CPU=1

before running

     make -e

**Note**: With FLTrator 1.5 there is a runtime switch to change the frame rate.
      See section `Performance issues` below.

Creating a Windows executable using mingw on Linux
==================================================

This requires that you have the mingw development package of your distribution installed.

Prepare a FLTK version compiled with mingw:
-------------------------------------------

Download FLTK into some directory say: `~/fltk-1.3-mingw/`:

    cd ~/fltk-1.3-mingw

    make clean
    ./configure --host=i686-w64-mingw32 --enable-cygwin --enable-localjpeg  --enable-localpng --enable-localzlib

*Note*: Use the mingw host you have installed on your machine in the `--host=...` switch
        (The above was mine under Ubuntu 14.04).

The compilation may stop (at least it did until recently) somewhere within fluid.
But this doesn't matter, as the FLTK libraries are built already at this point.

After successful build of FLTK change to `mingw` subfolder of your fltrator folder.

    cd {your-fltrator-folder}
    cd mingw

Edit the Makefile to point FLTK_DIR to your previous built FLTK-mingw or use
the export override and the same commands as above to build the Windows executables.

If that succeeds:

    make install

will then copy all needed files to the INSTALL_PATH  specified in the Makefile (default: ~/Downloads/fltrator).
You can copy them from there to your Windows machine.


Native builds under Windows
===========================

I didn't try it, but if you have a working development suite, I guess it should be very
easy to build as you need only single file projects.

---

Performance issues
==================

On older hardware or on a Windows system FLTrator's default framerate of 200 fps
may be too high.

With version 1.5 the frame rate can be changed in runtime:

On the **title screen** you see the current frame rate in the bottom/right corner.
Pressing '-' or '+' will change to the next value down or up.

Experiment with the values, until you get the best mix of performance vs  'jumpyness'.
Allowed values are 20, 25, 40, 50, 100, 200. I recommend a rate 40+.

Of course lower values will decrease the smoothness of the scrolling.

If you found your best rate, you can put it in the command line:

    -Rfps  e.g. -R40

**Note for Windows**: You will get 100 and 200 FPS only, if you didn't compile with
the `HAVE_SLOW_CPU` option.