# Session Log — fort5.s → C Conversion  Part 1: Analysis
**Date:** 2026-08-08
**Branch:** `agents/asm-to-c-conversion`
**Working directory:** `heyigor-FortApocalypse.worktrees/asm-to-c-conversion`
**Task:** Convert `fort5.s` to `dev/src/fort5.h` + `dev/src/fort5.c`
**Prior session:** fort4.s → fort4.c (see `session-log-fort4-conversion.md`)
**Final compile test:** `cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c dev/src/fort5.c` — **clean, zero diagnostics**

---

## 1. Session Context

This session continued directly from the fort4.s conversion session (context-compacted before this work began). The chain of conversions so far:

| File | Purpose |
|------|---------|
| `fort1.s` | Game logic, modes, title screen |
| `fort2.s` | Pods, missiles, tanks, print utilities |
| `fort3.s` | VBlank, chopper/robot AI, rockets |
| `fort4.s` | Interrupt driver part II: scroll, joystick, lasers, numbers |
| **`fort5.s`** | **Slave AI, fuel base, scanner, fort explosion, DLI chain, sounds** |

`fort5.s` is approximately 9080 lines (including the data section) and is titled in the source as the module handling MOVE.SLAVES, CHECK.FORT, the display-list interrupt chain (LINE1–LINE4), and the full sound subsystem.

All public functions from fort5.s were already referenced as `extern` in the existing C files (`fort1.c`, `fort2.c`, `fort3.c`). fort5.h was created at the start of the session; fort5.c was written in one pass and compiled clean.

---

## 2. Files Read for Context Before Writing

### `dev/src/fort1.h`
Confirmed: MODE_* and STATUS_* constants, all function prototypes. Does not contain hardware macros.

### `dev/src/fort1.c` (extern survey)
Confirmed extern variables available to fort5.c:
- `adr1_lo`, `adr1_hi`, `adr2_lo`, `adr2_hi`
- `temp1`, `temp2`, `temp3`, `temp4`
- `mode`, `level`, `fort_status`, `fuel_status`, `fuel1`, `fuel2`
- `bonus1`, `bonus2`
- `chopper_status`, `chop_x`, `chop_y`
- `bak_color`
- `slave_status[8]`, `slave_x[8]`, `slave_y[8]`, `slave_dx[8]`
- `slave_num`, `slaves_left`, `slaves_saved`
- `land_x`, `land_y`, `land_fx`, `land_fy`
- `land_chop_x`, `land_chop_y`, `land_chop_angle`

### `dev/src/fort2.c` (extern survey)
Confirmed: `s1_1_val`, `s1_2_val`, `s4_val`, `s6_val`, `bak2_color`

### `dev/src/fort3.c` (extern survey)
Confirmed: `robot_x`, `s2_val`, `s3_val`, `s5_val`, `fort_status`, `level`, `rocket_x[3]`, `laser_status`

### `dev/src/fnt1.h`
Confirmed:
- `extern const uint8_t *const POS_MASK1` — 8-byte XOR mask for scanner pixel toggle
- `extern const uint8_t *const FORT_EX1`, `FORT_EX2`, `FORT_EX3`, `FORT_EX4` — explosion frame data pointers

### `fort7.s` (grep)
Confirmed new variables not yet declared in any C file:
- `SCAN.ADR1 .BS 2` — persistent 16-bit source address for DO.LINE
- `SCAN.ADR2 .BS 2` — persistent 16-bit dest address for DO.LINE
- `FUEL.TEMP .BS 1` — refuel animation step counter (4 down to -1)
- `TIM4.VAL .BS 1` — refuel frame timer
- `TIM9.VAL .BS 1` — slave info display timer

### `fort.s` (grep)
Confirmed:
- `TEMP5 .BS 1`, `TEMP6 .BS 1` — extra temp registers used by NEXT.PART1 loops
- Hardware equates: AUDF1=$D200, AUDC1=$D201, AUDF2=$D202, AUDC2=$D203, AUDF3=$D204, AUDC3=$D205, AUDF4=$D206, AUDC4=$D207
- CHBASE=$D409, WSYNC=$D40A, VDSLST=$0200/$0201
- COLBK=$D01A, COLPF0-3=$D016-D019
- HPOSP0=$D000, HPOSP2=$D002, HPOSP3=$D003
- PLAY.SCRN=$0300, SCANNER=$39C0
- S.LINE1=$0AE0 (CHR.SET1+736), S.LINE2=$0B40 (CHR.SET1+832), S.LINE3=$0BA0 (CHR.SET1+928)
- WINDOW.1 = CHR.SET2+712

