<p align="right">25/06/2016</p>


Welcome to FLTrator (a FLTK retro arcade game)!
===============================================

Introduction
------------

**FLTrator** is a *VERY* retro style game in which you steer a spaceship through a scrolling
landscape with hostile rockets and other obstacles.

It is loosely inspired by a game called 'Penetrator', which was an arcade game from 1983(!)
on the Sinclair ZX Spectrum. (I always found it a nearly perfect made game both from a
technical and playability point of view, considering it had to fit in only 48k of ram).

That's also the reason for the name: it is written using **FLTK** - the
**F**ast-**L**ight-**T**oolkit, so **FLT** + Pene**trator** => **FLTrator**.

To be honest, I started out only to make a tiny experiment if something like that
could possibly be done with nothing else but FLTK. But as the first results were
positive, I got hooked and tried to make something playable out of it.

But of course it could be endlessly improved and expanded,... which I think will likely
not happen ever.

What's become of it now is: __10 different levels__ of play - no mission or things like that -
the goal beeing only to end level 10.

Levels can be edited with a supplied level editor though! So you can create other, perhaps
more exciting levels, than the ones defined by me.

---

The game
--------

You control a spaceship with the keyboard that is moving over a right to left scrolling
terrain. But the terrain is changing fast and there are some enimies in it, that won't
let you pass so easy...

Touching any part of the landscape with the ship is always deadly. Touching objects mostly.

The first aspect of the game is the landscape. Landscapes have a ground level and
optionally a "sky" (or ceiling). This offers some ways to make them "interesting" 
by creating steep hills, tight gaps or tunnels,..

The other part are the objects, that can be put into the landscape:

### Rockets
   - Can be shot at with missiles and bombs.
   - Always destroy ship at collision.
   - Get more dangerous with each level.

### Radars
   - Are not particularly dangerous, but are obstacles and score well.
   - Can be shot at with missiles (but need 3 hits!) and bombs.
   - Always destroy ship at collision.
   - Trigger rocket launch if not destroyed early.

### Drops
   - Are nasty.
   - Can be "evaporated" with missiles.
   - Only destroy ship when they hit it fully, otherwise do no harm, and are just deflected.
   - Get more dangerous with each level.

### Badies
   - Are also nasty as they block the way.
   - Can be attacked with missiles, but it needs up to 5 hits to succeed.
   - Are always deadly when touched.

### Phasers
   - Emit deadly vertical beams at random intervals (hint: they turn red when fully charged).
   - Can be destroyed with missiles and bombs.

### Clouds
   - Do no harm, except that the ship gets invisible when passing through.
   - Can not be destroyed.


The combination of landscape and objects makes it possible to create some
challenging tracks.

And most important of course is the:

### Spaceship
   - Can fire missiles horizontally.
   - Can drop bombs vertically.
   - Can lag speed for some time, or speed up, which can be very usefull.


The game is controlled by keyboard only, just like in the old days..


See all options with

  `fltrator --help` or find the navigation/control keys in the title menu.

---

The landscape editor
--------------------

You can edit any of the supplied landscapes or create new landscapes and
freely position objects in it.

See all options with

   `fltrator-landscape --help` or press `F1` key for help.

If you have created some interesting new landscapes I would be glad to see them!

---

Licence
-------

The game code is licensed under *GPL 3* or later.

