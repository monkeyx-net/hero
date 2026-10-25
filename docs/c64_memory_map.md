# C64 H.E.R.O. — memory map and load chain

All addresses refer to the decrypted 64 KB image (`build/c64mem.bin`, written by
`hero --dump-image`, or the `image` CMake target). Labels live in `docs/c64.labels`.

## Tape contents (`H.E.R.O.tap`)

| # | Type | Name | Range | Notes |
|---|---|---|---|---|
| 0 | PRG (relocatable) | `H.E.R.O. (TM)` | `$0801-$1D3F` | BASIC `10 SYS 4096` + encrypted loader |
| 1 | PRG (absolute) | `ACT_IVIS_ION` | `$7E00-$BFFF` | encrypted protection stage + game |

Standard CBM ROM-loader encoding. All blocks decode with valid checksums, and both
copies of each block match.

## Load chain

All decryption uses the routine at `$0FE8`. For each byte: `k += step; mem ^= k`.
The running key `k` carries across pages.

1. `$1000`: aborts if the NMI vector was hooked. Points RESTORE at `$0FCA` (memory
   wipe). Decrypts `$1100-$11FF` (key/step = `$102C/$102D`) and RTSes into it.
2. `$1100`: checks the IRQ/BRK vectors and that no cartridge `CBM80` is present.
   Decrypts `$1A00-$1DFF` (`$112C/$112D`) and calls `$1A00` (loading screen).
   Kernal-LOADs file 1, then decrypts `$7E00-$7FFF` (`$1D35/$1D36`) and
   `$8000-$BFFF` (`$7FFA/$7FFB`). `JMP $7E00`.
3. `$7E00`: self-decrypting code (lagged-Fibonacci keystream at `$7F21`). It checks
   - `$BA` = 1 (last device was cassette),
   - `$01` bit 4 = 0 (PLAY still pressed),
   - no device answers on serial bus 8–15 (`OPEN 15,dev,15` / `READST`).

   On failure it prints "DISCONNECT ANY OTHER PERIPHERALS EXCEPT CASSETTE UNIT
   AND RELOAD." and fills memory with SID noise. On success it jumps to `$8009`.

`tools/herounpack.c` reproduces steps 1–2 statically. Step 3 only *checks* and
modifies nothing the game needs, so `$8000-$BFFF` is the complete game.

## Game image `$8000-$BFFF`

Cartridge-style. `$8000` holds cold/warm vectors `$8009`/`$8009` plus `CBM80`.
It is almost certainly the cartridge version of the game repackaged for tape.

| Address | What |
|---|---|
| `$8009` | cold start: S=`$FF`, NMI → `$90DB`, CIA IRQs off, clear ZP `$22-$BE`, clear `$0400-$08FF` |
| `$8290` | sprite builder. Expands 1-bit shape data into multicolour sprites. Mirrored left/right variants are made with ROL/ROR. Called for `$9F82→$2800` and `$A054→$3800`. |
| `$8CFD` | copies 7 bytes into every 3rd byte (vertical sprite column fill) |
| `$80D7` | copies 5 pages `$A4A0-$A99F` → charset `$2000-$24FF` (160 chars) |
| `$90DB` | NMI (RESTORE) handler |
| `$97CB` | raster IRQ. Sets ECM colour splits for rock bands, then the bottom-row split. **Then runs the entire per-frame game update** (`$9882…`). |
| `$9EA4` | wait one frame (`$4D` = 1, IRQ clears it) |
| `$9F62` | 32 bytes copied to `$0820` at init |

## VIC-II setup

| Register | Value | Meaning |
|---|---|---|
| `$DD00` | bank 0 | VIC sees `$0000-$3FFF` |
| `$D018` | `$18` | screen `$0400`, charset `$2000` |
| `$D011` | `$58` | **ECM** (extended background colour), 25 rows, YSCROLL 0 |
| `$D016` | `$08` | 40 columns, hi-res chars (multicolour switched in at the bottom split) |
| `$D025/$D026` | 2 / 15 | sprite multicolours (red / light grey) |
| sprites | `$2800-$3FFF` | pointers `$A0-$FF`, built at init |

**ECM**: char code bits 6–7 select background `$D021-$D024`, giving 64 glyphs.
The IRQ rewrites `$D022/$D023` on specific raster lines to colour the rock bands.

**Bottom row** (raster `$EF`): `$B6` ≠ 0 selects charset `$1000`, which is the
Commodore character ROM (e.g. "DESIGNED BY JOHN VAN RYZIN"). Otherwise it uses
the game charset (Activision logo, chars `$4C-$6A`). The port must supply its own
lookalike 8x8 font. The Commodore ROM font must not be shipped.

## Zero page (partial)

| Addr | Use |
|---|---|
| `$22-$25` | src/dst pointers for builders |
| `$4D` | frame-wait counter (decremented by IRQ) |
| `$5D-$5F` | colours restored to `$D021-$D023` after the split |
| `$79` | sprite 0 colour (`$D027`) |
| `$B6` | bottom row uses ROM font when ≠ 0 |
| `$7E`, `$B1`, `$B2`, `$BD` | state flags tested by the IRQ update sequence |