---

## 3. New Variables Introduced in fort5.c

These are declared `extern` in fort5.c and must be defined in a future fort7.c (the BSS/variable definitions file):

| C name | Assembly name | Size | Purpose |
|--------|--------------|------|---------|
| `scan_adr1_lo`, `scan_adr1_hi` | `SCAN.ADR1` | 2 bytes | Persistent map read pointer for DO.LINE |
| `scan_adr2_lo`, `scan_adr2_hi` | `SCAN.ADR2` | 2 bytes | Persistent scanner write pointer for DO.LINE |
| `fuel_temp` | `FUEL.TEMP` | 1 byte | Refuel animation step: 4→3→2→1→0→-1 |
| `tim4_val` | `TIM4.VAL` | 1 byte | Per-frame refuel timer |
| `tim9_val` | `TIM9.VAL` | 1 byte | Slave info display countdown |
| `temp5`, `temp6` | `TEMP5`, `TEMP6` | 1 byte each | Loop counters in NEXT.PART1 |

---

## 4. Assembly Function Inventory

| Assembly label | C name | Visibility | Role |
|---------------|--------|-----------|------|
| `MOVE.SLAVES` | `move_slaves()` | public | Process one slave per call; advance slave_num |
| `S.COL` | `s_col()` | static | Check if slave occupies dangerous tile |
| `S.COL2` / `.1` | `s_col2()` | static | Kill slave: erase + OFF + dec count + print |
| `S.ERASE` | `s_erase()` | static | Write TILE_EMPTY/$48 + TILE_ERASE_T/$1F to map |
| `S.MOVE` | `s_move()` | static | Move slave one step, retry on wall collision |
| `S.DRAW` | `s_draw()` | static | Write slave sprite chars to map |
| `PRINT.SLAVES.LEFT` | `print_slaves_left()` | public | Print "MEN TO RESCUE" + update PLAY.SCRN[5/6] |
| `PICK.UP.SLAVE` | `pick_up_slave()` | public | Check chopper proximity to all 8 slaves |
| `CHECK.FUEL.BASE` | `check_fuel_base()` | public | Detect landing at base; drive refuel animation |
| `RE.FUEL` | `re_fuel()` | static | Per-frame timer handler for refuel sequence |
| `DF1` | `df1()` | static | Draw base shape at current fuel_temp step |
| `FE` | `fe()` | static | Empty: just return (assembly label = just RTS) |
| `F1` | `f1()` | static | Final descent phase of refuel |
| `DRAW.BASE` | `draw_base()` | static | Write 5 rows × 6 bytes of BASE.SHAPE to map |
| `MULT.BY.40` | `mult_by_40()` | static | Multiply val×40 → temp1 (lo), temp2 (hi) |
| `SET.SCANNER` | `set_scanner()` | public | Compute scanner base address; call DO.LINE ×3 |
| `DO.LINE` | `do_line()` | static | Copy 8 map rows × 12 bytes to scanner char data |
| `POS.IT` | `pos_it()` | public | Toggle scanner minimap pixel for (temp1,temp2) |
| `CHECK.FORT` | `check_fort()` | public | If fort_status==EXPLODE: run checksum then explosion |
| `DO.CHECKSUM1` | `do_checksum1()` | static | ROM checksum; on pass: call next_part1() |
| `DO.CHECKSUM2` | `do_checksum2()` | public | ROM checksum; on fail: for(;;){} |
| `DO.CHECKSUM3` | `do_checksum3()` | public | Page $B980 checksum; on fail: for(;;){} |
| `NEXT.PART1` | `next_part1()` | static | Fort explosion sequence + mode reset |
| `LINE1` | `line1()` | public | DLI handler: set VDSLST→line2, gradient |
| `LINE2` | `line2()` | public | DLI handler: set VDSLST→line3, missile HPos |
| `LINE3` | `line3()` | public | DLI handler: set VDSLST→line4, CHBASE, colors |
| `LINE4` | `line4()` | public | DLI handler: set VDSLST→line1, call do_sounds |
| `DO.SOUNDS` | `do_sounds()` | public | All 6 sound channels: S1–S6 in fall-through chain |

---

## 5. Static Data Tables in fort5.s

