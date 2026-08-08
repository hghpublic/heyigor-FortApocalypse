# Session Log — fort4.s → C Conversion
**Date:** 2026-08-08  
**Branch:** `agents/asm-to-c-conversion`  
**Working directory:** `heyigor-FortApocalypse.worktrees/asm-to-c-conversion`  
**Produced files:** `dev/src/fort4.h`, `dev/src/fort4.c`  
**Final compile test:** `cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c` — **clean, zero diagnostics**

---

## 1. Session Context

This session was a direct continuation of a prior conversation that was context-compacted. The prior session had already converted:
- `fort1.s` → `dev/src/fort1.h` + `dev/src/fort1.c`
- `fort2.s` → `dev/src/fort2.h` + `dev/src/fort2.c`
- `fort3.s` → `dev/src/fort3.h` + `dev/src/fort3.c`

The task for this session was: **convert `fort4.s` to C**.

`fort4.s` is titled "MAIN INTERRUPT DRIVER PART II" (765 lines). It is called from the VBlank interrupt handler in `fort3.s`/`fort3.c`. All of its public functions were already referenced as `extern` in `fort3.c`, waiting for `fort4.c` to be created.

The prior compacted session had already done all the assembly research (reading `fort4.s`, `fort.s`, `fort7.s`, `fort8.s`, `fort6.s`, `fnt1.s`) and was at the point of writing. This session re-verified context and wrote the files.

---

## 2. Source File Inventory

### 2.1 `fort4.s` (765 lines) — the input

Full function list in assembly order:

| Assembly label        | C name              | Role |
|-----------------------|---------------------|------|
| `POS.CHOPPER`         | `pos_chopper()`     | set temp1_i/temp2_i from chop_x/chop_y, call POS.IT.I |
| `POS.ROBOT`           | `pos_robot()`       | set temp1_i/temp2_i from r_x/r_y, fall through to POS.IT.I |
| `POS.IT.I`            | `pos_it_i()` static | XOR scanner minimap pixel for (temp1_i, temp2_i) |
| `READ.STICK`          | `read_stick()`      | guard (OFF/CRASH → return), call DO.STICK |
| `DO.STICK`            | `do_stick()` static | full joystick handler (demo playback + real input) |
| `READ.TRIG`           | `read_trig()`       | fire button with rising-edge detection |
| `HOVER`               | `hover()`           | every 8 frames nudge angle toward center |
| `DRAW.MAP` / `DO.X`   | `draw_map()`        | horizontal scroll logic |
| `DRAW.MAP` / `DO.Y`   | `draw_map()` cont.  | vertical scroll logic + HSCROL/VSCROL write + DSP.MAP fill |
| `COMPUTE.MAP.ADR.I`   | `compute_map_adr_i()` | (temp1_i,temp2_i) → adr1_i using MAP−5 base |
| `COMPUTE.MAP.ADR`     | `compute_map_adr()` | (temp1,temp2) → adr1 using MAP−5 base |
| `DO.LASER.1`          | `do_laser_1()`      | every 8 frames: copy/clear LASERS.1 + LASER.3 |
| `DO.LASER.2`          | `do_laser_2()`      | every 8 frames: copy/clear LASERS.2 + LASER.3[16..23] |
| `DO.BLOCKS`           | `do_blocks()`       | every 128 frames: clear BLOCK.1-4, random refill |
| `DO.ELEVATOR`         | `do_elevator()`     | timer-driven: clear BLOCK.5-8, advance elevator slot |
| `ELEVATORS`           | `elevators[]` static | 4-entry pointer table to BLOCK.5-8 |
| `DO.EXP`              | `do_exp()`          | randomise EXPLOSION/EXPLOSION2 shapes, flash missile colours |
| `DO.NUMBERS`          | `do_numbers()`      | guard (NEW_PLAYER/GAME_OVER → return), call DO.N |
| `DO.N`                | `do_n()` static     | draw score/bonus/fuel; decrement bonus and fuel on schedule |
| `DDIG`                | `ddig(uint8_t val)` | BCD byte → 2 digit calls via DRAW |
| `DRAW`                | `draw(uint8_t digit)` static | one nibble → 2 display bytes at S.ADR, advance, DEX |
| `INC.SCORE`           | `inc_score(uint8_t hi, uint8_t lo)` | BCD add hi:lo to SCORE1/2/3 |
| `DEMO.STICK`          | `demo_stick[]` static | 108-byte demo joystick replay table |

---

## 3. Research Phase

### 3.1 Files Read This Session

Before writing code, the following files were examined (in addition to what was already in the compacted summary):

**`dev/src/fort1.h`** — verified contents: only MODE constants, STATUS constants, and function declarations. Does NOT contain hardware register macros or memory address macros (those live in `fort1.c`).

**`dev/src/fort1.c`** (key excerpts) — verified:
- `#define REG(a) (*(volatile uint8_t *)(uintptr_t)(a))`
- Memory addresses: `CHR_SET2 = 0x0C00`, `MAP_BASE = 0x1103`, `SCANNER_BASE = 0x39C0`, `RAM1_STUFF = 0x0C90` (= CHR_SET2+144), `RAM2_STUFF = 0x0100`
- All extern variable declarations: `adr1_lo/hi`, `temp1/2/3/4`, `temp1_i`, `s_adr_lo/hi`, `s_flg`, `mode`, `score1/2/3`, `bonus1/2`, `fuel1/2`, `fuel_status`, `laser_status`, `laser_spd`, `chopper_x/y/angle/status`, `chop_x/chop_y`, `sx/sy/sx_f/sy_f`, `demo_status`, `demo_count`, `elevator_spd/dx/tim`, `tim1_val/tim2_val`, `s2_val`
- `extern void ddig(uint8_t val);` ← fort4.c must define this
- `extern void hover(void);` ← fort4.c must define this
- `extern void compute_map_adr(void);` ← fort4.c must define this
- `extern void compute_map_adr_i(void);` ← fort4.c must define this
- Usage of `ddig`: in `m_new_player`, sets `demo_count = 0; ddig(chop_left);` — `demo_count` used as a proxy for the digit-position counter X. In `m_game_over`, calls `ddig(hi3); ddig(hi2); ddig(hi1);` without resetting.

