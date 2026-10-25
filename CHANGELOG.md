# H.E.R.O. — native SDL2 port (C64 version) — Changelog

---
## Changelog

### v0.1.0 (2026-10-25)

#### Game

- **The tape, decoded** — the player's `H.E.R.O.tap` is read directly: the
  CBM ROM-loader blocks are decoded and checked, and the loader's three
  decryption stages are replayed to recover the game (a cartridge-style
  program at `$8000-$BFFF`). The tape's copy protection (cassette, PLAY
  key, no disk drives) is documented but not needed.
- **The game in C** — all 155 routines of the original 6502 program
  translated to C, one function per routine, its state at the original
  addresses: the hero, prop-pack, power, laser, dynamite, walls, lanterns,
  enemies, rafts, tentacles, magma, the miner, scoring, extra lives, PRO
  mode, attract mode and the demo.
- **Checked against the original** — every routine runs against the
  original 6502 code on game states captured from the original (levels 1,
  5, 9, 13, PRO): RAM, colour RAM, video and sound registers equal in every
  run; all 256 screens of the 20 levels built identically.
- **The C64, modelled** — VIC-II (ECM text, multicolour, sprites, raster
  splits, sprite collisions), SID (3 voices, ADSR, filter), CIA keyboard and
  joysticks; the program's main loop runs as a coroutine with the C64's CPU
  time per frame, so screen redraws take the same 6 frames.

#### Port

- **Hi-res picture** — the Scale2x pixel-art filter; F2 (L1 on a pad) or
  `--hires`.
- **Display for handhelds** — the 320x200 play area by default (F4/R1 or
  `--border` for the whole frame); whole-number scaling when it fills 90% of
  the screen, otherwise sharp bilinear (crisp at the next whole scale,
  smoothed down), as in Star Force: 720x720 shows 720x450, 1280x720 shows
  1152x720; `--integer` for whole numbers only. A software renderer
  fallback for KMSDRM without OpenGL ES, and paced frames when vsync is
  missing or ignored.
- **Lower CPU use** — the VIC-II drawn a character cell at a time and
  border-only lines skipped (0.51 to 0.18 ms a frame, the same pixels and
  collisions); the hi-res filter at 2x on the CPU with the rest of the
  scaling on the GPU (2.4 to 0.2 ms, 6.7 to 1 MB uploaded a frame); the
  picture made and uploaded only when the game has drawn a new frame.
- **Lookalike font** — for the credits row the original draws with the C64
  character ROM; no Commodore ROM needed.
- **Controls** — keyboard and game controllers; F1 start, F3 start level,
  P pause (RUN/STOP), PageUp RESTORE; fullscreen by default, `--windowed`.
- **Headless runs** — `--frames` with scripted input, PNG screenshots, RAM
  dumps, WAV audio, state hashes (replay tests).
- **Build and release** — CMake with CTest, Linux (x86_64, aarch64),
  PortMaster, Windows, macOS and web builds, CI and release workflow, from
  the sibling Star Force port.
- **Development tools** — tape decoder, 6502 disassembler with labels,
  coverage and flow analysis, a reference C64 model running the original
  (`c64view`).