### Slave animation characters

Four tables of 2 bytes each (2 animation frames):

| Table | Frame 0 | Frame 1 | Description |
|-------|---------|---------|-------------|
| `SLAVE.CHR.TL` | 0x4A | 0x4A | Top-left (identical both frames) |
| `SLAVE.CHR.TR` | 0x49 | 0x49 | Top-right (identical both frames) |
| `SLAVE.CHR.B.L` | 0x3E | 0x3D | Bottom-left (two distinct frames) |
| `SLAVE.CHR.B.R` | 0x3B | 0x3C | Bottom-right (two distinct frames) |

The top tiles do not animate (same char both frames). The bottom tiles cycle between two chars to create a walking effect.

**Assembly data overlap note:** `LAND.CHR` (5 bytes: `0x3E,0x3D,0x3B,0x3C,0x44`) shares the same assembly address as `SLAVE.CHR.B.L`. The land_chr table begins at the bottom-left slave char and extends two more bytes. In C these are separate named arrays — the overlap is an assembly space-saving trick.

### SLAVE.PICKUP.MESS
Screen-code string "MEN  TO  RESCUE" (each byte = Atari screen code with bit 7 set for inverse video), terminated by 0xFF. 16 bytes total.

### WARNING
Screen-code string "LOW  ON  FUEL", terminated by 0xFF. 14 bytes total.

### BASE.SHAPE
54 bytes = 9 rows × 6 columns of character codes. Represents the fuel base graphic. Rows 0–3 are all zeros (erasing); rows 4–8 form the visible base structure:
- Row 4: `0x44` × 6 (roof beam)
- Row 5/7: `0x55 0x58 0x58 0x58 0x58 0x56` (side walls)
- Row 6: `0x55 0x26 0x35 0x25 0x2C 0x56` (interior with fuel indicator chars)
- Row 8: `0x54 0 0 0 0 0x54` (floor pillars)

`DRAW.BASE` indexes into this with `temp3 * 6` and writes rows `[temp3]..[temp3+4]`.

### FORT.EXP
Originally a 4-pointer table in assembly (4 × 2 bytes = 8 bytes total):
```
.DA FORT.EX1, FORT.EX2, FORT.EX3, FORT.EX4
```
`FORT.EX1`–`FORT.EX4` are defined in `fnt1.s` at chars $0C–$0F. In `fnt1.h` they are exported as `extern const uint8_t *const`. Because a static array initializer in C11 cannot take the address of an `extern const` pointer, this table is initialized at runtime inside `next_part1()` as a local array:
```c
const uint8_t *fort_exp[4];
fort_exp[0] = FORT_EX1; fort_exp[1] = FORT_EX2;
fort_exp[2] = FORT_EX3; fort_exp[3] = FORT_EX4;
```

---

## 6. Key Algorithm Analysis

### 6.1 SLAVE.DX State Machine

`slave_dx[i]` is an 8-bit value encoding both direction and animation frame:

| Bits | Value when right | Value when left | Meaning |
|------|-----------------|----------------|---------|
| Bit 7 | 0 | 1 | Direction: 0=right, 1=left |
| Bit 6-4 | 001 ($10) | 111 ($F0) | Direction marker |
| Bit 0 | 0 or 1 | 0 or 1 | Animation frame |

**State transitions in S.MOVE:**
- Moving right: `dx = ((dx+1) & 0x01) | 0x10` — increments, masks to bit 0, resets direction marker to $10
- Moving left:  `dx = ((dx-1) & 0x01) | 0xF0` — decrements, masks to bit 0, resets direction marker to $F0

**Wall bounce:** `dx ^= 0xE0` flips bit 7 (direction) and bits 6-5, effectively changing $10 ↔ $F0 without touching the animation frame bit.

**Draw selection:**
- `dx & 0x80` = 1 → left-facing: use `slave_chr_bl[frame]` / `slave_chr_tl[frame]`
- `dx & 0x80` = 0 → right-facing: use `slave_chr_br[frame]` / `slave_chr_tr[frame]`
- `frame = dx & 0x03` (only bit 0 is ever set, so frame is 0 or 1)

### 6.2 PICK.UP.SLAVE Distance Check with EOR #$FE

The assembly computes `dx = slave_x[i] - chop_x` and then:
```
BPL .skip_eor   ; if result ≥ 0 (positive), skip
EOR #$FE        ; if negative (dx in range $FF..$80), apply XOR
```

