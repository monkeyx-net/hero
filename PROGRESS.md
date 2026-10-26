# PROGRESS

Last updated: 2026-10-25

## Overall

| Phase | Weight | Done | Weighted |
|---|---:|---:|---:|
| 0 Repository & tooling | 5% | 100% | 5.0% |
| 1 Extraction | 10% | 80% | 8.0% |
| 2 Reverse engineering | 20% | 100% | 20.0% |
| 3 Asset conversion | 10% | 100% | 10.0% |
| 4 Core engine | 10% | 100% | 10.0% |
| 5 Gameplay | 25% | 100% | 25.0% |
| 6 Sound | 5% | 100% | 5.0% |
| 7 Hi-res mode | 10% | 75% | 7.5% |
| 8 Verification & polish | 5% | 85% | 4.2% |
| **Total** | 100% | | **94.7%** |

`Phase done %` = checked items ÷ items in that phase below (dropped items excluded).

## Metrics

| Metric | Current | Target |
|---|---:|---:|
| TAP blocks decoded (checksum OK) | 8 / 8 | all |
| Loader/protection stages understood | 3 / 3 | 3 |
| Original routines translated to C | 155 / 155 | all |
| Routine diff-test state mismatches (`ctest`: 9k runs; 18k with 40 snapshots) | 0 | 0 |
| Diff-test coverage: start levels | 1, 5, 9, 13, PRO | all options |
| Screens built identically to the original (all 20 levels) | 256 / 256 | all |
| Routines with register-only differences (no caller depends on them) | 73-74 | 0 (cosmetic) |
| Replay regression scripts passing | 4 / 4 | all |
| Redraw pacing vs original (frames per screen change) | 6 vs 6 | equal |
| Attract-mode event spacing vs original | within 1 frame | within 1 frame |
| Build warnings (`-Wall -Wextra -Werror`) | 0 | 0 |
| Platforms building | Linux (native + Ubuntu 20.04 container), Windows (MinGW cross), web | + macOS (CI only, unverified) |
| Web build equal to native (replay hash under node) | yes | yes |
| CPU per frame, desktop (game+VIC / SID / picture, hi-res) | 0.18 / 0.04 / 0.20 ms | handheld-safe (< 5 ms on an A35) |
| Display fit (640x480, 720x720, 1280x720) | 2x, 720x450, 1152x720 | fill without blur |
| Levels completed in a real playthrough (by a human) | 0 | 20 |

## Phase 0 — Repository & tooling
- [x] git repository initialised
- [x] `.gitignore` excludes `.tap`, `.a26`, `extracted/`, `build/`
- [x] `CLAUDE.md`, `PLAN.md`, `PROGRESS.md`
- [x] CMake builds the port, tools and tests (same process as Star Force)
- [x] the game reads the player's `H.E.R.O.tap` directly (`src/tape.c`), no extraction step

## Phase 1 — Extraction
- [x] `tapdump` decodes pulses → bytes
- [x] header + data blocks parsed, checksums verified (both copies match)
- [x] PRGs written to `extracted/prg/`
- [x] 3-stage encrypted loader + protection decoded (`herounpack` → `c64mem.bin`)
- [ ] `a26dump` splits F8 banks (low priority: the 2600 ROM is reference only)

## Phase 2 — Reverse engineering
- [x] `dis6502` with labels file, coverage map and flow analysis (target `disasm`)
- [x] reference C64 model (`c64sys`/`c64view`) runs the original
- [x] memory map documented (`docs/c64_memory_map.md`)
- [x] main loop + IRQ documented (`docs/c64_logic.md`)
- [x] player physics / power / laser / dynamite documented
- [x] enemy AI documented
- [x] level data format documented
- [x] sound effects documented

## Phase 3 — Asset conversion
- [x] charset: built by the original code from the image (`$A4A0` → `$2000`)
- [x] sprites: built by the original code (`$8290`)
- [x] level maps + colours (read in place from `$B500-$BFFF`)
- [x] status bar / title graphics
- [x] lookalike 8x8 font for the char-ROM text row (no Commodore ROM)
- [x] sound data (effect register tables used in place)
- [~] `--viewer` mode: dropped. Headless `--shot` plus the reference model replace it.

## Phase 4 — Core engine
- [x] fixed 50 Hz PAL frame (`hw_frame`), main thread as coroutine with cycle budget
- [x] VIC-II line renderer (ECM/MC/bitmap, sprites, per-line splits) shared with the reference model
- [x] keyboard + game controller input (controller untested on hardware)
- [x] collision system: VIC sprite collisions + char-map corners, as in the original