**`dev/src/fort3.c`** (key excerpts) — verified:
- Also declares `adr1_i_lo/hi`, `temp2_i`, `r_x/r_y`, `robot_status`, `rocket_status[3]/rocket_x[3]/rocket_y[3]`, `s2_val/s3_val/s5_val`
- Has extern declarations for all fort4 public functions: `pos_chopper`, `pos_robot`, `compute_map_adr_i`, `inc_score`, `do_numbers`, `draw_map`, `read_trig`, `do_exp`, `do_laser_1`, `do_laser_2`, `do_blocks`, `do_elevator`, `read_stick`
- Defines `#define CHR_SET2 ((const uint8_t *)0x0C00u)` — read-only pointer; fort4.c needs writable so must define its own `CHR_SET2`

**`dev/src/fnt1.h`** — provides:
- `extern const uint8_t *const POS_MASK1;` — scanner XOR mask table (8 bytes at fnt1 glyph 0x0B)
- `extern const uint8_t *const EXP_SHAPE;` — explosion shape (8 bytes at fnt1 glyph 0x3C)

**`fort4.s`** — read completely (reproduced in section 5 below with annotations)

**`fort7.s`** (grep) — verified:
- `TRIG.FLAG .BS 1` (offset 0x0250 = 592)
- `ELEVATOR.NUM .BS 1` (offset 0x0700 = 1792)

**`fort.s`** (grep) — verified:
- `S.TEMP .BS 1` (offset 0x0203)
- `S1.2.VAL .BS 1` (offset 0x0221)
- `STICK = $278` (OS joystick shadow)

**`fort8.s`** — read completely to compute DSP.MAP, SCORE.DIG, FUEL.DIG, BONUS.DIG offsets

---

## 4. Critical Offset Calculations

### 4.1 DSP.MAP offset within RAM1.STUFF

`DSP.MAP .EQ *-Z1+RAM1.STUFF` where Z1 starts at RAM1.STUFF.

Bytes before the DSP.MAP label in the Z1 block:
```
.HS 70708070           = 4 bytes  (offsets 0-3)
.DA #$44,PANEL         = 3 bytes  (offsets 4-6)
.HS 04040404           = 4 bytes  (offsets 7-10)
.DA #$44,NAVA.PANEL    = 3 bytes  (offsets 11-13)
.DA #$44+$80,PLAY.SCRN = 3 bytes  (offsets 14-16)
.HS 502080             = 3 bytes  (offsets 17-19)
DSP.MAP                = offset 20
```

**Result:** `DSP.MAP = RAM1_STUFF + 20 = 0x0C90 + 20 = 0x0CA4`

In C: `#define DSP_MAP_PTR (RAM1_STUFF + 20u)`

### 4.2 SCORE.DIG, FUEL.DIG, BONUS.DIG offsets within RAM2.STUFF

`Z2` / `PANEL` starts at `RAM2.STUFF = 0x0100`.

Counting bytes from the `Z2` label:

```
.AT /       /           = 7 bytes  (offset 0)
.HS B3D3A3C3AFCFB2D2A5C5 = 10 bytes (offset 7)
.AT /  /                = 2 bytes  (offset 17)
SCORE.DIG               → offset 19

.AT /            /      = 12 bytes (offset 19)
.AT /         /         = 9 bytes  (offset 31)
.AT /                    / = 20 bytes (offset 40)
.AT /                    / = 20 bytes (offset 60)
.AT /  /                = 2 bytes  (offset 80)
.HS A6C6B5D5A5C5ACCC    = 8 bytes  (offset 82)
.AT /  :/               = 3 bytes  (offset 90)
.HS 5C5D5E5F6061626364656667 = 12 bytes (offset 93)
.AT /=  /               = 3 bytes  (offset 105)
.HS A2C2AFCFAECEB5D5B3D3 = 10 bytes (offset 108)
.AT /    /              = 4 bytes  (offset 118)
FUEL.DIG                → offset 122

.AT /        /          = 8 bytes  (offset 122)
.AT /  ;/               = 3 bytes  (offset 130)
.HS 68696A6B6C6D6E6F70717273 = 12 bytes (offset 133)
.AT />   /              = 4 bytes  (offset 145)
BONUS.DIG               → offset 149
```

**Results:**
- `SCORE_DIG = RAM2_STUFF + 19  = 0x0113`
- `FUEL_DIG  = RAM2_STUFF + 122 = 0x017A`
- `BONUS_DIG = RAM2_STUFF + 149 = 0x0195`

### 4.3 MAP−5 base for compute_map_adr_i

`MAP = 0x1103`, so `MAP−5 = 0x10FE`.
- Low byte: `0xFE`
- High byte: `0x10`

The assembly adds temp1_i to the low byte (propagating carry), then adds temp2_i to the high byte:
```c
uint16_t s = (uint16_t)(0xFEu + temp1_i);
adr1_i_lo = (uint8_t)(s & 0xFFu);
adr1_i_hi = (uint8_t)((uint8_t)(0x10u + (uint8_t)(s >> 8)) + temp2_i);
```

### 4.4 CHR_SET2 sub-area addresses