`EOR #$FE` on a negative byte: for dx = $FF (-1), $FF ^ $FE = $01. For dx = $FE (-2), $FE ^ $FE = $00. For dx = $FD (-3), $FD ^ $FE = $03. This effectively converts -1→1, -2→0, -3→3. This is NOT the same as absolute value (`NEG`): -2 gives 0, -3 gives 3. It is the specific behavior of the 6502 instruction sequence and must be replicated exactly.

```c
uint8_t dx = (uint8_t)(slave_x[si] - chop_x);
if (dx & 0x80u) dx ^= 0xFEu;
if (dx >= 4u) continue;
```

This preserves the exact capture radius behavior: a slave within 4 pixels horizontal distance (with this specific EOR metric) triggers pickup.

### 6.3 S.COL / S.COL2 Fall-Through

In the assembly, `S.COL` checks the bottom tile at the slave's current position (via ADR1), then decrements ADR1+1 (the high byte of the map address, stepping one row up) and checks the top tile. If either tile is dangerous (0, $20 EXP, $71 MISS.LEFT, $72 MISS.RIGHT), execution falls through at label `.1` which IS the entry point of `S.COL2`.

S.COL2 is also called directly from MOVE.SLAVES when `slave_status == STATUS_PICKUP`. In both cases it does the same work: erase slave, set status to OFF, decrement slaves_left, print the updated count.

The `SEC; RTS` at the end of S.COL2 sets carry = 1 (collision detected). In C this becomes `return 1`. S.COL returns 0 (CLC before RTS) on safe tiles. The caller in MOVE.SLAVES branches on carry: `BCS .3` (skip move/draw).

### 6.4 DO.LINE Scanner Copy Algorithm

`DO.LINE` copies map data into character set graphics for the scanner minimap display. The scanner uses character cells stored in `CHR.SET1` (at `S.LINE1`, `S.LINE2`, `S.LINE3`). Each character cell is 8×8 pixels = 8 bytes.

**Key insight about the stride:** The destination advances by 8 bytes per map byte copied. This is because in the character set memory layout, the 8 scanlines of each character are at consecutive byte positions within the cell. Writing one byte per character cell (at stride 8) fills one horizontal scanline across all 12 character cells in a row.

**Per-call behavior:**
- Outer loop: 8 iterations (rows 7 down to 0)
- Inner loop: 12 iterations per outer loop
  - Read 1 byte from `scan_adr1` (map tile) → `src++`
  - Write to `scan_adr2` → `dst += 8`
- After inner loop: `scan_adr1 += 40` (advance to next map row); `scan_adr2 += 1` (advance to next scanline in character cells)

After all 8 outer loops, one full band of 12 scanner characters has been updated.

`SET.SCANNER` calls `DO.LINE` three times (for S.LINE1, S.LINE2, S.LINE3) after resetting `scan_adr2` but NOT `scan_adr1` — the map read pointer advances continuously across all three calls.

### 6.5 POS.IT — TEMP1 Overwrite Subtlety

`POS.IT` uses `temp1=tile_x` and `temp2=tile_y` as input globals. However, `MULT.BY.40` (the multiplication helper) stores its input value into `TEMP1` before computing the result. This means calling `mult_by_40(temp2)` overwrites `temp1`.

The assembly handles this with the X register:
```
LDX TEMP1       ; save original tile_x in X
LDA TEMP2       ; load tile_y for multiplication
JSR MULT.BY.40  ; temp1 = (tile_y*40)&0xFF, temp2 = (tile_y*40)>>8
TXA             ; recover tile_x from X
LSR; LSR; LSR   ; tile_x >> 3 = byte offset within scanner row
CLC; ADC #SCANNER+3; ADC TEMP1  ; add row offset (lo byte) + $C3
```

In C:
```c
uint8_t tx = temp1;     // save tile_x before mult_by_40 clobbers temp1
mult_by_40(temp2);      // temp1 = row_offset_lo, temp2 = row_offset_hi
uint32_t addr = 0x39C0u + 3u + (uint32_t)(tx >> 3u)
              + (uint32_t)temp1 + ((uint32_t)temp2 << 8u);
```

### 6.6 CHECK.FUEL.BASE Landing Window

The fuel base landing zone depends on level:

| Level | X range | Y range |
|-------|---------|---------|
| 0 | 23..243 (any non-extreme X) | 9..12 |
| 1 | 0x82..0x87 (130..135) | 9..12 |

