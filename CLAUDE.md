# CLAUDE.md

Guidance for Claude Code working in this repository.

## What this project is

A **C + SDL2** port of Activision's *H.E.R.O.*. It replicates the **Commodore 64
version** and adds a **hi-res** rendering mode. See `PLAN.md` for the plan and
`PROGRESS.md` for status.

- C64 assets and logic come from `H.E.R.O.tap` (C64 raw tape image). The game
  game reads the player's tape directly (`src/tape.c`: decode + decrypt) at
  startup; nothing is extracted to disk.
- `H.E.R.O. (USA).a26` (8 KB Atari 2600 ROM) is a secondary reference only.

## How the port works (read this first)

- **Game logic** (`src/hero_*.c`) is a hand translation of the 6502 code. There
  is one C function per original routine, named `sXXXX` after its C64 address
  and commented with what it does. Values pass through the global registers
  `ra/rx/ry/rc` exactly like the original. State lives at the original addresses
  in `M[]`. Tables and level data are read straight from the original image in
  `M[$8000-$BFFF]`. High-level description: `docs/c64_logic.md`.
- **Hardware model** (`src/hw.c`): VIC-II registers applied per raster line,
  sprite collisions (`$D01E/$D01F`), SID register writes, CIA keyboard/joystick,
  colour RAM. The three raster IRQs run at their raster lines inside
  `hw_frame()`.
- **Main thread** (`hero_main`, the original `$8009`/`$8154`) runs as a
  coroutine on an `SDL_Thread`, with a strict semaphore handoff so only one side
  ever runs. Frame waits call `hw_yield()`. CPU time is charged with `CYC(n)`
  against a per-frame budget, so long jobs (screen redraws) span frames like
  the original.
- **Rendering**: `src/vic.c` is the VIC-II line renderer (cell based), shared
  with the reference model. Its output and collisions must stay identical:
  check any change against the old version on game frames and random state.
  `render_hires.c` crops the frame (play area / border) and applies Scale2x
  for hi-res (2x on the CPU only). `platform_present` scales on the GPU:
  whole numbers when they fill ≥ 90%, otherwise sharp bilinear (render target),
  like Star Force. Pictures are made/uploaded only for new frames. Renderers
  never touch game state.
- **Sound**: `src/sid.c` synthesizes from the SID register writes, timestamped
  by raster line.
- **Font**: one text row uses the C64 character ROM. `src/font.c` is an
  original lookalike. Never add the Commodore ROM.

## Hard rules

1. **Only C (C11) and SDL2.** No other libraries, and no other languages in the
   build. Tools in `tools/` and tests in `tests/` are C.
2. **Never commit original or derived copyrighted data.** The `.tap`, `.a26`,
   `extracted/` and `build/` are git-ignored; keep it that way. Read data from
   `M[]` (the loaded image). Don't paste dumps into source files.
3. **Keep translations faithful.** Changes to `src/hero_*.c` must keep
   `ctest` (difftest) at **0 state mismatches**. Quirks stay unless `PLAN.md` says otherwise.
4. **Zero-page indexed accesses wrap**: use `ZPX(base, idx)` for `zp,X`/`zp,Y`,
   and plain `M[base + idx]` only for absolute-indexed (`$0026,Y`-style) operands.
5. **Renderers are read-only**; both modes must play identically.
6. **Fixed 50 Hz PAL simulation** (`hw_frame()` once per frame). Never tie logic to wall-clock time.
7. **Update `PROGRESS.md`** whenever a task is completed or a metric changes.

## Layout

```
src/        the port
  main.c            loop, headless mode (--frames/--shot/--dump/--input/--keys/--watch/--wav)
  hw.c/.h           C64 hardware model + main-thread coroutine
  hero.h            translation conventions, zero-page names, routine prototypes
  hero_main.c       cold start, main loop, sprite setup/animation, timers, level start
  hero_irq.c        raster IRQs $9734/$975F/$97CB
  hero_screen.c     screen construction, walls, colours, per-screen objects
  hero_play.c       input, physics, laser, dynamite, map collision, screen exits
  hero_events.c     collisions, deaths, rescue/level end, game over
  hero_panel.c      power bar, lives, dynamite, score, level, bottom text row
  hero_sound.c      SID effects; hero_util.c: waits, sprite shadows, 16-bit math
  vic.c, sid.c, font.c, render_hires.c, platform_sdl.c
  tape.c            TAP decoding + the loader's decryption (the game's only input)
  png.c             PNG writer for screenshots
tools/      tapdump, herounpack, dis6502 (labels + coverage + flow analysis),
            cpu6502 + c64sys + c64view (reference C64 model running the original), c64run
tests/      difftest.c (C translation vs original 6502), test_unit.c (no data; runs in CI),
            replay.cmake + replay.txt/.expected (deterministic RAM hashes)
docs/       c64_memory_map.md, c64_logic.md, c64.labels
src/packaging/  package.cmake, README-native.txt, portmaster/ (HERO.sh, port.json, gameinfo.xml)
src/web/        shell.html for the Emscripten build
.github/workflows/build.yml   CI: version gate (CHANGELOG == CMakeLists), Linux x86_64/aarch64
                (Ubuntu 20.04), Windows (MinGW), macOS, web, then a GitHub release
extracted/  (ignored) dev leftovers: cov.bin (coverage for disasm)
build*/     (ignored) CMake build dirs
```

