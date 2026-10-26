# H.E.R.O. - native SDL2 port of the C64 version (work in progress)

A C11 / SDL2 port of the Commodore 64 version of Activision's 1984
*H.E.R.O.* for Linux handhelds (PortMaster: ArkOS, ROCKNIX, muOS, Knulli),
desktops (Linux, Windows, macOS) and the web, checked against the original
6502 program.

**Status:** the whole game runs in C: every routine of the C64 program is
translated and checked against the original code on real game states, and
all 256 screens of the 20 levels build identically. The C64's video and
sound chips are modelled, with an optional hi-res picture. Next: testing on
devices and a release. See [PROGRESS.md](PROGRESS.md) and [PLAN.md](PLAN.md).

You need your own C64 tape image of the game (`H.E.R.O.tap`, the
Activision cassette); none is included. Put it next to the program, set
`HERO_TAP`, or pass `--tap`.

Converted by monkeyx.

```bash
cmake -S . -B build && cmake --build build -j
build/hero --windowed --tap /path/to/H.E.R.O.tap
```

## Controls

One player, an 8-way stick and a fire button (the game reads both C64
joystick ports, so any pad works).

| Action | Pad | Keyboard |
|---|---|---|
| Move / fly (down: dynamite) | left stick or D-pad | arrows |
| Fire (laser) | A or B | Ctrl, Z or Space |
| Start game | Start | F1 |
| Start level (1, 5, 9, 13, PRO) | Back | F3 |
| Pause (RUN/STOP) | Y | P |
| Reset (RUN/STOP + RESTORE) | | P held + PageUp |
| C64 / hi-res picture | L1 | F2 |
| Play area / whole C64 frame (border) | R1 | F4 |
| Fullscreen / screenshot | | F11 / F12 |
| Quit | PortMaster hotkey | Esc |

## Display

The 320x200 play area is shown (this game's C64 border is always black; F4
or `--border` shows the whole frame). It is scaled by a whole number when
that fills at least 90% of the screen, otherwise fitted with "sharp
bilinear" (drawn crisp at the next whole scale, then smoothed down), so
square and wide handheld screens are filled without blur or uneven pixels:

| Screen | Picture |
|---|---|
| 640x480 | 640x400, 2x |
| 720x720 | 720x450, fitted |
| 960x544 | 870x544, fitted |
| 1280x720 | 1152x720, fitted |
| 1920x1080 | 1600x1000, 5x |

`--integer` keeps whole-number scaling only. The hi-res picture (F2) is a
Scale2x pixel-art filter. The game uses about 0.2 ms of CPU a frame on a
desktop (0.4 ms with hi-res); scaling is done by the GPU, with a software
fallback for handhelds without OpenGL ES.

## Building

Needs CMake 3.13+, a C11 compiler and SDL 2.0.10+.

```bash
ctest --test-dir build              # tests (tape tests need H.E.R.O.tap in the source folder)
cmake --build build --target package_linux package_portmaster
emcmake cmake -S . -B build-web && cmake --build build-web --target serve
```

### Builds that include your tape (personal use only)

Release builds never contain the game: players add their own `H.E.R.O.tap`.
For your own devices you can build archives and a web page with your tape
already inside. They are named `-with-tape` (the web page bundles it into
`index.data`). **Do not publish or share them.**

```bash
# packages with the tape next to the program (Linux tarball, PortMaster zip, ...)
cmake -S . -B build-mine -DHERO_DEV_DATA=OFF -DHERO_PACKAGE_TAP=/path/to/H.E.R.O.tap
cmake --build build-mine -j --target package_linux package_portmaster
#   -> build-mine/packages/hero-0.1.1-linux-x86_64-with-tape.tar.gz
#      build-mine/packages/hero-0.1.1-portmaster-x86_64-with-tape.zip

# web page that starts straight into the game, served on http://localhost:8000/
emcmake cmake -S . -B build-web-mine -DHERO_WEB_TAP=/path/to/H.E.R.O.tap
cmake --build build-web-mine --target serve
```

With the tape in the source folder the build also offers the development
targets `image` (the decrypted program), `disasm` (labelled disassembly)
and `run-ref` (the original game on the reference C64 model), and
`replay_update` after an intended change to the game's behaviour.
How the port works: [CLAUDE.md](CLAUDE.md), [docs/c64_logic.md](docs/c64_logic.md),
[docs/c64_memory_map.md](docs/c64_memory_map.md).

## License

Apache 2.0 for the source; see [NOTICE](NOTICE) for the third-party material
this project refers to. No tape image, game data or Commodore ROM is
included or distributed.
