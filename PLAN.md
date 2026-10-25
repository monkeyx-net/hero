# H.E.R.O. Port Plan — C + SDL2

## Goal

A native **C + SDL2** reimplementation of Activision's *H.E.R.O.* that:

1. **C64 mode** — replicates the Commodore 64 version (graphics, sound, level
   layouts, enemy behaviour, timing, scoring) as faithfully as possible, using
   assets and logic recovered from `H.E.R.O.tap`.
2. **Hi-res mode** — the same game logic rendered at a higher internal resolution
   with upscaled/refined artwork, smooth scrolling between screens and modern
   display options. Switchable at runtime; gameplay must be identical in both modes.

The Atari 2600 ROM `H.E.R.O. (USA).a26` is a secondary reference (original design,
level data cross-check), not the fidelity target.

## Hard constraints

- **Language:** C (C11). **Only external dependency:** SDL2. No SDL_image,
  SDL_mixer, SDL_ttf, no other libraries. Tools that extract/convert assets are
  also written in C.
- **No copyrighted data in git.** `H.E.R.O.tap`, the `.a26`, and anything decoded
  from them (`extracted/`) are git-ignored. The build extracts assets from the
  user's own copy of the originals.
- **Deterministic logic:** fixed-rate simulation (C64 PAL frame = 50 Hz) decoupled
  from rendering, so input replays produce identical results.

## Known facts about the source media

| File | Facts |
|---|---|
| `H.E.R.O.tap` | C64-TAPE-RAW v1, PAL, 976 044 pulse bytes. Pulses are almost exclusively `0x30`/`0x42`/`0x56` (short/medium/long) → **standard CBM Kernal ROM-loader encoding**, no turbo loader at the pulse level. Each block is recorded twice (use the second copy for error-checking). |
| `H.E.R.O. (USA).a26` | 8 KB, Atari **F8** bank-switching (hotspots `$1FF8/$1FF9`). Reset vector → `$F000`. |

**Resolved (2026-10-25):** the tape holds a 3-stage encrypted loader + protection
check wrapping a cartridge-style game image at `$8000-$BFFF` (`CBM80`). Fully
decrypted by `tools/herounpack.c`; see `docs/c64_memory_map.md`. The game runs
its whole update inside a raster IRQ (50 Hz), uses ECM text mode with per-line
colour splits, and has a **PRO LEVEL** mode.

Levels: 20 (0-19). After level 20, levels 13-20 repeat. PRO mode starts at level 17 and
then picks each next level pseudo-randomly from the frame counter ($8508). NTSC is not modelled (PAL tape).

## Reference model

`tools/c64sys.c` + `tools/c64view.c` form a small C64 model in C + SDL2: a 6502
core, raster IRQ, KERNAL traps, CIA joystick/keyboard and a line-based VIC-II
renderer. It runs the original from the decrypted image. Uses:
- capture RAM snapshots for asset extraction (`extracted/c64init.bin`)
- screenshots of every level for comparison
- differential tests: same input script on the original and on the port
- trace/debug the original while documenting routines

The Commodore character ROM is **not** required. The one text row that uses it
renders as garbage unless `--chargen` is given.

## Approach (as built)

The port is a **hand translation of the original 6502 code into C** (one function
per routine, `src/hero_*.c`). It runs on a small model of the C64 hardware the
logic depends on (`src/hw.c`, `src/vic.c`, `src/sid.c`). Game data (level
tables, sprite shapes, charset) is read straight from the decrypted original
image at runtime, so no asset conversion step is needed and nothing copyrighted
lands in git.

Correctness is proved by **routine-level differential testing**
(`tests/difftest.c`, run by `ctest`). Every translated routine runs against the
original 6502 routine on the CPU core, using real game states captured from the
original on the reference model (levels 1, 5, 9, 13, PRO). The test then
compares all RAM, colour RAM, VIC and SID state.

## Repository layout

```
CMakeLists.txt        game, tools, tests (CTest), packages, web; CI in .github/workflows
src/                  the port (see CLAUDE.md for the per-file map)
  hero_*.c, hero.h    translated game logic
  hw.c/.h             C64 hardware model + main-thread coroutine
  vic.c/.h            VIC-II line renderer + collisions (shared with tools/)
  sid.c/.h            SID synthesizer
  render_hires.c      hi-res renderer (Scale2x x2)
  font.c              lookalike font for the char-ROM text row
  platform_sdl.c, main.c
tools/                tapdump, herounpack, dis6502, cpu6502, c64sys/c64view (reference model), c64run
tests/difftest.c      differential test vs the original code
docs/                 c64_memory_map.md, c64_logic.md, c64.labels
extracted/, build/    (git-ignored)
```