| Name | Offset | Absolute |
|------|--------|----------|
| `LASERS.1` | +8 | 0x0C08 |
| `LASERS.2` | +40 | 0x0C28 |
| `LASER.3` | +72 | 0x0C48 |
| `BLOCK.1` | +80 | 0x0C50 |
| `BLOCK.2` | +88 | 0x0C58 |
| `BLOCK.3` | +96 | 0x0C60 |
| `BLOCK.4` | +104 | 0x0C68 |
| `BLOCK.5` | +112 | 0x0C70 |
| `BLOCK.6` | +120 | 0x0C78 |
| `BLOCK.7` | +128 | 0x0C80 |
| `BLOCK.8` | +136 | 0x0C88 |
| `EXPLOSION` | +256 | 0x0D00 |
| `EXPLOSION2` | +504 | 0x0DF8 |
| `MISS.CHR.LEFT` | +904 | 0x0F88 |
| `MISS.CHR.RIGHT` | +912 | 0x0F90 |

---

## 5. Per-Function Analysis and Translation Decisions

### 5.1 POS.CHOPPER / POS.ROBOT / POS.IT.I

**Assembly:**
```
POS.CHOPPER: LDA CHOP.X; STA TEMP1.I; LDA CHOP.Y; STA TEMP2.I; JMP POS.IT.I
POS.ROBOT:   LDA R.X;    STA TEMP1.I; LDA R.Y;    STA TEMP2.I
             ;  JMP POS.IT.I  ← COMMENTED OUT → falls through to POS.IT.I
POS.IT.I:
    X = temp1_i  (tile X)
    A = temp2_i  (tile Y)
    ; compute y*40 using shift-add trick:
    A = (A<<2 + A) = A*5, then shift left 3 more with TEMP3.I holding overflow
    ; result: TEMP3.I:TEMP2.I = y*40  (16-bit)
    ; compute scanner address:
    adr1_i = SCANNER + 3 + (x>>3) + y*40
    ; XOR pixel:
    *adr1_i ^= POS_MASK1[x & 7]
```

**Key insight:** The shift-add for y*40:
- `A = y*2; A = y*4; A += y → y*5`
- `TEMP3.I=0; ASL×3 with ROL TEMP3.I each time → y*40` in 16-bit

**C translation:**
```c
static void pos_it_i(void) {
    uint8_t x = temp1_i, y = temp2_i;
    uint16_t offset = 3u + (uint16_t)(x >> 3) + (uint16_t)y * 40u;
    SCANNER_BASE[offset] ^= POS_MASK1[x & 7u];
}
```

**Note on POS_MASK1:** `{0x80, 0x80, 0x20, 0x20, 0x08, 0x08, 0x02, 0x02}` — pairs of identical bits. This is because the scanner uses 2bpp where 2 horizontal pixels per tile column.

**Note on pos_robot fall-through:** In the assembly, `; JMP POS.IT.I` is commented out, making pos_robot literally fall through to the POS.IT.I code. In C this is handled by an explicit `pos_it_i()` call at the end of `pos_robot()`.

---

### 5.2 READ.STICK / DO.STICK

**READ.STICK guard logic:**
```
if (chopper_status == OFF || chopper_status == CRASH) return;
// else fall through to DO.STICK
```

**DO.STICK full flow:**

1. **Save odd-bit of angle:** `saved = chopper_angle & 1; chopper_angle &= 0xFE`
   - The LSB of the angle encodes left/right facing direction separately from the animation frame index. It's preserved across the joystick handler.

2. **Demo mode override:** When `demo_status == 0` (demo running):
   - Read `demo_stick[demo_count]` into the STICK hardware register
   - Every 16 frames (`FRAME & 0xF == 0`): advance `demo_count`, wrap at 108

3. **Read stick:** `stick = STICK_HW` (after possible demo override)

4. **Neutral (no direction):** `if (stick == 0x0F) { hover(); s1_2_val = 20; }`

5. **Fuel-empty override:** `if (fuel_status == EMPTY) s1_2_val = 60;`

6. **RIGHT pressed** (`stick & JOY_RIGHT == 0`):
   - `s1_2_val = 17`
   - Move X: `if (chopper_angle >= 14 || (frame & 1 == 0)) chopper_x++`
     - At high angle (≥14, pointing right-ish): always move
     - At low angle: only every other frame
   - Angle: `if (frame & 3 == 0) chopper_angle += 2` (every 4 frames, lean right)

7. **LEFT pressed** (`stick & JOY_LEFT == 0`):
   - `s1_2_val = 17`
   - Move X: `if (chopper_angle < 4 || (frame & 1 == 0)) chopper_x--`
   - Angle: `if (frame & 3 == 0) chopper_angle -= 2`

8. **UP pressed** (only when fuel not empty):
   - `s1_2_val = 13; chopper_y--; hover()`

9. **DOWN pressed** (only when not LAND/PICKUP status):
   - `s1_2_val = 26; chopper_y++; hover()`

10. **Angle clamp:**
    - `if (chopper_angle & 0x80) chopper_angle = 0` — underflow (went negative)
    - `if (chopper_angle >= 18) chopper_angle = 16`
    - `chopper_angle |= saved_bit0` — restore odd bit

**Frame reading:** FRAME is read once into a local variable at the start of `do_stick`. The FRAME counter is $0014 (OS tick counter, updated by VBlank). Within a single interrupt invocation it doesn't change, so one read is sufficient and correct.

---

### 5.3 HOVER

```c
void hover(void) {
    if (FRAME_HW & 7u) return;  // only every 8 frames
    uint8_t a = chopper_angle;
    if (a >= 4u && a < 14u) return;  // neutral range, no nudge
    if (a < 8u) { chopper_angle += 2u; chopper_angle += 2u; }  // nudge right
    else        { chopper_angle -= 2u; chopper_angle -= 2u; }  // nudge left
}
```

