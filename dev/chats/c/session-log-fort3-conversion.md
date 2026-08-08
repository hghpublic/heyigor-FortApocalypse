# Session Log: fort3.s → C Conversion
**Date:** 2026-08-07 / 2026-08-08  
**Branch:** agents/asm-to-c-conversion  
**Working directory:** `.worktrees/asm-to-c-conversion`  
**Prior context:** fort1.s, fort2.s already converted (fort1.c/h, fort2.c/h exist).

---

## Goal

Convert `fort3.s` (1048-line Atari 6502 SynAssembler source, "MAIN INTERRUPT DRIVER PART I") to idiomatic C, producing:

- `dev/src/fort3.h` — public API + shared data declarations
- `dev/src/fort3.c` — full implementation

The constraints:
- Target is Atari 400/800 6502, 8-bit values everywhere.
- Must compile clean with `cc -std=c11 -Wall -Wextra -fsyntax-only`.
- No malloc, no standard library beyond `<stdint.h>`.
- Hardware registers accessed via volatile pointer casts (`REG(addr)` macro).
- Faithfully reproduce assembly semantics, not rewrite game logic.

---

## Source File Overview — fort3.s

The file contains these sections in order (assembly labels → C functions):

| Assembly label        | Lines      | C function              | Notes                                         |
|-----------------------|-----------|-------------------------|-----------------------------------------------|
| `VERTBLKD`            | 130–840   | `vertblkd()`            | VBlank interrupt handler entry point          |
| `ROBOT.BRAINS`        | 860–1410  | `robot_brains()`        | Robot AI: spawn, rate-limit, chase, fire      |
| `ROB.X / ROB.Y`       | 1430–1490 | static arrays `rob_x[]`, `rob_y[]` | 16 spawn positions       |
| `R.START/R.F/R.B/R.END` | 1500–2690 | (body of `robot_brains()`) | fire + boundary + chase sections          |
| `R.LEFT/R.RIGHT/R.DOWN/R.UP` | 2710–3660 | static helpers | Sub-step movement with collision checks |
| `CHECK.CHR.I`         | 3670–3950 | `static check_chr_i()`  | Map glyph solid-test                         |
| `DO.CHOPPER`          | 3960–4850 | `do_chopper()`          | Chopper physics: gravity, landing, collision  |
| `SAVE.POS`            | 4860–5010 | `save_pos()`            | Copy current pos to LAND.* variables          |
| `CHECK.LAND`          | 5030–5170 | `static check_land()`   | Identify landing tile vs. hyperspace tile     |
| `DO.ROBOT.CHOPPER`    | 5180–6020 | `do_robot_chopper()`    | Robot pixel-position + collision dispatch     |
| `UPDATE.CHOPPER`      | 6030–7370 | `update_chopper()`      | VBlank: clear/draw chopper sprite, crash logic |
| `CCXY`                | 7120–7370 | `static ccxy()`         | Compute CHOP.X/Y from pixel coords            |
| `UPDATE.ROBOT.CHOPPER`| 7380–8170 | `update_robot_chopper()` | VBlank: clear/draw robot sprite              |
| `UPDATE.ROCKETS / CHECK.ROCKET.COL / MOVE.ROCKETS` | 8180–9830 | `update_rockets()` | Combined collision + sprite + motion |
| `ROCKET.EXP`          | 9840–10130 | `static rocket_exp()`  | Restore tiles after explosion delay          |
| Data tables           | 10140–10450| const arrays            | `hit_list`, `rob_x/y`, `rocket_dx/dy`, masks |

---

## Technical Concepts Established Before Writing

### Atari PM graphics memory layout

`player_base` is a 0x800-byte page-aligned buffer. Offsets:
- `+0x300` = MIS (missile data, 256 bytes)
- `+0x400` = PL0 (player 0, 256 bytes)
- `+0x500` = PL1
- `+0x600` = PL2
- `+0x700` = PL3

Each player/missile byte is one row; the sprite Y coordinate is the byte index into the player data.

### Hardware register collision layout

The Atari GTIA collision registers report bit flags. At address 0xD004-0xD00F:
- `P0PF`..`P3PF` (0xD004–D007): player→playfield collisions
- `M0PL`..`M3PL` (0xD008–D00B): missile→player collisions  
- `P0PL`..`P3PL` (0xD00C–D00F): player→player collisions

**Critical gotcha:** `SIZEM` (write, 0xD00C) occupies the **same address** as `P0PL` (read). The REG() macro is used for both; the compiler cannot know which; the C code must be written so writes to SIZEM and reads of P0PL do not interfere (they do not in the original assembly, which reads collision registers then writes SIZEM later).

