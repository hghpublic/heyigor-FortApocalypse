# Session Log: convert fort3.s — Part 1: Analysis

## Task
Convert `fort3.s` (Fort Apocalypse VBlank interrupt driver, chopper/robot AI, rocket updates)
to C. Output files: `dev/src/fort3.h` and `dev/src/fort3.c`.
Source: 1048 lines of Atari 400/800 6502 SynAssembler.
Description in file header: "MAIN INTERRUPT DRIVER PART (I)".

---

## fort3.s — Source File Overview

| Assembly label | C function | Notes |
|---|---|---|
| VERTBLKD | `vertblkd()` | VBlank interrupt entry point |
| ROBOT.BRAINS | `robot_brains()` | Robot AI: spawn/fire/chase |
| ROB.X / ROB.Y | `rob_x[16]`, `rob_y[16]` (static) | Spawn positions |
| R.START / R.F / R.B / R.END | (body of `robot_brains()`) | Sub-sections |
| R.LEFT / R.RIGHT / R.DOWN / R.UP | static helpers | Sub-step movement |
| CHECK.CHR.I | `check_chr_i()` (static) | Glyph solid-test (interrupt-safe) |
| DO.CHOPPER | `do_chopper()` | Gravity, landing, collision dispatch |
| SAVE.POS | `save_pos()` | Copy chopper pos to LAND.* vars |
| CHECK.LAND | `check_land()` (static) | Landing/hyperspace tile test |
| DO.ROBOT.CHOPPER | `do_robot_chopper()` | Robot pixel-position + collision |
| UPDATE.CHOPPER | `update_chopper()` | VBlank: clear/draw chopper sprite |
| CCXY | `ccxy()` (static) | Chopper pixel → tile coordinate |
| UPDATE.ROBOT.CHOPPER | `update_robot_chopper()` | VBlank: clear/draw robot sprite |
| UPDATE.ROCKETS + CHECK.ROCKET.COL + MOVE.ROCKETS | `update_rockets()` | Combined collision+sprite+motion |
| ROCKET.EXP | `rocket_exp()` (static) | Restore tiles after explosion delay |
| HIT.LIST / TANK.SHAPE | `hit_list[22]` (public) | Collision tile table |
| ROCKET.DX/DY, ROCKET1/2/3.MASK | const arrays | Rocket motion + bitmask tables |

---

## Key Technical Concepts

### Atari PM Graphics Memory Layout

`player_base[]` is a 2KB page-aligned buffer. Offsets:
```
+0x300 = MIS (missile data: 256 bytes, 4 missiles × 2 bits per row)
+0x400 = PL0 (player 0, 256 bytes: one byte = one raster row)
+0x500 = PL1
+0x600 = PL2
+0x700 = PL3
```

Each player byte is one screen row. Sprite Y = byte index into the player data.
In C: `PLAYER_PL0[y] = sprite_byte` draws at row y.

### Hardware Register Collision Encoding

GTIA collision registers at $D004–$D00F (read-only):
- `P0PF`–`P3PF` ($D004–D007): player→playfield collision bits
- `M0PL`–`M3PL` ($D008–D00B): missile→player collision bits
- `P0PL`–`P3PL` ($D00C–D00F): player→player collision bits

**Critical:** `$D00C` is `P0PL` (read) AND `SIZEM` (write) — same address, different function
by direction. In fort3.c: `#define P0PL_HW REG(0xD00C)` (read) and `#define SIZEM_HW REG(0xD00C)` (write). Both are valid — they don't interfere since the collision registers are read first and SIZEM is written later.

### CHOPPER.COL Encoding (Assembly, lines 170–310)

```asm
LDA M2PL; ORA M3PL; AND #$03   ; missiles 2+3 hit any player → bits 0-1
ORA P0PL; ORA P1PL               ; player 0+1 collisions → up to bits 0-3
ASL; ASL; ASL; ASL               ; shift ALL of this to upper nibble
ORA P0PF; ORA P1PF               ; playfield hits → lower nibble bits 0-3
STA CHOPPER.COL
```