**Logic:** When the chopper is tilted too far right (angle ≥ 14) or too far left (angle < 4), it automatically nudges back toward center by ±2 degrees every 8 frames. This simulates helicopter rotor torque that naturally returns to level flight.

**Assembly detail:** The assembly does two INC/DEC instructions rather than one ADD #2. Semantically identical in C.

---

### 5.4 READ.TRIG

**Full flow:**

```
if (chopper_status == CRASH) return

if (demo_status == 0):
    if (FRAME & 0xF == 0) goto fire_section
    return

// Real game:
X = TRIG0  (hardware, 0=pressed 1=released)
if (X != 0):
    trig_flag = X  // store "released" state
    return
// X == 0 (pressed):
if (trig_flag == 0) return  // was already pressed, no rising edge
trig_flag = 0  // record "now pressed"

if (mode == TITLE or OPTION):
    mode = START_MODE
    demo_status = START_MODE  // (LDA #START.MODE already in A; ; LDA #1 commented)

fire_section:
    elevator_dx ^= 0xFE  // flip direction of elevator
    find free rocket slot (check index 1 then 0; slot free when rocket_status[i]==0)
    if none: return
    compute rocket direction from chopper_angle
    set rocket_status[slot], rocket_x[slot], rocket_y[slot]
    s2_val = 0x3F
```

**TRIG.FLAG edge detection:**
- `trig_flag = 1` = trigger up (default/released)
- `trig_flag = 0` = trigger currently held
- Rising edge: trig_flag goes 1→0 on first press

**`demo_status = START_MODE` detail:** In the assembly, `LDA #START.MODE; STA MODE; ; LDA #1 (commented); STA DEMO.STATUS` — the `LDA #1` is commented out, so `STA DEMO.STATUS` uses A which still holds `START.MODE = 3`. So demo_status becomes 3, not 1. Both are non-zero (real game) so the behavior is the same, but the exact byte value is 3.

**Rocket direction computation:**

`a = (chopper_angle & 0x1E) >> 1` (extract bits 4:1, then shift, giving 0-8)