### CHOPPER.COL encoding (line 190–310 of fort3.s)

```asm
LDA M2PL; ORA M3PL; AND #$03    ; missiles 2+3 hit any player → bits 0-1
ORA P0PL; ORA P1PL               ; players 0+1 hit each other → bits 0-3
ASL×4                             ; shift entire result to upper nibble
ORA P0PF; ORA P1PF               ; players 0+1 hit playfield → bits 0-3
STA CHOPPER.COL
```

So `CHOPPER.COL` upper nibble = missile/player-player hits (× 16), lower nibble = playfield hits.

The C translation:
```c
chopper_col = (uint8_t)(
    (uint8_t)(((M2PL_HW | M3PL_HW) & 0x03u) | P0PL_HW | P1PL_HW) << 4
    | P0PF_HW | P1PF_HW);
```

### ROBOT.COL encoding

```asm
LDA M0PL; ORA M1PL; AND #$0C    ; missiles 0+1 hit players 2+3 → bits 2-3
ORA P2PL; ORA P2PF; ORA P3PL; ORA P3PF
STA ROBOT.COL
```

The C translation — parenthesis placement critical, all OR terms must be inside the `(uint8_t)(...)` cast:
```c
robot_col = (uint8_t)(
    ((M0PL_HW | M1PL_HW) & 0x0Cu)
    | P2PL_HW | P2PF_HW | P3PL_HW | P3PF_HW);
```

### `.I` suffix variables

Interrupt-safe copies: `adr1_i_lo/hi`, `adr2_i_lo/hi`, `temp1_i`–`temp4_i`. These exist because the main game loop uses `ADR1`, `TEMP1`, etc. without saving, and the interrupt handler cannot safely trash them. The `.I` copies are dedicated to interrupt context.

`adr1_i_ptr()` helper:
```c
static inline uint8_t *adr1_i_ptr(void) {
    return (uint8_t *)(uintptr_t)((uint16_t)adr1_i_hi << 8 | adr1_i_lo);
}
```

### CHOPPER.SHAPES pointer array

`CHOPPER.SHAPES` (defined in fort6.s): 18 pointers (angle 0–17), each pointing to a 36-byte block: bytes 0–17 = PL0 (left half), bytes 18–35 = PL1 (right half). The robot uses the same shape table with its own angle. Declared as:
```c
extern const uint8_t * const chopper_shapes[18];
```

---

## Function-by-Function Analysis and Translation Decisions

### `vertblkd()`

Straightforward: read collision regs, call game functions in sequence. The `SEI/PHP/CLD` prologue and `PLP/CLI/JMP VVBLKD.RET` epilogue are OS interrupt mechanics; in C, these are replaced by a normal function call (the OS handles context save/restore around the deferred interrupt).

The commented-out debug probe code (lines 640–810: writing CHOP.X/Y etc. to a scratch page) is omitted.

### `robot_brains()` — spawn counter, distance check, fire direction table

**Spawn path analysis.** The assembly structure is:

```asm
ROBOT.BRAINS:
    LDA R.STATUS; CMP #OFF; BEQ .1    ; if OFF → goto .1
    CMP #CRASH;  BEQ .2               ; if CRASH → goto .2 (RTS)
    LDA FRAME; AND ROBOT.SPD; BEQ R.START ; if active+unmasked → goto R.START
    RTS
    ; (dead code follows — never reached)
    LDA TIM7.VAL; BEQ .0              ; DEAD — this BEQ .1 path is active below
.1: DEC TIM7.VAL; BNE .2             ; decrement timer; if not zero → RTS (.2)
.0: LDA #$88 ...                      ; timer hit 0: spawn attempt
```

The lines `LDA TIM7.VAL; BEQ .0` between the `RTS` and the `.1` label are **unreachable dead code**. The `BEQ .1` at the top jumps directly to `DEC TIM7.VAL` (the `.1` label IS at line 980, not at the LDA above it). Translated faithfully:

```c
if (st == STATUS_OFF) {
    if (--tim7_val != 0u) return;   // .1: decrement; BNE .2 (return)
    // .0: timer expired — attempt spawn
    ...
}
```

**Distance check using `EOR #$FE` (`EOR #-2`).** The assembly uses `EOR #-2` (= `EOR #$FE`) as an approximate absolute value. This is NOT a true abs. For a negative value `d`:
- `d EOR $FE` flips all bits except bit 0.
- Equivalent to: `~d & 0xFE`, which equals `(-d - 1) & 0xFE`.
- For even negative d: gives `(-d - 2)` (slightly less than abs).
- For odd negative d: gives `(-d - 1)` (slightly less than abs).