Result: upper nibble = missile/player-player hits (×16); lower nibble = playfield hits.
In C:
```c
chopper_col = (uint8_t)(
    (uint8_t)(((M2PL_HW | M3PL_HW) & 0x03u) | P0PL_HW | P1PL_HW) << 4
    | P0PF_HW | P1PF_HW);
```
Parenthesis placement is critical: all terms must be inside the final `(uint8_t)(...)` cast.

### ROBOT.COL Encoding

```asm
LDA M0PL; ORA M1PL; AND #$0C   ; missiles 0+1 hit players 2+3 → bits 2-3
ORA P2PL; ORA P2PF; ORA P3PL; ORA P3PF
STA ROBOT.COL
```
In C (same cast-placement concern):
```c
robot_col = (uint8_t)(
    ((M0PL_HW | M1PL_HW) & 0x0Cu)
    | P2PL_HW | P2PF_HW | P3PL_HW | P3PF_HW);
```

### `.I` Suffix — Interrupt-Safe Variables

Main game loop uses `ADR1`, `TEMP1`–`TEMP4` freely without saving them. The VBlank
handler (fort3.s) cannot safely clobber these. Solution: separate `.I` copies used
exclusively in interrupt context:

| Assembly | C | Used by |
|---|---|---|
| ADR1.I | `adr1_i_lo`, `adr1_i_hi` | check_chr_i, do_chopper, update_rockets |
| ADR2.I | `adr2_i_lo`, `adr2_i_hi` | check_chr_i (glyph pointer) |
| TEMP1.I–TEMP4.I | `temp1_i`–`temp4_i` | All interrupt-context helpers |

In C: `adr1_i_ptr()` reconstructs a pointer from `adr1_i_hi:adr1_i_lo`.

### CHOPPER.SHAPES Pointer Array

Defined in fort6.s: 18 pointers, one per angle 0–17. Each points to a 36-byte block:
- bytes 0–17: PL0 (left half of chopper/robot sprite)
- bytes 18–35: PL1 (right half)

Both chopper (PL0/PL1) and robot (PL2/PL3) use the same shape table.
Declared in fort3.c: `extern const uint8_t * const chopper_shapes[18]`.

### HIT.LIST — Three Labels, One Array

The assembly has three named labels pointing into one contiguous data block:
```asm
HIT.LIST:       .HS 40 5B5C5D5E5F 3B3C3D3E494A     ; bytes 0-11 (12 bytes)
TANK.SHAPE:     .AT -/lmnop/                        ; bytes 12-16 (5 bytes, Atari codes $4C-$50)
                .DA #MISS.LEFT,#MISS.RIGHT           ; bytes 17-18 ($71, $72)
HIT.LIST.LEN    .EQ *-HIT.LIST-1   (= 18)
                .AT /a /                            ; bytes 19-20 ($61, $20)
                .DA #EXP                            ; byte 21 ($20)
HIT.LIST2.LEN   .EQ *-HIT.LIST-1   (= 21)
```

In C:
```c
const uint8_t hit_list[22] = { ... };
#define HIT_LIST_LEN   18u
#define HIT_LIST2_LEN  21u
#define TANK_SHAPE     (hit_list + 12u)
```
Collision scan loops run `for i = HIT_LIST_LEN downto 0` (19 entries, inclusive).

### Rocket Missile-Plane Bit Packing

Three rockets share one 8-bit missile byte per row, using 2-bit fields:
- Rocket 0: bits 1:0 — `rocket1_mask[0]=0xFC` (AND-clear), `rocket2_mask[0]=0x03` (OR-set)
- Rocket 1: bits 3:2 — `rocket1_mask[1]=0xF3`, `rocket2_mask[1]=0x0C`
- Rocket 2: bits 5:4 — `rocket1_mask[2]=0xCF`, `rocket2_mask[2]=0x30`

Four missile rows are written per rocket: offsets 0,1 (head) and 4,5 (tail).
Direction 3 (straight down) uses only offsets 0,1 (no tail).

`SSIZEM` is a shadow of the SIZEM hardware register, updated each frame via AND/OR with the masks. SIZEM controls missile pixel width.

### EOR #$FE — Approximate Absolute Value