| a | rocket status |
|---|---------------|
| 0 | 1 (up) |
| 1 | 1 |
| 2 | 2 |
| 3 | 3 |
| 4 | 3 (special: LDA #3) |
| 5 | 3 |
| 6 | 4 (via .7: a-2=4) |
| 7 | 5 |
| 8 | 5 (would be 6, clamped to 5) |

Assembly logic:
```
if a < 4: use a (but 0 → 1 via final check)
if 4 ≤ a < 6: force status = 3
if a ≥ 6: status = a - 2 (clamped to max 5)
if status == 0: force to 1
```

**Rocket X position:** `rocket_x[slot] = (chopper_x & 3) + chopper_x + 8`  
This is the center pixel of the chopper plus 8 pixels, with a small bias from the low 2 bits of X position. Not a clean multiply — just what the assembly does.

---

### 5.5 DRAW.MAP

This is the most complex function. It contains two interlocked sections (DO.X and DO.Y) followed by register writes and the display list fill.

#### DO.X (horizontal scroll)

The Atari's HSCROL hardware register takes values 0-3 (the bottom 2 bits of `sx_f`). When these bits overflow from 0 to 3 (right scroll) or from 3 to 0 (left scroll), a new tile column is needed.

**Right scroll trigger:** `chopper_x > MIN_RIGHT (130)`
- Clamp chopper_x to 130
- Wrap check: if `sx >= 0xD9 (= 0xD8+1 = 217)`, reset `sx = 2`
- `sx_f--` (scroll right = sub-pixel decrements)
- Tile boundary: if `(sx_f & 3) == 3` after decrement (was 0, wrapped to 255 masked to 3), increment `sx`

**Left scroll trigger:** `chopper_x < MIN_LEFT (110)`
- Clamp chopper_x to 110
- Wrap check: if `sx < 3`, reset `sx = 0xD9`
- `sx_f++`
- Tile boundary: if `(sx_f & 3) == 0` after increment (was 3, carried to 0), decrement `sx`

**Wrap values:** 217 and 3 define the valid range of `sx`, giving approximately 214 scrollable tile columns (the map width).

#### DO.Y (vertical scroll) — detailed trace

The assembly is more complex because of the hard pixel clamps (MAX_DOWN, MAX_UP) vs. the scroll triggers (MIN_DOWN, MIN_UP) and the map edge checks (sy==24 = bottom, sy==0xFF = top).

```
if (sy != 24):                    // not at map bottom
    if (chopper_y > MIN_DOWN):    // chopper past scroll trigger
        clamp to MIN_DOWN
        FORCED → do_down_scroll
    // fall through to .80

.80:
    if (chopper_y > MAX_DOWN):    // hard pixel clamp
        clamp to MAX_DOWN
        if (sy_f & 7 != 0) OR (sy != 24): do_down_scroll
    // else: fall to .3

.3 (up-check section):
    if (sy != 0xFF):              // not at map top
        if (chopper_y < MIN_UP):  // above scroll trigger
            clamp to MIN_UP
            FORCED → do_up_scroll
    
.81:
    if (chopper_y < MAX_UP):      // hard pixel clamp (above pixel 100)
        clamp to MAX_UP
        if (sy_f & 7 != 7) OR (sy != 0xFF): do_up_scroll
```

**Key subtlety — the X register at .3:** In the assembly, the up-check at .3 uses CPX (compare with X register). The X register holds the CHOPPER.Y value loaded at the `.80: LDX CHOPPER.Y` instruction. This means:
- If we came from the FORCED path (chopper past MIN_DOWN, clamped to 166), X = 166 (from `LDX #MIN.DOWN`)
- If we came from .80 (SY==24 or chopper.Y < 213), X = original chopper.Y

In practice, when a forced down-scroll happens (chopper.Y was > 166), the resulting X=166 is always ≥ MIN_UP=146 and ≥ MAX_UP=100, so no up-scroll triggers. In the C translation, I use `y_check = chopper_y` to track this "last-known-Y" for the up-check comparisons, which gives the same result since clamped values are always in the correct range.

**Down-scroll sub-pixel:** `sy_f++; if ((sy_f & 7) == 0) sy++`
- Every 8 sub-pixels, advance tile row (because VSCROL uses 3 bits = 0-7)

**Up-scroll sub-pixel:** `sy_f--; if ((sy_f & 7) == 7) sy--`
- When sy_f bottom-3-bits wrap from 0 to 7 (= underflow), retreat tile row

#### Display list fill

After updating scroll registers:
```
HSCROL = sx_f & 3
VSCROL = sy_f & 7
temp1_i = sx; temp2_i = sy
compute_map_adr_i()  // → adr1_i_lo/hi = MAP−5 + sx + sy*256

// Fill 17 display list entries at DSP.MAP
// Each entry is 3 bytes: [cmd, addr_lo, addr_hi]
// Only addr_lo and addr_hi are updated (cmd is initialized at boot)
// Loop: X steps 1,2,3,4,5,6,...,51 (only positions 1,2 / 4,5 / ... used)
LDX #0; LDY #17
.5: INX; STA DSP.MAP,X (lo); INX; STA DSP.MAP+1,X (hi); INC ADR1.I+1; INX; DEY; BNE .5
```

In C:
```c
for (uint8_t row = 0; row < MAP_LINES; row++) {
    dsp[row*3 + 1] = adr1_i_lo;
    dsp[row*3 + 2] = adr1_i_hi;
    adr1_i_hi++;
}
```

Each map row is one 256-byte page (because the full-width tile array is exactly 256 bytes). Incrementing `adr1_i_hi` advances to the next row.

---

### 5.6 COMPUTE.MAP.ADR.I and COMPUTE.MAP.ADR

Same formula, different pointer pairs (`.I` = integer variant used by interrupt code):

```
result = MAP_BASE − 5 + temp + temp2 * 256
       = 0x10FE + temp1 + temp2 * 256

assembly:
  LDA #0xFE; CLC; ADC temp1; STA adr_lo   (may carry)
  LDA #0x10; ADC #0; STA adr_hi           (add carry)
  LDA temp2; CLC; ADC adr_hi; STA adr_hi  (add row)
```

C:
```c
uint16_t s = (uint16_t)(0xFEu + temp1_i);
adr1_i_lo = (uint8_t)(s & 0xFF);
adr1_i_hi = (uint8_t)((uint8_t)(0x10u + (uint8_t)(s >> 8)) + temp2_i);
```

The `s >> 8` gives 0 or 1 (the carry from the low-byte addition).

---

### 5.7 DO.LASER.1 / DO.LASER.2

Both run every 8 frames (`FRAME & 7 == 0`).

**DO.LASER.1:**
- If `laser_status == OFF`: clear LASERS_1 (32 bytes) and LASER_3 (8 bytes), return
- Else: `tim1_val += laser_spd; if (overflow to 0): copy laser_shapes[0..31] → LASERS_1, laser_shapes[24..31] → LASER_3`

**DO.LASER.2:**
- If `laser_status == OFF`: clear LASERS_2 (32 bytes) only (NOT LASER_3), return
- Else: `tim2_val += laser_spd; if (overflow to 0): copy laser_shapes[0..31] → LASERS_2, laser_shapes[16..23] → LASER_3`

**Key difference:** LASER_3 is cleared by DO.LASER.1 when laser is off, but NOT by DO.LASER.2. The LASER_3 character is the "center of fort" appearance and is updated by both lasers (different byte ranges of laser_shapes) when active.

**laser_shapes source:** Defined in `fort6.s` (not yet converted). Declared as `extern const uint8_t laser_shapes[32]` in fort4.c. The 32 bytes are:
```
{0xC0,0xC0,0x30,0x30,0x0C,0x0C,0x03,0x03,  // pattern 0: diagonal ↘
 0x03,0x03,0x0C,0x0C,0x30,0x30,0xC0,0xC0,  // pattern 1: diagonal ↗
 0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,  // pattern 2: horizontal bar
 0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30}  // pattern 3: vertical bar
```

---

### 5.8 DO.BLOCKS

Every 128 frames (`FRAME & 0x7F == 0`):

1. Clear BLOCK_1 through BLOCK_4 (all 32 bytes starting at CHR_SET2+80)
2. For each of the 4 blocks: if `!(RANDOM_HW & 0x80)`, fill that block's 8 bytes with `0x55`

The 32-byte clear covers all 4 blocks because BLOCK.2 starts exactly 8 bytes after BLOCK.1, etc.

Each `RANDOM_HW` read is independent (the hardware LFSR advances on each read). The four blocks are independently randomized.

---

### 5.9 DO.ELEVATOR

```
DEC ELEVATOR.TIM
if not zero: return

ELEVATOR.TIM = ELEVATOR.SPD  // reset timer

// Clear BLOCK.5 (32 bytes = BLOCK.5-8 all cleared)
BLOCK_5[0..31] = 0

// Advance slot
ELEVATOR.NUM = (ELEVATOR.NUM + ELEVATOR.DX) & 3  // mod 4

// Write pattern to new target block
target = ELEVATORS[ELEVATOR.NUM]  // pointer table
target[0..7] = 0x55
```

**ELEVATOR.DX:** Normally ±1 (advances or retreats). Flipped in `read_trig` by `EOR #0xFE` (flips bits 7:1 — since DX is signed ±1, XOR with 0xFE: 1→0xFF→-1 and -1(0xFF)→1). Actually more precisely:
- `0x01 XOR 0xFE = 0xFF = -1` (elevator reverses)
- `0xFF XOR 0xFE = 0x01 = +1` (elevator goes forward)
This flips the elevator direction every time the player fires a rocket.

**ELEVATORS table:** 4 `uint8_t*` pointers stored in the C `elevators[]` static array.

---

### 5.10 DO.EXP

```c
for i = 7 downto 0:
    v = EXP_SHAPE[i] & RANDOM_HW  // new random each iteration
    EXPLOSION[i]  = v
    EXPLOSION2[i] = v   // same value to both!

// Flash missile character colours (4 byte pairs at offsets 3..4)
for i = 3 to 4:
    MISS_CHR_LEFT[i]  = (RANDOM_HW & 0x0F) | 0xA0
for i = 3 to 4:
    MISS_CHR_RIGHT[i] = (RANDOM_HW & 0xE0) | 0x0A
```

`EXP_SHAPE = {0x3C, 0x3C, 0xFF, 0xFF, 0xFF, 0xFF, 0x3C, 0x3C}` — diamond outline mask. ANDed with fresh random bytes each frame to make the explosion shimmer.

Both `EXPLOSION` (CHR_SET2+256) and `EXPLOSION2` (CHR_SET2+504) get identical values — they are the two halves of the explosion graphic (top/bottom or left/right, depending on how sprites map to character cells).

The missile colour flash uses `0x0F` mask + `0xA0` base for left missiles and `0xE0` mask + `0x0A` base for right missiles, creating per-frame colour cycling.

---

### 5.11 DDIG and DRAW

These two routines implement BCD digit display with leading-zero suppression. DDIG processes one BCD byte (two nibbles); DRAW renders one nibble to the display buffer.

**DRAW rendering rules:**

A "digit" value arrives:
- 0x0: character '0' (value = 0, ones position always shown)
- 0x1–0x9: characters '1'–'9'
- 0xA: blank/space (only if not the last digit position = `ddig_x != 0`)

Assembly display format (`.HS` notation → actual bytes written):
- Blank:  `0x70 + 0x90 = 0x100 → 0x00` for hi byte (assembly overflow), `0x00 & 0x8F = 0x00` for lo byte
- Digit 0: `0x90` hi byte, `0x8A` lo byte (special case: CMP #0x90 → LDA #0x0A+128)
- Digit n: `0x90+n` hi byte, `(0x90+n) & 0x8F` lo byte

**Explanation of 0xF0+128:** In SynAssembler, integer arithmetic is 16-bit before truncation. `0xF0 + 128 = 0x170`. As an 8-bit immediate, the assembler takes `0x170 & 0xFF = 0x70`. Then `0x70 + (0x10+128) = 0x70 + 0x90 = 0x100 → 0x00` (with carry, but carry is irrelevant after STA).

So blank → writes `[0x00, 0x00]` to the display. This appears as a space in the Atari character mode.

**S.ADR advance:** After writing 2 bytes, `s_adr` (a 16-bit pointer in `s_adr_lo:s_adr_hi`) advances by 2.

**`ddig_x` decrement:** After each DRAW call, `ddig_x--`. When `ddig_x == 0`, the current call is the "ones" digit — blanks are replaced by '0' (never leave the ones digit empty).

**DDIG leading-zero suppression:**

Given `val` (BCD byte) and initial `s_flg`:

```
y = val

// Hi nibble
if s_flg == 0 AND hi_nibble(y) == 0:
    y |= 0xA0   // mark hi nibble as 0xA (blank)
    // s_flg stays 0
else:
    s_flg = 1   // hi nibble shown, all subsequent digits shown

// Lo nibble
if s_flg == 0 AND lo_nibble(y) == 0:
    y |= 0x0A   // mark lo nibble as 0xA (blank)
else:
    s_flg = 1

draw(y >> 4)    // hi nibble
draw(y & 0x0F)  // lo nibble (fall-through in assembly)
```

The key: `s_flg` persists across DDIG calls within a display group. Once a non-zero digit is encountered, `s_flg = 1` and all subsequent digits are rendered (no further blanking).

**Assembly fall-through:** In the assembly, the last two instructions of DDIG's lo-nibble handling are:
```
LDA S.TEMP
AND #$F     ; get lo nibble
; JSR DRAW   ← COMMENTED OUT
; RTS        ← COMMENTED OUT
DRAW:       ← falls through to DRAW
```
So DDIG never calls DRAW via JSR for the lo nibble — it falls through. In C, both DRAW calls are explicit function calls.

**`ddig_x` management in DO.N:**
```
Score:  ddig_x = 5, then ddig(score3), ddig(score2), ddig(score1)
        → 6 DRAW calls → ddig_x: 5,4,3,2,1,0 → last is ones digit
Bonus:  ddig_x = 3, then ddig(bonus2), ddig(bonus1)
        → 4 DRAW calls → ddig_x: 3,2,1,0
Fuel:   ddig_x = 3, then ddig(fuel2), ddig(fuel1)
        → 4 DRAW calls → ddig_x: 3,2,1,0
```

---

### 5.12 INC.SCORE

```
if demo_status == 0: return  (demo mode, no scoring)
BCD add: score1 += lo; score2 += hi + carry; score3 += 0 + carry
```

**Caller convention (assembly):** X register = `lo`, Y register = `hi`. In C function: `inc_score(uint8_t hi, uint8_t lo)`.

**BCD add implementation:** Software emulation since host C compilers have no SED/CLD:
```c
static uint8_t bcd_add(uint8_t a, uint8_t b, uint8_t *carry) {
    uint8_t lo = (a & 0xF) + (b & 0xF) + *carry;
    uint8_t c = (lo >= 10) ? 1 : 0;
    if (c) lo -= 10;
    uint8_t hi = (a >> 4) + (b >> 4) + c;
    c = (hi >= 10) ? 1 : 0;
    if (c) hi -= 10;
    *carry = c;
    return (hi << 4) | lo;
}
```

Note: The original assembly uses the carry from score1→score2→score3 addition without CLC between bytes. The bcd_add helper chains this carry correctly.

**BCD subtract (for bonus/fuel decrement in DO.N):** Similar approach, with borrow instead of carry:
```c
static uint8_t bcd_sub(uint8_t a, uint8_t b, uint8_t *borrow) {
    // subtract b from a with borrow-in
    // if lo underflows: add 10, set borrow-out
    // if hi underflows: add 10, set borrow-out
}
```

---

### 5.13 DEMO.STICK data

108-byte table of joystick replay values. Each byte encodes STICK register bits (RIGHT=0x08, LEFT=0x04, DOWN=0x02, UP=0x01, all active-low, all bits set = no movement = 0x0F).

```
Byte values: 0x0B = LEFT pressed (bit 2 clear)
             0x09 = LEFT+DOWN pressed
             0x0A = LEFT+UP pressed
             0x07 = RIGHT pressed (bit 3 clear)
             0x05 = RIGHT+DOWN
             0x06 = RIGHT+UP
             0x0D = DOWN only
             0x0E = UP only
             0x0F = no direction
```

The demo plays a pre-recorded flight path demonstrating the game during the attract mode.

---

## 6. Variable Declarations Required in fort4.c

### New variables (not in any existing C file):

| C name | Assembly source | Type |
|--------|----------------|------|
| `s1_2_val` | `fort.s: S1.2.VAL` | `extern uint8_t` |
| `trig_flag` | `fort7.s: TRIG.FLAG` | `extern uint8_t` |
| `elevator_num` | `fort7.s: ELEVATOR.NUM` | `extern uint8_t` |
| `laser_shapes[32]` | `fort6.s: LASER.SHAPES` | `extern const uint8_t` |
| `ddig_x` | (new: X register substitute) | `uint8_t` (defined here) |

### Variables already declared in other C files but needed by fort4.c:

From fort1.c externs: `adr1_lo/hi`, `temp1/2`, `s_adr_lo/hi`, `s_flg`, `mode`, `score1/2/3`, `bonus1/2`, `fuel1/2`, `fuel_status`, `laser_status`, `laser_spd`, `tim1_val/tim2_val`, `chopper_status/x/y/angle`, `chop_x/y`, `sx/sy/sx_f/sy_f`, `demo_status`, `demo_count`, `elevator_spd/dx/tim`, `s2_val`

From fort3.c externs: `adr1_i_lo/hi`, `temp1_i/temp2_i`, `r_x/r_y`, `robot_status`, `rocket_status[3]/rocket_x[3]/rocket_y[3]`

All re-declared as `extern` in fort4.c. No conflict since fort1.c is not in the compile command for the syntax test.

---

## 7. Hardware Macros Defined in fort4.c

```c
#define REG(a)       (*(volatile uint8_t *)(uintptr_t)(a))
#define FRAME_HW     REG(0x0014)   /* OS frame counter (FRAME = $14) */
#define RANDOM_HW    REG(0xD20A)   /* Atari hardware LFSR */
#define TRIG0_HW     REG(0xD010)   /* fire button (0=pressed, 1=released) */
#define HSCROL_HW    REG(0xD404)   /* ANTIC fine horizontal scroll */
#define VSCROL_HW    REG(0xD405)   /* ANTIC fine vertical scroll */
#define STICK_HW     REG(0x0278)   /* OS joystick shadow (STICK0 = $278) */
```

These are per-TU definitions; fort1.c and fort3.c define their own copies with the same or similar names. No ODR conflict since macros are preprocessor-only.

---

## 8. Design Decisions and Rationale

### 8.1 `ddig_x` as a separate global

The assembly uses the CPU X register to track the digit position counter across DDIG/DRAW calls. In the existing fort1.c, `demo_count = 0` is set before single-digit ddig calls — this appears to be a naming confusion from the prior conversion session, treating `demo_count` as a proxy for the X register. 

Rather than perpetuate this confusion, fort4.c defines `uint8_t ddig_x` (declared in fort4.h as extern). The DO.N function sets `ddig_x` appropriately before each digit group. External callers (like fort1.c's m_new_player) should set `ddig_x = 0` for single-digit displays — though the existing code sets `demo_count = 0` instead, which works coincidentally because that call happens to not need blank suppression (chop_left is a small number, displayed at the "last digit" position).

### 8.2 `laser_shapes` as extern

`laser_shapes[32]` is defined in `fort6.s` which has not been converted yet. Declaring it `extern const uint8_t laser_shapes[32]` allows the syntax check to pass; the definition will be provided when `fort6.c` is written.

### 8.3 Static `elevators[]` with literal addresses

The ELEVATORS table in fort4.s is:
```
.DA BLOCK.5, BLOCK.6, BLOCK.7, BLOCK.8
```
(four 2-byte address constants).

In C, this becomes:
```c
static uint8_t * const elevators[4] = {
    (uint8_t *)0x0C70u,  /* BLOCK.5 */
    (uint8_t *)0x0C78u,  /* BLOCK.6 */
    (uint8_t *)0x0C80u,  /* BLOCK.7 */
    (uint8_t *)0x0C88u,  /* BLOCK.8 */
};
```

Using literal addresses avoids the need for a static array initializer that references a non-constant pointer expression (which `CHR_SET2 + 112u` technically is in strict C, since the cast from integer to pointer is implementation-defined). The literal values are pre-computed: `0x0C00 + 112 = 0x0C70`, etc.

### 8.4 CHR_SET2 as writable pointer

fort3.c defines `CHR_SET2` as `((const uint8_t *)0x0C00u)` (read-only). fort4.c needs to write to CHR_SET2 sub-areas (LASERS_1, BLOCK_1, EXPLOSION, etc.), so it defines `CHR_SET2` as `((uint8_t *)0x0C00u)` (writable). These are in different translation units so there's no conflict.

### 8.5 BCD arithmetic helpers

The 6502 `SED`/`CLD` instructions enable hardware BCD mode for ADC/SBC. Since C has no equivalent, software BCD helpers `bcd_add()` and `bcd_sub()` are implemented as `static` functions in fort4.c. They correctly handle per-nibble carry/borrow propagation.

### 8.6 `do_stick` and `do_n` as static

These are implementation helpers only called from their respective public wrappers (`read_stick`, `do_numbers`). Making them `static` matches the assembly's use of internal labels and prevents name conflicts with future files.

### 8.7 DRAW.MAP scroll clamp logic

The assembly's `DO.Y` section has a complex structure with labeled branches. The C translation uses `y_check` to track the "last loaded chopper_y" (the X register value in assembly) for the up-scroll comparisons. This correctly mirrors the assembly behavior where the X register is loaded once at `.80` and then reused at `.3` and `.81` even if `chopper_y` was subsequently clamped.

---

## 9. Potential Issues / Known Limitations

### 9.1 `fort1.c`'s hi-score ddig call

In `m_game_over` (fort1.c), `ddig(hi3); ddig(hi2); ddig(hi1)` is called without setting `ddig_x`. This means `ddig_x` will have whatever value remained from the last `do_numbers()` call (likely 255 after the fuel display's last draw decremented from 0). Leading zero suppression will be incorrect for this display. This is a pre-existing bug in the fort1.c translation and is out of scope for this session.

### 9.2 `s_temp` not mapped

The assembly variable `S.TEMP` is used in DDIG/DRAW as a temporary save of the Y register between the two DRAW calls. In C, there's no need for this temporary (the `y` local variable serves the same purpose). `S.TEMP` is therefore not exposed in fort4.c at all — it's purely an assembly implementation detail with no semantic significance in C.

### 9.3 `laser_shapes` pending fort6.c

Until `fort6.c` is written, `do_laser_1` and `do_laser_2` will have an unresolved external reference at link time. The syntax check passes because `-fsyntax-only` does not link.

### 9.4 `robot_status` vs sprite status

In `read_stick`, the guard checks `chopper_status == STATUS_OFF || chopper_status == STATUS_CRASH`. These use the `STATUS_OFF = 1` and `STATUS_CRASH = 4` constants from fort1.h. For rockets, a different convention applies: `rocket_status[i] == 0` means free (not STATUS_OFF = 1). This is intentional — rockets use 0 for free and 1-5 for direction, while chopper/robot use the STATUS_* constants.

---

## 10. Files Produced

### `dev/src/fort4.h` (45 lines)

```c
#pragma once
#include <stdint.h>

/* Digit position counter */
extern uint8_t ddig_x;

void pos_chopper(void);
void pos_robot(void);
void read_stick(void);
void hover(void);
void read_trig(void);
void draw_map(void);
void compute_map_adr_i(void);
void compute_map_adr(void);
void do_laser_1(void);
void do_laser_2(void);
void do_blocks(void);
void do_elevator(void);
void do_exp(void);
void do_numbers(void);
void ddig(uint8_t val);
void inc_score(uint8_t hi, uint8_t lo);
```

### `dev/src/fort4.c` (605 lines)

Sections in order:
1. Includes: `stdint.h`, `fort1.h`, `fort4.h`, `fnt1.h`
2. Hardware REG macros: FRAME_HW, RANDOM_HW, TRIG0_HW, HSCROL_HW, VSCROL_HW, STICK_HW
3. Memory address macros: CHR_SET2, SCANNER_BASE, RAM1_STUFF, RAM2_STUFF_BASE
4. Named CHR_SET2 sub-areas: LASERS_1, LASERS_2, LASER_3, BLOCK_1-5, EXPLOSION_PTR, EXPLOSION2_PTR, MISS_CHR_LEFT, MISS_CHR_RIGHT
5. Display list area macros: DSP_MAP_PTR, SCORE_DIG_PTR, BONUS_DIG_PTR, FUEL_DIG_PTR
6. Scroll constants: MIN_RIGHT/LEFT, MIN/MAX DOWN/UP
7. Joystick masks: JOY_RIGHT/LEFT/DOWN/UP
8. Extern variable declarations (all variables used by fort4 functions)
9. Module globals: `uint8_t ddig_x`
10. Static data: `demo_stick[108]`, `elevators[4]`
11. Static helpers: `pos_it_i()`, `draw()`, `bcd_add()`, `bcd_sub()`, `hover()`, `do_stick()`, `do_n()`
12. Public functions: all 17 exported functions

---

## 11. Compile Test

```
$ cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
     dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c
(no output — clean pass)
```

Zero warnings, zero errors, all three translation units accepted.

---

## 12. Next Steps

The remaining assembly files to convert are:
- `fort5.s` — likely contains slave/entity AI
- `fort6.s` — laser shapes, missile/rocket graphics, `laser_shapes[32]` must be defined here
- `fort7.s` — variable declarations (zero page, BSS) — may need a `fort7.c` for definitions
- `fort8.s` — display list initialization data
- `fnt2.s` — second character set (already partially handled by fnt2.c)

The fort6.c conversion should be next to resolve the `laser_shapes` extern in fort4.c.
