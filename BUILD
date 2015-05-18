FLTrator Build/Installation notes
=================================

I developed this game on Linux (Ubuntu 14.04 64 bit).

It is written with FLTK, and has no other dependencies.

To compile it, you first need FLTK 1.3.3 or above.


General
=======

As this program consists only of one source file/per executable, you could
compile it just with fltk-config:

fltk-config --use-images --compile fltrator.cxx

fltk-config --use-images --compile fltrator-landscape.cxx

The excutable can be run from the build folder, no need for an install.

Nevertheless there is a Makefile in order to be more flexible.


Audio
=====

The program plays .wav files by using the builtin play tool of the
OS. This is 'aplay' for Linux and 'play' for Apple.

If you don't hear sound please make sure, that this program is installed
and plays sounds from the command line.

Under Windows there is no such tool and one cannot output sound in background
easy. So I made a little executable 'playsound' that is called by fltrator.
It has to be in the path somewhere (or in the same path as fltrator).

It is also possible to specify a play command through command line (see help).


Building under Linux (this should also work for Mac)
====================================================

Either fetch FLTK from www.fltk.org and compile it yourself or install the
development package of your distribution (if they are 1.3.3 or higher).

Use FLTK from distribution:
---------------------------

In a terminal change to the fltrator directory and issue:

  export FLTK_DIR=

  make -e all
  make install (optional)


Use downloaded FLTK:
--------------------

Say you downloaded it to ~/fltk-1.3.3/:

First configure and compile FKTK.

  cd ~/fltk-1.3.3

Configure and compile it, usually you only need:

  make

If that succeeds go to the fltrator directory and

either edit the Makefile and change FLTK-DIR to ~/fltk-1.3.3 and issue

  make all
  make install (optional)

or:

  export FLTK_DIR=~/fltk-1.3.3/

  make -e all
  make install (optional)


Running fullscreen under Linux
------------------------------

I found some instructions how to change resolution using Xrandr and created
a program 'xrand_change_screen_res' and a shell script 'fltrator-fullscreen'.
Calling this script will change screen resolution to 800x600 before running
fltrator and restore to original resolution afterwards. Put these programs
manually somewhere in your path, they are not installed with 'make install'.


---


General notes about the Windows version
=======================================

Prologue: I never program for Windows. When I need something for Windows I build
it with mingw under Linux. So forgive me if I talk nonsense.

I have noticed in some of my recent projects, that it is neary impossible
with standard procedures to get reliable hispeed timers that work persistant
under all flavours of Windows. Either they are not accurate or they eat all CPU.

FLTrator normally would like to use a 200Hz timer for scrolling the landscape.
This is just the right speed for a smooth scrolling by 1 pixel/frame. No problem
under ANY Linux system. But a BIG problem for a Windows system, as far as I
experience (But maybe it's just my lack of knowledge, how to setup things right).

I managed to run FLTrator under Windows 7 on a recent quad core processor with 200Hz.
But the same executable performs utterly poor on an old Windows XP system with
a Pentium processor.

I found 40Hz to be the ideal rate in this case. Therefore I have added an option
'HAVE_SLOW_CPU' to compile with a 40Hz timer. This makes the scrolling a little
jumpy, but maybe this even adds to the retro feeling..

Should you experience bad performance, you can enable the switch in your compile
shell with:

  export HAVE_SLOW_CPU=1

before running

	make -e


Creating a Windows executable using mingw on Linux
==================================================

This requires that you have the mingw development package of your distribution installed.

Prepare a FLTK version compiled with mingw:
-------------------------------------------

Download FLTK into some directory say: ~/fltk-1.3-mingw/:

  cd ~/fltk-1.3-mingw

  make clean
  ./configure --host=i686-w64-mingw32 --enable-cygwin=yes --enable-localjpeg=yes --enable-localzlib=yes --enable-localpng=yes

*Note*: Use the mingw host you have installed on your machine in the '--host=...' switch
        (The above was mine under Ubuntu 14.04).

The compilation may stop (at least it did until recently) somewhere within fluid.
But this doesn't matter, as the FLTK libraries are built already at this point.

After successful build of FLTK change to 'mingw' subfolder of your fltrator folder.

  cd {your-fltrator-folder}
  cd mingw

Again adjust the Makefile to point FLTK_DIR to your previous built FLTK-mingw or use
the export override and the same commands as above to build the Windows executables.

If that succeeds:

   make install

will then copy all needed files to the INSTALL_PATH  specified in the Makefile (default: ~/Downloads/fltrator).
You can copy them from there to your Windows machine.


Native builds under Windows
===========================

I didn't try it, but if you have a working development suite, I guess it should be very
easy to build as you need only single file projects.