The robot spawn distance check uses `EOR #-2` (= `EOR #$FE`):
```asm
LDA R.X; SEC; SBC CHOP.X; BPL .4; EOR #-2
.4: CMP #34; BGE .6   ; if |dx| >= 34: far enough
```
`EOR #$FE` is NOT a true absolute value. For negative `d`: result = `~d & 0xFE` = `(-d-1) & 0xFE`. It gives slightly less than `|d|` for negative values. In C:
```c
if ((int8_t)dx < 0) dx ^= 0xFEu;
```
This matches assembly behaviour exactly (not `dx = -dx`).

### Robot Spawn Table Indexing

Assembly:
```asm
LDA RANDOM; AND #7; LDX LEVEL; DEX; BNE .3; CLC; ADC #8
```
`DEX` after `LDY LEVEL`: BNE skips `ADC #8` when `LEVEL != 1`; takes the add when `LEVEL == 1`.
- LEVEL == 1 (fort level): uses spawn positions 8–15 (near fort area, higher X)
- LEVEL != 1: uses positions 0–7 (more central)

### Robot Fire Direction Table Reconstruction

Assembly maps even ROBOT.ANGLE values (0, 2, 4, ..., 16) to rocket directions 1–5 via a
chain of CMP/BLT/BGE/SBC instructions. The 9 output values (for index 0–8 = angle/2):
```c
static const uint8_t fire_dir[9] = { 1u, 1u, 2u, 3u, 3u, 3u, 4u, 5u, 5u };
uint8_t dir_idx = (uint8_t)((robot_angle & 0x1Eu) >> 1);
rocket_status[2] = fire_dir[dir_idx];
```
Verified by tracing each branch in the assembly (CMP #4 → < 4 = indices 0-1 = dir 1;
CMP #6 → = 4,5 first go to `.60` with value 3 then clamped; CMP #6 after subtraction, etc.).

### CCXY Constant: 88 = 76+12

The assembly has `SBC #76+12` (the assembler evaluates `76+12=88`). This is the pixel Y
offset from sprite origin to the top of the visible game area, matching the display list
layout. In C: `chopper_y - 88u`.

### CHECK.LAND — Three-Row Scan via Page Advance

`do_chopper()` checks three consecutive map rows at `chop_x`:
1. Calls `compute_map_adr_i()` once to set `adr1_i`
2. Calls `check_land()` — reads `adr1_i_ptr()[0]`
3. `adr1_i_hi++` — advances to next page = next map row (map is page-based: each row = 256 bytes)
4. Calls `check_land()` again
5. `adr1_i_hi++` again; calls `check_land()` a third time

`check_land()` does NOT call `compute_map_adr_i()` — it uses the existing `adr1_i` pointer directly.

---

## External Dependencies Declared in fort3.c

### From fort6.s (chopper shape data)
- `chopper_shapes[18]` — shape pointer table

### From fort5.s (landing + slave pickup)
- `land_chr[5]` — landing tile characters (LAND.CHR)
- `pick_up_slave()` — returns `int`: 1 = slave picked up, 0 = not

### From fort4.s (map + sprite positioning)
- `pos_chopper()`, `pos_robot()` — sprite positioning
- `compute_map_adr_i()` — (temp1_i, temp2_i) → adr1_i
- `do_numbers()`, `draw_map()`, `read_trig()`, `do_exp()`
- `do_laser_1/2()`, `do_blocks()`, `do_elevator()`, `read_stick()`
- `inc_score(uint8_t hi, uint8_t lo)`

### From fort7.s zero-page (game state)
All interrupt-context variables: `adr1_i_lo/hi`, `adr2_i_lo/hi`, `temp1_i`–`temp4_i`,
plus all game state: `mode`, `chopper_status/x/y/angle`, `ochopper_y`, `sx/sy/sx_f/sy_f`,
`chop_x/chop_y`, `robot_status/x/y`, `orobot_y`, `r_status/angle/spd/fx/fy/x/y`,
`rocket_status/x/y/tim/temp/tempx/tempy[3]`, `orocket_y[3]`, `tim3/7/8_val`,
`level`, `fuel_status`, `fort_status`, `grav_skl`, `ssizem`, `s2/3/5_val`,
`bak2_color`, `land_fx/fy/x/y/chop_x/chop_y/chop_angle`
