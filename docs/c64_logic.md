# C64 H.E.R.O. — game logic

How the original works, as recovered from the disassembly and implemented in
`src/hero_*.c` (one C function `sXXXX` per routine at `$XXXX`). See
`docs/c64_memory_map.md` for the load chain and memory layout.

## Execution model

The game has two threads of control, and the port keeps both:

| | Original | Port |
|---|---|---|
| **Main thread** | `$8009` cold start, then the endless loop at `$8154` | `hero_main()` on a coroutine (`SDL_Thread` + strict semaphore handoff) |
| **Raster IRQ** | 3 IRQs per frame, chained through `$0314`: `$9734` (line `$AF`), `$975F` (`$B6`), `$97CB` (`$ED`) | `hero_irq()`, called by `hw_frame()` at the same raster lines |

Main-thread jobs block with frame waits (`$9E9B`, `$9EA4`, `$9EAD`, `$9BEC`).
In the port these yield to the next frame. Long jobs such as screen drawing take
several frames on the real machine and the redraw is visible. The port gives the
main thread a per-frame cycle budget (`HW_MAIN_CYCLES_PER_FRAME` = 13600,
measured on the reference model). Its routines charge approximate cycle costs
with `CYC(n)`, so redraws take the same ~6 frames.

### Main loop `$8154`
1. `$877A` build the screen if `$BD` (redraw) is set
2. `$91AD` F1 pressed → start a game
3. `$8339` run expired timers (table `$0820`, 8 × [reload, count, callback])
4. `$8F75` joystick changes → callbacks `($85)` = `$8FE8` (direction), `($87)` = `$9033` (fire)
5. `$98ED` react to sprite-sprite collisions (`$D01E`)
6. `$9A83` deaths, sinking, game over

### IRQ `$97CB` (bottom of the frame)
Busy-waits through the bottom text row to recolour the Activision logo line by
line (`$F2-$F7`). At `$FC` it restores the playfield (ECM, charset `$2000`,
colours `$5D-$5F`). Then it runs the per-frame update:
- `$8236` sprite animation (table `$0800`: speed, count, frames, base pointer)
- `$8BB4` flashing wall (levels 14+), `$836E` timer countdown, `$8C5F` enemy movement
- `$94C4` propeller spin-up/down, `$9524` hero physics (unless frozen, `$7E`)
- `$9231` laser length / burning through the wall, `$9685` attach prop/legs/laser sprites
- `$8F5C` joysticks, or `$9103` scripted demo input in attract mode
- `$84A8/$8445/$8427` propeller / laser / flight sound
- `$9EB4` frame counters, `$904B` keyboard (F1, F3, RUN/STOP), `$9162` idle screen blank

`$9734` (line `$AF`) sets the colour of the animated band row. `$975F` (line
`$B6`) switches to multicolour text for the status panel and copies the sprite
shadow registers `$3C-$4C` to the VIC.

## World

- A level is a vertical sequence of screens. `$58` = level (0–19). `$57` =
  screen within the level. `$A3A8+level` = index of the level's first screen in
  the screen tables. `$A3D0+level` = index of the last screen (the miner's).
- Screen description (`$8D23`), 8 bytes from tables `$B800-$BFFF`:
  - `$60/$61/$62`: objects (enemy/lantern type in the low 2 bits, X position)
  - `$63-$68`: rock layout, 2 bytes per band × 3 bands, mirrored halves
  - `$BF00`: flags. Wall column → `$6E`, water/lava middle band → `$6B`,
    shaft side → `$69/$6A`.
- Screen drawing: three bands of 5 char rows (`$886C`). Each byte → two nibbles
  → 4 columns × 5 rows from the pattern tables `$B500/$B600/$B700`. Then edge
  shading (`$9C93`, 7 passes from `$9CD3`) and walls between bands (`$9DD1`).
  Some edge chars are random: `$9E8B` uses the raster line as the random source.
- The destructible wall (`$8E45`, column `$6E`): the laser burns it slowly
  (`$9231`: counter `$A9` overflows twice), or dynamite destroys it at once (`$942D`).
- Row 16 (`$0680`) is the animated water/lava band (`$8AFB`/`$8B45`, timer 1).

## Hero

- Position = sprite 1 shadow (`$3E/$3F`). The engine works in X/2 units with
  the X MSB in bit 7 (`$9EDB` / `$9EF9`).
- Physics (`$9539`/`$9563`/`$9598`): 8.8 fixed-point position `$0844-$0847`.
  Each axis is moved, then tested against the map by the 4 corner chars
  (`$96A9`/`$970B`). Chars `$0D-$37` are solid (`$83`), `$2B-$37` are deadly (`$84`).
- The propeller (`$94C4`) ramps the vertical speed table `$9504` by joystick up.
- Leaving a screen edge moves to the neighbouring screen (`$95AE`: up/down
  = ±1, left/right = ±`$72`) and requests a redraw.
- Animation state (`$9632`) = (in air, facing, moving) → sprite 2 frames.
- Power `$7A` drains on timer 0 (`$9F45`/`$865E`). At 0 you lose a life and the level restarts.

## Objects and events (`$98ED` → `$9912`)

| Sprite | Object | Touch by hero | Laser |
|---|---|---|---|
| 3 | laser beam (on while fire is held, `$9033`) | — | — |
| 4, 5 | spider / bat / moth / snake (`$8BE3`, moved by `$8C5F`) | death (`$7B`) | killed, +50 (`$99A0`) |
| 6 | lantern (`$CC`… frames), raft (`$E9`), tentacle | lantern → lights out (`$6D`, `$8E9A`/`$8F37`) | — |
| 7 | miner (`$89BD`) / dynamite / debris | rescue → `$9C0B` | — |

- **Dynamite** (`$9300`): push down while standing. Timer 2 → `$937C` explode
  → `$93A4` flash → `$942D` destroys the wall (+75) / `$944B` kills the hero if
  too close.
- **Rescue** (`$9C0B`): +1000, then power and dynamite are counted into the
  score (`$A44E/$A462` per level) and the next level starts (`$84EE`). After
  level 20, levels 13–20 repeat. PRO mode (`$A7`) starts at level 17 and then
  picks the next level from the frame counter.
- **Death** (`$9B1D`/`$9B61`), **sinking** (`$9A83`, deep screens), **game over**
  (lives `$98` = 0 → attract mode).
- **Score** BCD `$53-$55` (`$86AB`). Extra life every 20000 (max 6).

## Attract mode / keys

- `$B1` = `$FF` in attract mode. Joystick input comes from the per-screen tables
  at `$9154`. The bottom row cycles logo / credits / "PRESS F1 TO START" (`$91DD`).
- F3 cycles the start level 1, 5, 9, 13, PRO (`$B4`/`$B7`/`$A7`). F1 starts.
  RUN/STOP pauses. RESTORE with RUN/STOP held restarts.