## Phase 5 — Gameplay
- [x] hero movement + prop-pack
- [x] power meter
- [x] laser
- [x] dynamite + destructible walls
- [x] lanterns / darkness
- [x] magma walls / hazards
- [x] enemies (spider, bat/moth, tentacle, raft)
- [x] miner rescue + level bonus
- [x] scoring, lives, extra lives
- [x] PRO LEVEL mode
- [x] title / attract / game over

## Phase 6 — Sound
- [x] SDL audio (queued, per-frame)
- [x] SID synth (3 voices, tri/saw/pulse/noise, sync/ring, ADSR, SVF filter)
- [x] all effects wired (register writes timestamped by raster line)

## Phase 7 — Hi-res mode
- [x] `render_hires.c` reading only the finished frame
- [x] pixel-art upscaler in C (Scale2x ×2 = 4x)
- [ ] smooth interpolation / transitions
- [x] runtime toggle (F2) + `--hires`

## Phase 8 — Verification & polish
- [x] reference-model comparison (timeline, redraw pacing, screenshots)
- [x] routine-level differential test + all-screens test (`ctest`)
- [x] replay regression tests (`ctest`, `replay_update`)
- [x] unit tests that need no data (`test_unit`, run in CI)
- [ ] config file (keys, scale, mode)
- [x] fullscreen (F11), pause (P = RUN/STOP), resizable window with aspect fit
- [x] handheld display: play-area crop, integer / sharp-bilinear fit, software fallback, vsync-less pacing
- [x] CPU use: cell-based VIC (2.7x faster, identical output), 2x CPU hi-res + GPU scaling
- [x] clean `-Wall -Wextra -Werror` build
- [x] CI: version gate, Linux x86_64/aarch64 + PortMaster packages (rehearsed in ubuntu:20.04), release job
- [x] Windows (MinGW) build (cross-compiled locally)
- [ ] macOS build (in CI, not yet seen to pass)
- [x] web build (Emscripten fibers; same replay hash as native under node)

## Known issues / not verified

- Not played end-to-end by a human yet. Sound fidelity has only been checked
  numerically (levels, envelopes), not by ear.
- The main-thread timing model is a cycle budget, not cycle-exact. Logic is
  unaffected, but the frame on which an event happens can differ by ±1 from
  the original.
- The reference model has no badline/sprite DMA stalls, so the real machine
  is a little slower than both.
- Interrupt masking is approximated: raster IRQs are skipped, not delayed,
  while the main thread has `SEI` (only during level start).

## Log

- 2026-10-25: Repo created. Plan written.
- 2026-10-25: Tape decoded. Loader and protection reversed. Reference C64
  model runs the original. Port skeleton built.
- 2026-10-25: Whole game translated to C (155 routines, `src/hero_*.c`) on a
  C64 hardware model with a main-thread coroutine. SID synth, hi-res Scale2x×2
  mode, lookalike font. Differential test vs the original found and fixed:
  zero-page wrap, loop exit values, the `$8218` loop count. Result: 0 state
  mismatches in 18k routine runs (levels 1/5/9/13/PRO), and all 256 screens of
  all 20 levels are identical. Replay hashes added. Docs: `c64_logic.md`, labels.
- 2026-10-25 — v0.1.0 release setup: CMake/CTest replacing the Makefile, the
  tape read directly by the game (no-tape screen otherwise), PNG screenshots,
  packages (Linux, PortMaster, Windows, macOS), web build on Emscripten fibers,
  GitHub Actions workflow and README/CHANGELOG/NOTICE/LICENSE (as in Star Force).
- 2026-10-25 — CPU/GPU and display: VIC renderer cell based (0.51 → 0.18 ms,
  proven identical on 1.1M lines), hi-res 2x on CPU + GPU (2.4 → 0.2 ms),
  upload only new frames, play-area crop, Star Force's integer/sharp-bilinear
  fit (checked at 640x480, 720x720, 960x544, 1280x720), software renderer
  fallback, pacing when vsync is missing.
- 2026-10-25 — v0.1.0 released by CI (all platforms green). v0.1.1 prepared:
  `HERO_PACKAGE_TAP` for personal packages with the tape (`-with-tape`), README
  section with the package and web commands.