The result is slightly-less-than-abs, never more. Translated faithfully:
```c
if ((int8_t)dx < 0) dx ^= 0xFEu;
```

**Spawn table indexing:** Assembly `LDX LEVEL; DEX; BNE .3` means:
- If `LEVEL == 1` → DEX makes X=0 → BNE not taken → `ADC #8` → use indices 8–15.
- If `LEVEL != 1` (e.g., 0) → BNE taken → skip `ADC #8` → use indices 0–7.

```c
uint8_t idx = RANDOM_HW & 7u;
if (level == 1u) idx = (uint8_t)(idx + 8u);
```

**Fire direction table.** The assembly computes rocket direction from even ROBOT.ANGLE via a sequence of comparisons. Reconstructed as a 9-entry lookup table (index = angle/2, covering 0–8 representing angles 0,2,4,...,16):
```c
static const uint8_t fire_dir[9] = { 1u,1u,2u,3u,3u,3u,4u,5u,5u };
uint8_t dir_idx = (uint8_t)((robot_angle & 0x1Eu) >> 1);
rocket_status[2] = fire_dir[dir_idx];
```
Verified against the assembly's CMP #4 / CMP #6 / CMP #6 / SBC #2 / CMP #6 / CMP #0 chain.

**Boundary + chase logic.** The `R.B` section handles left/right boundaries (R.X == 48 or 216) separately from open-field chase. When at a boundary:
1. Every 4 frames, steer angle toward center (angle < 4 or ≥ 14 → nudge by ±2).
2. If ROBOT.STATUS == OFF (not yet on screen), snap R.X to the just-inside boundary value.
3. Skip horizontal chase; fall through to vertical chase.

```c
if (at_boundary) {
    if ((FRAME_HW & 3u) == 0u) {
        uint8_t ang = robot_angle;
        if (ang < 4u || ang >= 14u) {
            if (ang < 8u) { robot_angle += 2u; robot_angle += 2u; }
            else          { robot_angle -= 2u; robot_angle -= 2u; }
        }
    }
    if (robot_status == STATUS_OFF) r_x = snap_x;
    // fall through to vertical chase
} else {
    if (chop_x != r_x) {
        if (chop_x < r_x) r_left(); else r_right();
    }
}
// vertical chase always runs:
if (chop_y != r_y) { ... }
pos_robot();
```

**R.END angle clamping.** After all movement:
1. If ROBOT.ANGLE is negative (≥ 128 as uint8): clamp to 0.
2. If ≥ 18: clamp to 16.
3. Restore oscillation bit (bit 0) saved in temp4_i.
4. Mask R.FX to 2 bits, R.FY to 3 bits.

### `check_chr_i()` (static)

Reads map byte at (`temp1_i`, `temp2_i`) via `compute_map_adr_i()` → `adr1_i_ptr()`. Strips reverse-video bit (`& 0x7F`). Multiplies by 8 to get glyph offset in `CHR_SET2` (at 0x0C00). Scans 8 rows (index 7 down to 0) — any non-zero row = solid, returns 1 (carry set). Returns 0 otherwise.

The assembly uses ADR2.I for the glyph pointer; in C we use a local pointer — equivalent and simpler.

### `r_left()`, `r_right()`, `r_down()`, `r_up()` (static)

Each checks 2–4 positions (current + forward cells) for solid tiles before committing to the move. If any collision: return without moving. The fraction counters (`r_fx`, `r_fy`) wrap to adjust the tile counter (`r_x`, `r_y`).

**`r_up()` is different** — skips collision checks if `r_y < 3` (near top of map). This prevents the subtraction in `TEMP2.I - 3` from underflowing into invalid map rows.

### `check_land()` (static)

Called three times from `do_chopper()` with `adr1_i_hi` incremented between calls (checking three consecutive map rows at chop_x). Reads the map byte via `adr1_i_ptr()` (not `compute_map_adr_i()` — the pointer is already set). Compares against `LAND.CHR[0..LAND_LEN]`. If a landing tile: return immediately (temp3_i unchanged). If `$48` (hyperspace tile): increment `temp4_i`, return. Otherwise: increment `temp3_i` (non-landing).

### `do_chopper()` — detailed collision dispatch

The assembly collision dispatch is a dense tangle of `BEQ`/`BNE`/`JMP` labels. Fully traced:

```
.3:  LDA CHOPPER.COL
     BEQ .12         → col==0: goto .12 (= set FLY, RTS)
     CMP #4; BEQ .20 → col==4: goto .20 (= JMP .4 = crash)
     CMP #8; BNE .6  → col==8: if col!=8 goto .6 (playfield check)
     ; col==8 (hyperspace):
     LDA LEVEL; BNE .20   → if level!=0: crash
     STA CHOPPER.COL; ...
.12: LDX #FLY; BNE .5 FORCED → always taken (FLY≠0); .5: STX CHOPPER.STATUS; RTS
```

So `.12` is a **fall-through target** reached from two places:
1. When `col == 0` (BEQ .12).
2. After the hyperspace setup (fall-through from `STA S5.VAL`).

Both set STATUS_FLY and return.

```c
if (col == 0u) { chopper_status = STATUS_FLY; return; }
if (col == 4u) goto crash;
if (col == 8u) {
    if (level != 0u) goto crash;
    chopper_col = 0u;
    mode   = HYPERSPACE_MODE;
    s3_val = 1u; s5_val = 1u;
    chopper_status = STATUS_FLY;
    return;
}
// .6: playfield collision — check tiles
```

After the tile-check section (three `check_land()` calls), the code checks `CHOPPER.COL & $F0` for missile/player hits (crash regardless of tiles), then bit 1 for slave pickup, then `temp3_i == 3` for all-non-landing (crash), then landing logic.

**pick_up_slave return convention.** The assembly has `JSR PICK.UP.SLAVE; BCC .9`. Carry set = slave picked up. Translated as `extern int pick_up_slave(void)` returning 1 (carry set) or 0 (carry clear). The fort5.c implementation must match this.

**Landing height check:** `CMP #10+4; BLT .8` — if `chop_y < 14`, skip fuel check (safe height). If `chop_y >= 14` and fuel is EMPTY → crash. Otherwise can land.

### `do_robot_chopper()`

Converts tile coordinates (`r_x`, `r_y`, `r_fx`, `r_fy`) to pixel coordinates (`robot_x`, `robot_y`), with off-screen detection for [SX, SX+48) × [max(SY,0), max(SY,0)+19).

**Robot Y formula:**
```asm
LDA R.Y
LDY SY; BPL .2; LDY #0
.2: SEC; SBC TEMP1.I   ; R.Y - max(SY,0)
ASL×3                   ; × 8
ADC #71+12              ; base offset 83
STA TEMP1.I
LDA SY.F; EOR #$FF; AND #7  ; ~SY.F & 7 (scroll sub-pixel)
ADC TEMP1.I
LDY SY; BPL .3; ADC #8      ; if SY<0: +8
.3: ADC R.FY
STA ROBOT.Y
```

In C:
```c
uint8_t sy_clamp = ((int8_t)sy < 0) ? 0u : sy;
uint8_t py = (uint8_t)(((uint8_t)(r_y - sy_clamp) << 3) + (71u + 12u));
py = (uint8_t)(py + ((sy_f ^ 0xFFu) & 7u));
if ((int8_t)sy < 0) py = (uint8_t)(py + 8u);
py = (uint8_t)(py + r_fy);
robot_y = py;
```

### `update_chopper()`

**BEGIN path:** The `STATUS_BEGIN` state (first frame of new life) causes `CCXY` to be called directly — **without** a preceding `pos_chopper()`. This is intentional: the chopper's absolute position needs to be established from its sprite coordinates before calling pos_chopper. The normal flow (GO_MODE path at the end of the function) calls `pos_chopper()` first, then falls into `CCXY`.

Translated as:
```c
if (cs == STATUS_BEGIN) {
    chopper_status = STATUS_FLY;
    ccxy();   // no pos_chopper() before this
    return;
}
...
if (mode != GO_MODE) return;
pos_chopper();  // normal GO_MODE path: pos_chopper first
ccxy();
```

**CRASH disintegration:** 18 rows from `chopper_y` are AND-masked with RANDOM. Colors cycle. Every other frame `chopper_y++` (sinking animation). When `tim3_val` hits 0: if robot is alive, clear it; call `pos_chopper()` to blank its sprite; switch to `NEW_PLAYER_MODE`.