*FLTrator* uses images from:

   [Clipartlord](http://www.clipartlord.com)

   [Pixabay](http://www.pixabay.com)

that are Public Domain or are licensed under `Creative Commons 3.0`.
The images are modified slightly (colors, sizes, details), but keep their
licence.

See `ATTRIBUTION` file for files with Creative Commons licence.

*FLTrator* also uses sound files (`.wav`) from:

   [Soundbible](http:///www.soundbible.com)

   [Wavsource](http://www.wavsource.com)

   [OpenGameArt](http://www.opengameart.org)

that are licenced under `Attribution 3.0 licence` or are Public Domain.
The sounds are modified slightly (duration, format), but keep their licence.

See `ATTRIBUTION` file for files with Creative Commons licence.

---

Resume
------

To answer the initial question, wheater it's possible to write a game just
using FLTK: **yes, it's possible**, but probably only if it's such a simple game
as this one is and if you have already some experience with FLTK's principles.

FLTK has a very simple, yet powerful event handling and drawing system, a lot of
useful image handling methods - and it is really **FAST**.

Also most real work had nothing to do with the gameplay (this was finished
quite early - except the necessary performance tuning), but writing the
framework "around" the game and designing the levels.

Initially I had much trouble to get the game running decently on Windows mostly
because of timer and sound woes.

Sound generally is an issue, that probably will want you to use a dedicated library
soon. I have put some effort into sticking with 'aplay' on Linux and an
"aplay-emulation" for WIN32, which after all do the job quite well.

But in the first place I had much fun writing it and learned a lot during
this time (probably also, that I'm not of the game writer type :)).

So there it is, it's done - and become quite cute, I think.

Although I won't probably make huge changes to the game, if you have any easy to
implement ideas for improvement or find bugs, please report them to me.


Have fun with **FLTrator**!

**Curious**: Can you finish _all ten_ levels?

---

Links
-----

   [Sourceforge project site](http://sourceforge.net/projects/fltrator/)

   [FLTK](http://www.fltk.org/)

   [Watch YouTube video v2.3r (Level 1)](https://youtu.be/q1tK40ZBG7E)


FLTrator runtime notes
======================

I developed this game on Linux (Ubuntu 14.04 64 bit).

I have successfully tested and run a Mingw compiled version of the program
under Windows XP and Windows 7.

Under Linux the program was tested with various distros, also some older ones.

Audio
-----

According to the goal to use no external libraries, the program plays `.wav`
files by using the builtin play tool of the OS. This is `aplay` in case of Linux.

If you don't hear sound please make sure, that this program is installed
on your system and plays sounds from the command line.

Under Windows I wrote an interfacing program `playsound.exe`, that wraps the Windows
`PlaySound()` function. *FLTrator* calls this program to output sound.
It has to be in the path somewhere (or in the same path as `fltrator.exe`).

You can add your own background sounds by putting them as `.wav` files into the `bgsound`
folder. From there they will be picked randomly. Or if you want a level to play a specific
sound put the file into the `wav` directory of this level (create the directory if needed)
under the name `bgsound.wav` e.g. `wav/3/bgsound.wav` for level 3.

---

Running fullscreen
------------------

Running *FLTrator* in fullscreen on a big monitor is really a great pleasure!
It also runs faster and more smooth I believe.

There are two ways to run fullscreen:

-  1) Change display resolution to e.g. 800x600 during execution

-  2) Scale the output to match full screen size

For 1) I dug out some instructions how to change resolution under Linux using Xrandr
and and also for Windows and integrated everything into the program.

Use `F10` to toggle fullscreen when in title menu or in paused state.
Or start with `-f` to immediately go fullscreen.

[Tested to work under Ubuntu 12.04 and 14.04, Windows 7]

For 2) it is now possible to scale the output up to 1920x1200.

Use `-W` command line switch to specify the screen dimension to use.

e.g. `-W1024x768` or `-W1920x1200`.

There are two 'shorthands':

-  `-Wf` uses the current screen size
-  `-W` alone uses the desktop workarea size

[Note: it also possible to scale the output down. E.g. to 400x300]

---

Performance issues
------------------

On older hardware or on a Windows system *FLTrator*'s default framerate of 200 fps
may be too high.

You can change the frame rate in runtime:

On the **title screen** you see the current frame rate in the bottom/right corner.
Pressing '-' or '+' will change to the next value down or up.

Experiment with the values, until you get the best mix of performance vs  'jumpyness'.
Allowed values are 20, 25, 40, 50, 100, 200. I recommend a rate 40+.

Of course lower values will decrease the smoothness of the scrolling.

If you found your best rate, you can put it in the command line:

    -Rfps  e.g. -R40

There's also a command line switch that sets the frame rate to 40 and disables all
unneccessary screen effects:

    -S (start with settings for a slow computer).

Your best combination of command line switches for a slow computer:

    -Scpsb


**NEW**: You can also try the auto-frame-correction mode by specifying `-C`.
         This tries to dynamically adjust the scrolling speed.

### Fast Machine?

If on the other hand you have a **fast** (any recent!) computer you should use:

    -F  (start with settings for a fast computer)

This also enables the **experimental background drawing** of a second scroll plane within
some levels (Don't hold your breath though - nothing really spectacalar .. but nice!)

    -FF (start with settings for very fast computer)

This also turns on gradient drawing of landscape.

    -FFF (start with all features)

This additionally turns on some effects like blending and scanline simulation.
