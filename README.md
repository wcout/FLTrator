<p align="right">18/12/2015</p>


Welcome to FLTrator - a FLTK retro arcade game!
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

### Rockets ![(image of rocket)](images/rocket.gif)
   - Can be shot at with missiles and bombs.
   - Always destroy ship at collision.
   - Get more dangerous with each level.

### Radars
   - Are not particularly dangerous, but are obstacles and score well.
   - Can be shot at with missiles (but need 3 hits!) and bombs.
   - Always destroy ship at collision.
   - Trigger rocket launch if not destroyed early.

### Drops ![(image of drop)](images/drop.gif)
   - Are nasty.
   - Can be "evaporated" with missiles.
   - Only destroy ship when they hit it fully, otherwise do no harm, and are just deflected.
   - Get more dangerous with each level.

### Badies ![(image of bady)](images/bady.gif)
   - Are also nasty as they block the way.
   - Can be attacked with missiles, but it needs up to 5 hits to succeed.
   - Are always deadly when touched.

### Phasers ![(image of phaser)](images/phaser.gif) ![(image of active phaser)](images/phaser_active.gif)
   - Emit deadly vertical beams at random intervals (hint: they turn red when fully charged).
   - Can be destroyed with missiles and bombs.

### Clouds ![(image of cloud)](images/cloud.gif)
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

Although I won't probably make huge changes to the game, if you have any easy to
implement ideas for improvement or find bugs, please report them to me.


Have fun with the game!

**Curious**: Can you finish _all ten_ levels?

---

Links
-----

   [Sourceforge project site](http://sourceforge.net/projects/fltrator/)

   [YouTube video level 4](https://www.youtube.com/watch?v=dZa2rYsGHHQ&feature=youtu.be)

   [YouTube video level 6](https://www.youtube.com/watch?v=PoPRpiJ5oAs&feature=youtu.be)

   [FLTK](http://www.fltk.org/)


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

Running FLTrator in fullscreen on a big monitor is really a great pleasure!
It also runs faster and more smooth I believe.

So I dug out some instructions how to change resolution under Linux using Xrandr
and and also for Windows and integrated it everything into the program.

Use `F10` to toggle fullscreen when in title menu or in paused state.
Or start with `-f` to immediately go fullscreen.

[Tested to work under Ubuntu 12.04 and 14.04, Windows 7]

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