## Build, run, test

```sh
cmake -S . -B build && cmake --build build -j     # game, tools, tests
build/hero --windowed [--hires]  # play (finds H.E.R.O.tap in the source folder in dev builds)
ctest --test-dir build           # unit + tape tests: difftest (routines, screens), replay
cmake --build build --target disasm          # build/hero.asm (docs/c64.labels + extracted/cov.bin)
cmake --build build --target run-ref         # play the ORIGINAL on the reference model
cmake --build build --target replay_update   # after an intended behaviour change
cmake --build build --target package_linux package_portmaster   # release archives
emcmake cmake -S . -B build-web && cmake --build build-web --target serve   # web

# headless (screenshots / RAM dumps / audio) - same input scripts for port and reference:
build/hero --frames 900 --keys "400:f1" --input "470:d,480:" --shot out.png --dump ram.bin
build/c64view build/c64mem.bin --frames 900 --keys "400:f1" --bmp ref.bmp --watch BD
magick ref.bmp -scale 200% ref.png     # view BMPs
```

`build/difftest H.E.R.O.tap [snapshots] [seed] [routine-hex]` tests one routine with
details. Register-only differences are warnings: call-site analysis showed no
caller depends on them.

## Changing or adding logic

1. Find the routine in `build/hero.asm` (target `disasm`) and in `docs/c64_logic.md`.
2. Translate or modify `sXXXX` in the matching `hero_*.c`, keeping the flag
   semantics. `cmp()` returns the difference: `NEG()` for BMI/BPL, `rc` for
   BCC/BCS.
3. Add it to the `routines[]` table in `tests/difftest.c` (and to `call_addr()`
   if it is reached via `JMP (ind)`).
4. `ctest --test-dir build` → 0 state mismatches. Check visually with headless screenshots.

## Source media facts

- TAP: standard CBM ROM-loader encoding. Two files: BASIC loader `$0801` and
  encrypted `ACT_IVIS_ION` `$7E00-$BFFF`. Decrypted game = cartridge-style image
  `$8000-$BFFF`, entry `$8009`.
- Graphics: ECM text playfield, multicolour status panel, charset `$2000`
  (copied from `$A4A0`), sprites `$2800-$3FFF` (built at init), per-line colour
  splits.
- 2600 ROM: 8 KB, F8 hotspots `$1FF8`/`$1FF9`, reset vector `$F000`.

## Releases

Bump the version in **both** `CMakeLists.txt` (`project(hero VERSION x.y.z)`)
and the first `### vX.Y.Z (date)` heading of `CHANGELOG.md`; CI refuses a
mismatch and publishes a GitHub release for each new version pushed to main.
Release builds use `-DHERO_DEV_DATA=OFF`. Never bundle the tape in anything
published: `HERO_PACKAGE_TAP` (packages, named `-with-tape`) and `HERO_WEB_TAP`
(web) exist for the user's own devices only.

## Machine notes

- gcc, cmake, SDL2 2.32, Emscripten, MinGW (x86_64-w64-mingw32), docker, node and
  ImageMagick (`magick`) are installed. The web build's headless mode runs under
  node (`node build-web/web/index.js --frames N --hash` with a local HERO_WEB_TAP build). `mame` is
  present but has no C64 ROMs, so use the in-repo reference model.
- Headless SDL: `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/hero`.

## Style

- C11, 4-space indent. Translated routines follow the conventions in `hero.h`.
  Platform code uses `snake_case` with module prefixes.
- Fixed-width types for anything mirroring C64 RAM. Keep files focused.