The Y range `9 ≤ chop_y < 13` aligns with the visual base platform location. The X range for level 0 is very wide (nearly the whole map) because the level-0 base spans the full width; level 1 has a single narrow landing pad.

**Fall-through to warning section:** After the landing test, the function always reaches the warning check. This is intentional: when the chopper JUST triggered REFUEL in the same frame, the warning check immediately sees STATUS_REFUEL and calls clear_info() (removing any warning message). This is the correct "you landed" feedback.

### 6.7 RE.FUEL Animation Sequence

`fuel_temp` counts 4→3→2→1→0 (normal steps) then wraps to $FF (= -1 as int8_t).

`RE.FUEL` fires on `tim4_val` countdown (every 1 frame). Each timer expiry:
- If `(int8_t)fuel_temp < 0` (i.e., fuel_temp = $FF): call `f1()` (landing complete logic)
- Else: call `df1()` (draw next base animation step and decrement fuel_temp)

`DF1` sets temp2=11 (map row for base), temp3=fuel_temp, then calls draw_base(). The fuel base graphic fills in row-by-row as fuel_temp decrements from 4 to 0, visually animating the base materializing.

`F1` handles the final landing descent: if `chop_y >= 10`, we wait for chopper to settle lower. When `chop_y < 10`, refuel completes: fuel_status=FULL, fuel_temp=4 (reset for next use), draw base, save_pos().

### 6.8 NEXT.PART1 Explosion Bit-Scatter

This is the most complex algorithm in fort5.s. It writes fort explosion tiles to the map in a specific pattern driven by bit-scanning the `FORT.EX*` frame data.

**Outer structure:** 4 explosion frames (temp3 = 0..3), each using a 6-byte × 3-row data block from fnt1 (chars $0C–$0F).

**Inner structure for each frame:**
- `temp4` iterates over 6 columns (0..5)
- `temp6` iterates over 3 rows within each column (0..2)
- For each (temp4, temp6) position:
  - Load byte `fd[temp4]` from the explosion frame data
  - Interpret as 8 bits (LSB-first, using logical right shift = initial carry 0)
  - Each bit controls 3 consecutive vertical chars at Y positions 23, 22, 21... (Y = 23 - bit*3)
  - Bit=1 → write TILE_EXP ($20) to all three positions
  - Bit=0 → write 0 (blank)
  - After each 3-char group, advance `adr1_hi++` (one map row down)

**Assembly carry tracking:** The assembly uses `ASL TEMP3; ROR TEMP5` to extract bits LSB-first from the loaded byte. `ASL TEMP3` with TEMP3=0..3 always produces carry=0, making `ROR TEMP5` equivalent to a logical right shift. This means the C implementation uses simple `>> 1` to extract bits.

**16-frame flash loop per explosion frame:**
```
bak2_color = $10; AUDC4 = $CF
for 16 times:
    wait_frame(2)
    bak2_color++
    s3_val = 1
    AUDF4 = RANDOM
bak2_color = 0
```
This creates a cycling background color and explosion sound burst for each of the 4 frames.

### 6.9 ROM Checksum (DO.CHECKSUM1/2)

Both DO.CHECKSUM1 and DO.CHECKSUM2 perform an identical ROM integrity check over pages $90–$AF ($9000–$AFFF = 8KB):

```
sum = 0; ovf = 0
for page = $90 to $AF:
    for byte = 0 to 255:
        sum += ROM[page:byte]
        if carry: ovf++
if sum != 0 OR ovf != 0: trap (for(;;){})
```

The check passes only if the sum of all 8192 ROM bytes plus carry-tracking is exactly zero (meaning the ROM was built with a checksum byte that makes the total sum to zero).

`.HS 12` is the SynAssembler notation for the raw byte `0x12`, which on the 6502 is an illegal opcode that causes the CPU to hang. In C this becomes `for(;;){}`.

**DO.CHECKSUM1 vs DO.CHECKSUM2:** DO.CHECKSUM1 is static (called only from CHECK.FORT) and falls through directly to NEXT.PART1 on success. DO.CHECKSUM2 and DO.CHECKSUM3 are public (called from other modules). DO.CHECKSUM3 checks only page $B9 ($B980, 256 bytes) with a simple sum (no carry tracking).

A shared static helper `rom_checksum_ok()` is used to avoid code duplication between the public and private checksum functions.