**Oscillation:** Every 4 frames, `chopper_angle ^= 1`. This toggles the oscillation bit (the even/odd frame variant of the current angle's animation).

### `ccxy()` (static)

```c
static void ccxy(void) {
    chop_x = (uint8_t)(((uint8_t)(chopper_x - 24u) >> 2) + sx);
    uint8_t cy = (uint8_t)((uint8_t)(chopper_y - 88u) >> 3);
    temp1_i = cy;
    uint8_t sy_base = ((int8_t)sy >= 0) ? sy : 0u;
    chop_y = (uint8_t)(sy_base + temp1_i);
    pos_chopper();
}
```

Note: `chopper_y - 88` = `chopper_y - (76+12)`. The constant is 76+12=88.

### `update_robot_chopper()`

When `ROBOT.STATUS == OFF`, robot_x and robot_y are zeroed (sprite effectively hidden), then the clear/draw loop still runs (harmless since y=0 writes to the first rows of PM RAM, which are normally blank). This matches the assembly: there's no early `RTS` in the `OFF` path, just the zero-assignment and fall-through.

When `R.STATUS == CRASH`: disintegrate 18 rows at `robot_y` with RANDOM AND, cycle PCOLR2/3. When `tim7_val` hits 0: set R.STATUS=OFF, call `pos_robot()`, set `tim7_val=255` (long delay before respawn).

### `update_rockets()` — combined loop

The assembly uses a single loop `NXT.RCK … DEX; BMI ROCKET.EXP; JMP NXT.RCK` running from X=2 down to X=0. Three logical sections per iteration:

1. **CHECK.ROCKET.COL** (lines 8200–9020): if status≠0 and ≠7, compute map tile, check for collision.
2. **MOVE.ROCKETS** (lines 9040–9820): clear old sprite, draw new sprite, optionally advance position.
3. Then after the loop: **ROCKET.EXP** (lines 9840–10130): handle explosion countdowns.

**Rocket collision cases:**

```
tile == EXP2 ($3F) AND level==1: fort hit → FORT.STATUS=EXPLODE, ROCKET.STATUS=OFF, all ROCKET.TEMP=0
tile == EXP.WALL ($C7):          score, ROCKET.TEMP[x]=0, ROCKET.STATUS=7 (normal explo)
tile in HIT.LIST:                 ROCKET.STATUS=OFF (entity killed)
other solid tile:                 ROCKET.STATUS=7 (normal explosion)
```

After the status is set, regardless of case: write EXP character ($20) at hit location, set ROCKET.TIM=7.

**Implementation with `new_status` variable:**
```c
uint8_t new_status = 7u;  // default: normal explosion

if (tile == 0x3Fu) {
    if (level == 1u) {
        rocket_temp[0]=0; rocket_temp[1]=0; rocket_temp[2]=0;
        fort_status = STATUS_EXPLODE;
        new_status = 0u;
        goto rocket_col_done;
    }
    // wrong level: fall through to HIT.LIST check
}
if (tile == 0xC7u) {
    bak2_color = 0x10u;
    inc_score(0x20u, 0x00u);
    rocket_temp[(uint8_t)x] = 0u;
    new_status = 7u;
    goto rocket_col_done;
}
for (int i = (int)HIT_LIST_LEN; i >= 0; i--) {
    if (tile == hit_list[i]) { new_status = 0u; break; }
}
rocket_col_done:
rocket_status[x] = new_status;
adr1_i_ptr()[0] = 0x20u;
rocket_tim[x] = 7u;
```

**MOVE.ROCKETS unconditional draw.** All paths (OFF, EXP, out-of-bounds) flow to the sprite draw section. The assembly does:

```asm
.2  ; OFF path: STA ROCKET.STATUS,X; fall through to:
.3  ; EXP path (and OFF fall-through): zero X, set Y=$F0; fall to .5:
.5  ; draw section
```

So OFF and EXP both set `rocket_x[x]=0, rocket_y[x]=$F0`, then draw. Since `$F0` (240) is below the screen's visible area, the draw at y=$F0 is harmless. In bounds-check failure: also goes to `.2` (sets OFF), falls to `.3`, sets $F0, then draws.

The C `clear_pos` boolean captures this:
```c
int clear_pos = 0;
if (status == 0u) {
    clear_pos = 1;
} else if (status != 7u) {
    SIZEM_HW = ssizem;
    if (out_of_bounds) { rocket_status[x] = 0u; clear_pos = 1; }
} else {
    clear_pos = 1; // EXP
}
if (clear_pos) { rocket_x[x] = 0u; rocket_y[x] = 0xF0u; }
// draw always runs after this
```

**Missile plane bit packing.** Three rockets share one missile byte via bit-pairs:
- Rocket 0: bits 1:0 (mask $03, SSIZEM bit 0)
- Rocket 1: bits 3:2 (mask $0C, SSIZEM bit 2)
- Rocket 2: bits 5:4 (mask $30, SSIZEM bit 4)

Four rows are drawn per rocket (offset 0,1 = head; offset 4,5 = tail) using `ROCKET2.MASK` (OR). The SSIZEM shadow mirrors the missile-size hardware register.

Direction 3 (straight down) uses only 2 rows (offset 0,1 only) — tail omitted. Hence the `if (rocket_status[x] != 3u)` check before drawing the second pair.

### `rocket_exp()` (static)

Scans x=2..0 for rockets in status==7 (EXP). Decrements `rocket_tim[x]`; when it reaches 0, restore the original tile — unless it was EXP ($20, already destroyed) or an entity tile in HIT.LIST (entities should stay gone). Then set `rocket_status[x] = 0` (OFF).

---

## Data Tables

### `hit_list[22]`

This is one contiguous data block in the assembly with three named labels pointing into it:

```asm
HIT.LIST       .HS 405B5C5D5E5F 3B3C3D3E494A      ; indices 0-11
TANK.SHAPE     .AT -/lmnop/                        ; indices 12-16: Atari screen codes $4C-$50
               .DA #MISS.LEFT,#MISS.RIGHT           ; indices 17-18: $71, $72
HIT.LIST.LEN   .EQ *-HIT.LIST-1   (= 18)
               .AT /a /                            ; index 19-20: $61, $20
               .DA #EXP                            ; index 21: $20
HIT.LIST2.LEN  .EQ *-HIT.LIST-1   (= 21)
```

The `.AT` directive produces Atari internal screen codes. `-/lmnop/` = reverse-video lowercase l-p = Atari codes $4C-$50 with bit 6 set → stripped to $4C-$50 by the & 0x7F in collision checks. Confirmed by cross-reference with fort2.c hit_list/TANK_SHAPE usage.

**Why 22 bytes?** Indices 0–18 = `HIT.LIST.LEN`'s range (checked via `LDY #HIT.LIST.LEN; .2 CMP HIT.LIST,Y; DEY; BPL .2`). The loop at Y=18 checks index 18 then decrements to 17, 16, ..., 0 — covering 19 values (0–18 inclusive). Indices 19–21 extend to `HIT.LIST2.LEN` for other subsystems.

```c
const uint8_t hit_list[22] = {
    0x40,0x5B,0x5C,0x5D,0x5E,0x5F,   // 0-5
    0x3B,0x3C,0x3D,0x3E,0x49,0x4A,   // 6-11
    0x4C,0x4D,0x4E,0x4F,0x50,         // 12-16 TANK.SHAPE
    0x71,0x72,                         // 17-18 MISS.LEFT, MISS.RIGHT
    0x61,0x20,                         // 19-20
    0x20,                              // 21    HIT.LIST2.LEN=21
};
```

### Rocket mask tables

```c
static const uint8_t rocket1_mask[3] = { 0xFC, 0xF3, 0xCF };   // AND to clear bits
static const uint8_t rocket2_mask[3] = { 0x03, 0x0C, 0x30 };   // OR to set body
static const uint8_t rocket3_mask[3] = { 0x01, 0x04, 0x10 };   // OR to set size bit
```

The 4th entries (commented out as `$3F`, `$C0`, `$40`) would have been a 4th rocket; never implemented.

### Robot spawn tables

```c
static const uint8_t rob_x[16] = {
    0x84,0xC3,0x49,0xC3,0x49,0x84,0x84,0x84,   // indices 0-7: level!=1
    0xD7,0xD7,0xD7,0xD6,0xD6,0xD6,0x33,0x33,   // indices 8-15: level==1
};
```

Level 1 (fort level) uses positions 8–15 (higher X values, near fort area). Other levels use 0–7 (more central positions).

---

## Bugs Found and Fixed

### Bug 1: `do_chopper` col==0 wrongly fell into landing checks

**Root cause:** Initial translation used `goto no_col` when `col == 0`, where `no_col` jumped into the landing tile-check section. In the assembly, `BEQ .12` when col==0 jumps to `.12: LDX #FLY; BNE .5 FORCED; .5: STX CHOPPER.STATUS; RTS`. This sets STATUS_FLY and returns immediately — it does NOT run the landing tile checks.

**The confusion:** `.12` appears in the middle of the hyperspace setup code, visually close to the playfield collision section. Tracing the label carefully: the BEQ at line 4160 (`BEQ .12`) goes to line 4350, which is after all the tile-check code.

**Fix:**
```c
// WRONG:
if (col == 0u) goto no_col;
...
no_col:
    temp1_i = chop_x; ...  // tile checks still run — WRONG

// CORRECT:
if (col == 0u) { chopper_status = STATUS_FLY; return; }
```

### Bug 2: `pick_up_slave` declared `void` — can't use carry return

**Root cause:** `extern void pick_up_slave(void)` — assembly uses `JSR PICK.UP.SLAVE; BCC .9` to check if a slave was picked up.

**Fix:** Changed to `extern int pick_up_slave(void)` — returns 1 if slave picked up (carry set in asm), 0 otherwise. The corresponding fort5.c implementation must honour this convention.

### Bug 3: Fort explosion triggered on wrong level

**Root cause:** `if (level == 0u)` in the EXP2-tile check. Assembly:
```asm
LDY LEVEL; DEY; BNE .4
```
`DEY` after `LDY LEVEL`. BNE skips fort destruction when `Y ≠ 0` after decrement. Y becomes 0 only when `LEVEL == 1`. So fort is destroyed only on LEVEL == 1.

**Fix:** `if (level == 1u)`.

### Bug 4: `update_rockets` collision section — unreliable two-label structure

**Root cause:** Original code used two labels (`set_exp_tile`, `place_exp`) and an after-the-fact fixup `if (fort_status == STATUS_EXPLODE && ...) rocket_status[x] = 0u`. This was wrong: the fort explosion path sets `fort_status = STATUS_EXPLODE` but then fell through to `new_status = 7u` (normal explosion status), contradicting the assembly which explicitly branches to `BNE .3` (forced = goto `.3` which sets status=0 = OFF).

**Fix:** Rewrote the entire collision section with a single `new_status` variable initialized to 7, updated by each case, and a single `rocket_col_done:` label. Each case either sets `new_status` and/or `goto rocket_col_done`. Clean, matches assembly flow exactly.

### Bug 5: `vertblkd` robot_col — parenthesis misplacement

**Root cause:**
```c
// WRONG:
robot_col = (uint8_t)(M0PL_HW | M1PL_HW) & 0x0Cu)
            | P2PL_HW | P2PF_HW | P3PL_HW | P3PF_HW);
```
The `(uint8_t)(...)` cast closed after `& 0x0Cu`, then `P2PL_HW` etc. were ORed with an `int`-promoted value — the cast did not encompass them, so the final value could exceed 8 bits on platforms where `uint8_t` is promoted.

**Fix:**
```c
robot_col = (uint8_t)(
    ((M0PL_HW | M1PL_HW) & 0x0Cu)
    | P2PL_HW | P2PF_HW | P3PL_HW | P3PF_HW);
```

---

## Header File (`dev/src/fort3.h`)

```c
#pragma once
#include <stdint.h>

void vertblkd(void);
void robot_brains(void);
void do_chopper(void);
void do_robot_chopper(void);
void update_chopper(void);
void update_robot_chopper(void);
void update_rockets(void);
void save_pos(void);

extern const uint8_t hit_list[];
#define HIT_LIST_LEN   18u
#define HIT_LIST2_LEN  21u
#define TANK_SHAPE     (hit_list + 12u)

extern const int8_t rocket_dx[5];
extern const int8_t rocket_dy[5];
```

---

## Syntax Checks (Tests)

Three rounds of syntax checks performed:

**Round 1** (fort3.c alone):
```
cc -std=c11 -Wall -Wextra -fsyntax-only dev/src/fort3.c
```
→ Caught the `pick_up_slave` void declaration issue (value used from void function). Fixed.

**Round 2** (fort3.c after bug fixes):
```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort3.c
```
→ Passed clean.

**Round 3** (joint — fort2.c + fort3.c together):
```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort2.c dev/src/fort3.c
```
→ Passed clean. No conflicts between the two compilation units.

---

## Complete Final Source

### `dev/src/fort3.h`

```c
#pragma once
#include <stdint.h>

/* -----------------------------------------------------------------------
 * fort3.h — VBlank handler, chopper/robot AI, rocket updates
 *            (converted from fort3.s)
 * ----------------------------------------------------------------------- */

/* VBlank deferred interrupt handler */
void vertblkd(void);

/* Game-loop functions (called from VERTBLKD during GO_MODE) */
void robot_brains(void);
void do_chopper(void);
void do_robot_chopper(void);

/* VBlank sprite-update functions */
void update_chopper(void);
void update_robot_chopper(void);
void update_rockets(void);

/* Utility */
void save_pos(void);  /* copy current chopper position to LAND.* variables */

/* HIT.LIST data (also used by fort2.c) */
extern const uint8_t hit_list[];    /* 22 bytes; indices 0-18 = HIT.LIST.LEN range */
#define HIT_LIST_LEN   18u          /* HIT.LIST.LEN  = *-HIT.LIST-1 */
#define HIT_LIST2_LEN  21u          /* HIT.LIST2.LEN = *-HIT.LIST-1 */
#define TANK_SHAPE     (hit_list + 12u)   /* TANK.SHAPE label = HIT.LIST+12 */

/* Rocket motion tables — indexed by direction status 1-5 (use [status-1]) */
extern const int8_t rocket_dx[5];
extern const int8_t rocket_dy[5];
```

### `dev/src/fort3.c`

(See `dev/src/fort3.c` — ~957 lines. Complete listing is the file on disk.)

Key structural summary:

```
Includes & REG() macros
Hardware register defines (M2PL, P0PL/PF, HITCLR, SIZEM, HPOSP0/1, PCOLR0-3)
PM-RAM layout macros (PLAYER_PL0/1/2/3, PLAYER_MIS)
External variables (adr1_i_lo/hi, adr2_i_lo/hi, temp1_i-4_i, mode, chopper/robot state,
    rocket arrays, timers, level, fuel/fort status, sounds, land save, grav_skl, ssizem)
External functions (pos_chopper, pos_robot, compute_map_adr_i, pick_up_slave, inc_score,
    do_numbers, draw_map, read_trig, do_exp, do_laser_1/2, do_blocks, do_elevator, read_stick)
adr1_i_ptr() inline helper
Data tables: hit_list[22], rob_x/rob_y[16], rocket_dx/dy[5], rocket1/2/3_mask[3]
check_chr_i() — glyph solid test
r_left/r_right/r_down/r_up() — robot sub-step movement
check_land() — landing/hyperspace tile test
save_pos()
robot_brains()
do_chopper()
do_robot_chopper()
ccxy() static
update_chopper()
update_robot_chopper()
rocket_exp() static
update_rockets()
vertblkd()
```

---

## Remaining Extern Dependencies (Next Conversion Target)

`fort3.c` declares these externs that must be provided by other modules:

From **fort4.s** (not yet converted):
- `pos_chopper()` — position chopper sprite from `chopper_x/y`
- `pos_robot()` — position robot sprite from `robot_x/y`
- `compute_map_adr_i()` — compute `adr1_i` from (`temp1_i`, `temp2_i`)
- `draw_map()`
- `do_numbers()`
- `read_trig()`
- `do_exp()`
- `do_laser_1()`, `do_laser_2()`
- `do_blocks()`
- `do_elevator()`
- `read_stick()`
- `inc_score(uint8_t hi, uint8_t lo)`

From **fort5.s** (not yet converted):
- `pick_up_slave()` — returns `int` (1=found/carry, 0=not found)
- `land_chr[5]` — landing tile characters (LAND.CHR)

From **fort6.s** (not yet converted):
- `chopper_shapes[18]` — shape pointer table (CHOPPER.SHAPES)

From main **fort.s** / **fort7.s** (zero-page + main):
- All the `uint8_t` game state variables (`chopper_status`, `r_status`, `mode`, etc.)
- `player_base[]` — PM RAM buffer

---

## Notes for Future Sessions

1. **`pick_up_slave` calling convention** — declared `extern int pick_up_slave(void)`. The fort5.c implementation must return 1 on success, 0 on failure (matching assembly's carry-set/carry-clear convention).

2. **`compute_map_adr_i` side effects** — this function reads (`temp1_i`, `temp2_i`) as column and row, and writes the result pointer into `adr1_i_lo/hi`. After calling it, `check_chr_i()` and `check_land()` use `adr1_i_ptr()` directly (no recompute). The `do_chopper()` landing check increments `adr1_i_hi` between calls to advance to the next map row — this works because the map is organised as rows in separate 256-byte pages.

3. **CCXY constant: 88 = 76+12** — `chopper_y - 88` in `ccxy()`. The assembly literal is `76+12`; the Atari assembler evaluates this at assembly time. In C it's `88u` (or `(71u + 12u + 5u)`... actually `76+12=88`, matching `SBC #76+12` at line 7230).

4. **STATUS constants** (from fort1.h):
   ```
   STATUS_OFF=1, STATUS_ON=2, STATUS_FLY=3, STATUS_CRASH=4,
   STATUS_EXPLODE=5, STATUS_LAND=6, STATUS_BEGIN=7, STATUS_EMPTY=9,
   STATUS_REFUEL=10, STATUS_PICKUP=11
   ```
   GO_MODE=2, NEW_PLAYER_MODE=5, HYPERSPACE_MODE=10.

5. **Level semantics**: LEVEL=0 = underground/hyperspace section (hyperspace tile entry allowed). LEVEL=1 = fort level (fort can be destroyed by rockets, robot uses spawn positions 8–15).
