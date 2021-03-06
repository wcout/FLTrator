Changes in V2.4
===============

- Allow decoration (background) objects per level
   - uses a new display technique with alpha blitting, which actually
     uses less cpu to render the level
- Disable explosion sounds with key 'x' during game
- Draw tv mask option (simulates appearance like on an old tube display)
- Fix compilation warnings/bugs with recent compilers
- Minor improvements (better menu display, simplify/speedup some code)
- Minor bugfixes
- Fix minor regressions from v2.3
  - F10 key not working
  - focus out not handled correctly
  - missing title image when started with -d
- Landscape editor: better help text

Changes in V2.3
===============

- Scale down/up to arbitary screen sizes between 320x200 and 1920x1200
- Extended game with a "way home" mode
	- after finishing level 10 player must return home to level 1 with reversed terrain
- Even more gimmicks( Fade out effect, Scanline effect, ZX-Attribute clash effect at exit)
- Make all text translatable (read from language file if present)
- More ship types (but just different colors at the moment)
- Ship type saved per user
- add a 'lost' sound
- move fireworks screen into main loop
- drops/phaser can move/fire slightly horizontal
- add objects to level 6
- Better mouse control using just one button
- Minor bugfixes and improvements

Changes in V2.2
===============

- More gimmicks (explosions, animations)
- Basic joystick support for Linux version
- Better internal levels
- Landscape Editor: Undo
- Minor bugfixes

Changes in V2.1
===============

- Experimental ship acceleration/stall indicator
  (only when started with -F).
- Experimental background scroll plane with mountains or starfield
  (only when started with -F).
- Graphical remaining ships/progress display
  (only when started with -F).
- Put a gradient on landscape and background
  (only when started with -F or -FF).
- Add an option to runtime correct the frame rate (mainly useful on WIN32)
- First time setup of speed (when started with no parameters)
- Go automatically fullscreen when screen size is already 800x600
- Better window title
- Improved internal levels
- Install signal handler to trap crashes or cancelation (with ^C or by taskmanager)
  (restores screen mode and stops sound).
- Minor bugfixes

Changes in V2.0
===============

- Fullscreen Linux/WIN32(new!)
	- integrate switching code into main program
- WIN32: much tuning and testing for more playabilty/speed
- Background sound (Linux/WIN32)
	- Play a random file from bgsound folder or a specific one for a level
   - Interface to use own soundplayer
- Mouse/touchscreen control for ship
- Keyset for lefthanded players
- new 'Phaser' object
	- that emits a deadly vertical beam at irregular intervals
- Each level can have different graphics/sounds
- Once finished each level gets harder next time for the same player
- Demo mode replays finished levels
	- (data for first 3 levels "pre-played" included).
- Cleaned up title menu
- Added some 'gimmicks'
	- fireworks display ("just for fun")
	- menu sound
	- 'blend in' effect for menu/game screen
- Improve/fix collision detection
- Improved end screen
- Speedup of level rendering

---

Changes in V1.6
===============

- Add a new (penetrator like) ship type, selectable on title screen
- Finetuning of some details (explosion drawing, level graphics/parameters)
- Level editor additions (zoom window, scrollbar)

---

Changes in V1.5
===============

- Lower requirement for FLTK from 1.3.3 to 1.3.x
	- Works now with the FLTK packaged with more current distributions (e.g. Ubuntu 14.04)
- Tune the build system
	- Moved sources to subfolder
	- Homebrew `configure` script for Linux
	- Default to use installed FLTK version
	- Updated docs
- Runtime option (command line switch) to set framerate
- Make fullscreen (Linux) work more reliable
- Add a text animation on title screen
- Show version and framerate on title screen
- Fix an array access bug, that could crash the program

---

Changes in V1.4
===============

- Add Radar object
- Level fine tuning
	- improved object start intelligence
	- add level names
	- add level parameters
- Some improvements in level editor
- Many fixes

---

Changes in V1.3
===============

- `HAVE_SLOW_CPU` compile option for WIN32 and/or older hardware
- Fullscreen support under Linux
- Bugfixes

---

Changes in V1.2
===============

- Multiple User/Hiscore Concept
	- Current score and level are stored for multiple users
	- Each user always starts after last completed level
	- Easy switching between users
- Improved some levels
	- tuned difficulty, playability, added finished sound
- Bugfixes

---

Changes in V1.1
===============

- Improved performance under Windows
	- using hires timer (1 ms) instead FLTK timer (which is restricted to 15.6 ms).
- Improved some levels
	- outline, color change
- Improved landscape editor
	- added menu
	- added color change object
	- added landscape outline
- Bugfixes