### 6.10 DLI Handler Chain (LINE1–LINE4)

The four DLI handlers form a circular chain, each setting VDSLST (the deferred VBI vector) to point to the next handler. This creates a sequence of display effects timed to specific scan lines.

**LINE1 → LINE2:** 8-line ascending color gradient. COLBK = (x<<1)|$E0 for x=0..7 (colors $E0, $E2, $E4, $E6, $E8, $EA, $EC, $EE — ascending brightness). Each WSYNC_HW write stalls until the next scan line.

**LINE2 → LINE3:** Set rocket/missile horizontal positions from `rocket_x[2..0]` into HPOSM0-2 ($D004–$D006). Then 8-line descending color gradient (same colors, reversed order).

**LINE3 → LINE4:** Set HPOSP2/P3 for the robot sprite. Write WSYNC, then set CHBASE=$0C (switches character set to CHR.SET2 at $0C00). Set COLPF0=bak_color, COLPF1=$0A, COLPF2=$93, COLPF3=FRAME (flickers color). Second WSYNC, then COLBK=bak2_color (the explosion/flash color).

**LINE4 → LINE1:** Clear all 8 player/missile HPos registers ($D000–$D007). WSYNC, COLBK=0. Call `do_sounds()` if mode is STOP_MODE or GO_MODE.

### 6.11 DO.SOUNDS Channel Analysis

Six channels in fall-through order (no branches between channels, all execute each call):

**S1 — Chopper engine:**
- Skip if `chopper_status == STATUS_OFF` or `FRAME & 2` (fires every 4th frame)
- AUDC1 = $83 (noise polynomial + volume)
- `freq = (s1_1_val & 0x80) ? s1_2_val : s1_1_val` — s1_1_val starts high, falls; when it goes negative (bit 7 set), switch to s1_2_val (set by joystick to indicate direction/speed)
- `freq -= 4; s1_1_val = freq; AUDF1 = freq`

**S2 — Missile:**
- Skip if `s2_val & 0x80` (bit 7 set = missile inactive, value = $FF after one shot completes)
- `freq = (s2_val ^ $3F) + 16` — maps s2_val $3F→$10, down to s2_val 0→$4F
- AUDF2 = freq; AUDC2 = (freq == $4F) ? 0 : $86 — silence when s2_val==0 (fully expended)
- `s2_val--` — on next frame, s2_val=$FF triggers BMI skip (missile silent until next fire)

**S3 — Explosion:**
- Skip if `s3_val == 0`
- `freq = (RANDOM & 3) | s3_val; AUDF3 = freq + $10`
- `s3_val++; if s3_val == $31: s3_val = 0`
- `AUDC3 = s3_val ? $48 : 0` — note: uses NEW s3_val value (0 on last iteration = silence)

**S4 — Refuel:**
- Skip if `s4_val == 0`
- `xv = (FRAME & 7) ? $18 : 0` — alternating frequency for "filling" sound
- If `fuel2 < $20` (below max BCD 20): `yv = $A6`, BCD add 4 to fuel1/fuel2
- AUDF2 = xv; AUDC2 = yv

The BCD add uses SED/ADC/CLD in assembly. Since C has no decimal mode, it must be implemented with nibble arithmetic:
```c
uint8_t lo = (fuel1 & 0x0F) + 4;
uint8_t c = 0;
if (lo >= 10) { lo -= 10; c = 1; }
uint8_t hi = (fuel1 >> 4) + c;
c = 0;
if (hi >= 10) { hi -= 10; c = 1; }
fuel1 = (hi << 4) | lo;
// repeat for fuel2 with carry in
```

**S5 — Hyper-chamber:**
- Skip if `s5_val == 0`
- `old = s5_val; s5_val++`
- `if old == $50: s5_val = 0; freq = 0; else freq = old`
- AUDF2 = freq; AUDC2 = $A8

**S6 — Cruise missile:**
- Skip if `FRAME & 1` or `s6_val == 0` (every other frame)
- `old = s6_val; s6_val++`
- `if old >= $20: s6_val = 0`
- AUDF4 = old; AUDC4 = $07

Note: S4, S5 all write to AUDF2/AUDC2. Only the one that fires last in a given frame takes effect. This creates a priority order: S5 (hyper) overrides S4 (refuel), since S5 runs after S4 in the chain. S6 writes AUDF4/AUDC4 independently.