## Phases

### Phase 0 — Repository & tooling
- git repo, `.gitignore` excluding originals and derived data.
- `CLAUDE.md`, `PLAN.md`, `PROGRESS.md`.
- Build system: CMake (same build/CI/release process as the sibling Star Force port).

### Phase 1 — Extraction (C tools)
- `tapdump`: classify pulses (thresholds between 0x30/0x42/0x56), decode CBM
  header + data blocks, verify checksums, compare both copies, write
  `extracted/*.prg` and a log of block names/load addresses.
- If the program contains a secondary loader or packer, trace it with the
  disassembler and write a C unpacker (or emulate the depacker minimally in C).
- `a26dump`: split the 8 KB ROM into banks for reference.
- Exit criterion: complete C64 memory image(s) saved; checksums all pass.

### Phase 2 — Reverse engineering
- `dis6502` with an annotations file (`docs/c64.labels`) so the disassembly is
  regenerated, never hand-edited.
- Identify: main loop, IRQ/raster split, joystick read, player physics, power
  drain, laser, dynamite timer, wall destruction, lantern/darkness, lava/magma
  walls, enemy AI per type, miner rescue, scoring, level table, screen layout
  format, charset, sprite pointers, colours, SID sound routines.
- Document in `docs/c64_memory_map.md` and `docs/c64_logic.md`.
- Exit criterion: every routine called from the main loop is named and described.

### Phase 3 — Asset conversion
- *Changed:* the original data is used in place from the loaded image, so no
  `assetconv` is needed. The game builds its own sprites (`$8290`) and copies its
  charset at cold start, as on the C64.
- Lookalike 8x8 font replaces the Commodore character ROM (one text row).
- Exit criterion: screens render identically to the reference model (shared
  VIC renderer, verified screenshots).

### Phase 4 — Core engine
- Fixed 50 Hz tick, accumulator-based main loop, VSync'd presentation.
- 320x200 indexed framebuffer + C64 palette (Pepto/VICE-style), integer-scaled
  SDL texture.
- Input: keyboard + SDL_GameController, mapped to joystick (U/D/L/R/fire).
- Collision via character map + sprite boxes, mirroring the C64 approach
  (pixel-level where the original uses sprite collision registers).

### Phase 5 — Gameplay (C64-faithful)
Port routine-by-routine from the disassembly. *As built:* every routine is
translated, with state at the original addresses, and verified by `ctest`:
- hero: walk, prop-pack lift/descent, power meter, laser, dynamite, death
- walls (destructible), magma walls, lanterns / darkness, water/lava, rafts
- enemies: all types found in Phase 2
- miner rescue, level end bonus (power + dynamite), extra lives, scoring
- title/attract, level select / start options, game over, high score
- Exit criterion: levels 1..N playable end-to-end; replay tests stable.

### Phase 6 — Sound
- SDL audio callback driving a minimal SID subset (3 voices, triangle / saw /
  pulse / noise, ADSR, optional filter approximation) fed by the extracted sound
  data so effects are bit-accurate in register terms.

### Phase 7 — Hi-res mode
- Separate renderer consuming the same game state (no game logic in renderers).
- Internal resolution e.g. 1280x800 (4x of 320x200); assets upscaled at load
  time with a C pixel-art scaler (xBR/hqx-style) and optionally replaced by
  hand-made hi-res sprite sheets (BMP via `SDL_LoadBMP`, the only image loader
  allowed).
- Sub-frame interpolation of sprite positions, smooth screen transitions,
  optional CRT/scanline shader done in software.
- Runtime toggle (e.g. F2) and `--hires` flag.

### Phase 8 — Verification & polish
- Side-by-side comparison against the reference model (`c64view`, same input scripts).
- Routine-level differential test vs the original (`ctest`): done.
- Recorded input replays in `tests/` asserting score / level / RAM hash
  (the port is deterministic).
- Config file (keys, scale, mode), fullscreen, pause, window resize.
- Builds clean with `-Wall -Wextra -Werror`; CI builds Linux (x86_64, aarch64,
  PortMaster), Windows, macOS and web, and publishes releases.

## Risks

- **Packed/protected loader** in the TAP → resolved (3 stages + protection check).
- **Main-thread timing** is modelled with a cycle budget, not cycle-exact.
  Logic is unaffected (verified per routine). Redraw pacing matches the
  reference to within a frame.
- **SID fidelity** with a hand-written synth — accept "close", prioritise timing.
- **Hi-res art** — automatic upscaling may look poor; hand-drawn BMP replacements
  are a fallback.
- **Copyright** — keep all original data out of the repository.
