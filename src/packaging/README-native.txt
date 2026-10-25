H.E.R.O. (native SDL2 build of the Commodore 64 version)
========================================================

Converted by monkeyx.

Work in progress. Run ./hero, or hero.exe on Windows (add --windowed for a
window on a desktop). The C64 game is translated to C routine by routine and
checked against the original 6502 code; it runs on a model of the C64's
video, sound and input chips.

Tape image
  The game needs your own C64 tape image of H.E.R.O. (H.E.R.O.tap, the
  Activision cassette release). None is included. Put H.E.R.O.tap next to
  the program, set HERO_TAP to its path, or start with
  --tap /path/to/H.E.R.O.tap. Without it the program shows a screen
  explaining this.

Controls
  Move / fly: left stick, D-pad or arrows (down: lay dynamite)
  Fire (laser): A or B; keyboard Ctrl, Z or Space
  Start game: Start / F1        Start level (1, 5, 9, 13, PRO): Back / F3
  Pause: Y / P (RUN/STOP)       Reset: P held + PageUp (RESTORE)
  C64 / hi-res picture: L1 / F2   Play area / whole frame (border): R1 / F4
  Fullscreen: F11     Screenshot: F12
  Quit: Esc (PortMaster: the hotkey)

Options
  --hires        start with the hi-res picture (Scale2x pixel-art filter)
  --border       show the whole C64 frame (default: the 320x200 play area)
  --integer      whole-number scaling only (default: whole number when it fills
                 at least 90% of the screen, otherwise sharp bilinear)
  --windowed     a window instead of fullscreen
  --scale N      window size (N x the picture)

See NOTICE for the third-party material this project refers to.
