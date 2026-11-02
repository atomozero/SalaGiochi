```
 ____    _    _        _       ____ ___ ___   ____ _   _ ___
/ ___|  / \  | |      / \     / ___|_ _/ _ \ / ___| | | |_ _|
\___ \ / _ \ | |     / _ \   | |  _ | | | | | |   | |_| || |
 ___) / ___ \| |___ / ___ \  | |_| || | |_| | |___|  _  || |
|____/_/   \_\_____/_/   \_\  \____|___\___/ \____|_| |_|___|

        .  * .        \  |  /            .   *
     *        .     -- (^_^) --     .           .
   .     *            /  |  \            *   .
           ________________________________
          |  ____________________________  |
          | |                            | |
          | |   > INSERT COIN_           | |
          | |                            | |
          | |     [ PRESS  START ]       | |
          | |____________________________| |
          |                                |
          |    (o)   ___         ( A )     |
          |   --+--     |         ( B )    |
          |     |    ___|  START           |
          |________________________________|
                 ||                ||
                _||_              _||_
```

```
+--------------------------------------------------------------------+
|  A games room for children on Haiku and RenkuOS.                   |
|  Big pictures, one gamepad, no menus to get lost in.               |
|  Made for the Toshiba AC100, happy on any Haiku machine.           |
+--------------------------------------------------------------------+
```

## `> WHAT IS IT_`

**SalaGiochi** ("games room" in Italian) is a full-screen launcher that turns a
Haiku computer into a little arcade: a row of consoles, a wall of game covers,
and a gamepad to choose and play. Behind it run [libretro](https://www.libretro.com)
cores, the emulators of RetroArch, loaded directly: no RetroArch, no settings
screens, nothing a child can break.

```
   [ CONSOLES ]  -->  [ GAMES ]  -->  [ PLAY! ]  -->  ( time's up :) )
        ^                                                   |
        +---------------------- back ------------------------+
```

## `> WHAT IT PLAYS_`

```
.--------------------.-------------------------------.-----------------.
| SYSTEM             | FILES                         | CORE            |
|--------------------+-------------------------------+-----------------|
| NES / Famicom Disk | .nes  .fds                    | fceumm          |
| Game Boy (Color)   | .gb   .gbc                    | gambatte        |
| Game Boy Advance   | .gba                          | gpsp            |
| Mega Drive         | .md   .gen  .smd              | picodrive       |
| Master System      | .sms                          | picodrive       |
| Game Gear          | .gg                           | picodrive       |
| Super Nintendo     | .sfc  .smc                    | snes9x2005      |
| Arcade             | .zip                          | mame2003_plus   |
| Vectrex            | .vec                          | vecx            |
| PlayStation        | .cue  .pbp  .chd              | pcsx_rearmed    |
| Nintendo DS        | .nds    (the mouse is the pen)| desmume2015     |
| TIC-80             | .tic                          | tic80           |
| Haiku games        | .gioco  (see below)           | the game itself |
'--------------------'-------------------------------'-----------------'
```

```
  +------------------------------------------------------------+
  |  !!  NO GAMES ARE INCLUDED. Bring the game files you own.  |
  +------------------------------------------------------------+
```

## `> HOW TO PLAY_`

```
   D-pad / stick ......... choose        A / X ............ play
   Enter / Space ......... play too      Esc / Z .......... back
   (arcade) Start ........ puts a coin in first, then starts
   double click the SUN (^_^) ......... back to the desktop
```

- The console names, the folders and the cores follow the language of the
  system (English and Italian today).
- The screens are scaled to the display, whatever its size.
- Covers: a picture with the name of the game, next to it or in its
  `covers` folder, is shown on the wall.
- Names: a `names.txt` in the folder of a console gives the games a name to
  read, one per line: `supertnk.zip = Super Tank`.

## `> FOR THE GROWN-UPS_`

```
        .----------------------------.
        |   PARENTS' AREA            |
        |                            |
        |   How much is  7 x 8 ?     |      <- a small sum keeps
        |   [ 56 ]                   |         the little ones out
        '----------------------------'
```

Behind a multiplication, with the mouse or the gamepad:

- **minutes a day**: when they are over, the games stop for today;
- **the highest volume**;
- **hidden games**: the ones that are not for now.

### Adding games

Drop the game files on **Add games** (the sun with a `+`, on the desktop): each
one goes to the folder of its console, inside the **Games** folder of your home
(`~/Games`, or its name in the language of the system).

### Haiku games: `.gioco`

A `.gioco` file starts a native Haiku program as if it were a game, and the
time played counts all the same. One argument per line, `#` for comments:

```
# Games/Haiku/Pinball.gioco
/boot/system/apps/Pinball
```

## `> BUILDING_`

```sh
make                # SalaGiochi and "Add games", with their catalogs
```

Cross-building, for example for the AC100 (ARMv7-A, VFPv3-D16, no NEON):

```sh
make CXX=arm-unknown-haiku-g++ \
     ARCHFLAGS="-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm" \
     RC=rc XRES=xres
```

`pcsx_rearmed-haiku.patch` holds the changes that let the PlayStation core
build on Haiku. The cores themselves are separate projects, each with its own
license: the list, with where to find their sources, is in
[CORES.md](https://github.com/atomozero/Toshiba_AC100/blob/main/packaging/salagiochi/CORES.md)
of the Toshiba AC100 project, where SalaGiochi is packaged.

## `> LICENSE_`

SalaGiochi is under the [MIT license](LICENSE). The libretro cores it starts
are not part of it, and keep their own licenses.

```
        \  |  /
      -- (^_^) --      GAME OVER?  never. just  "see you tomorrow".
        /  |  \
```
