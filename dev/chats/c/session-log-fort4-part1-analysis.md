# Session Log: convert fort4.s — Part 1: Analysis

## Task
Convert `fort4.s` (Fort Apocalypse interrupt driver part II) to C.
Output files: `dev/src/fort4.h` and `dev/src/fort4.c`.
Source: 765 lines of Atari 400/800 6502 SynAssembler.
File header: "MAIN INTERRUPT DRIVER PART (II) — POSITION THINGS, READ.STICK,
READ.TRIG, DO.LASER.1, DO.LASER.2, DO.BLOCKS, DO.ELEVATOR, DO.EXP,
DO.NUMBERS, DRAW.MAP".

---

## fort4.s — Source File Overview

| Assembly label | C function | Visibility | Role |
|---|---|---|---|
| POS.CHOPPER | `pos_chopper()` | public | Load chop_x/y into temp1_i/temp2_i, call pos_it_i |
| POS.ROBOT | `pos_robot()` | public | Load r_x/r_y into temp1_i/temp2_i, fall through to pos_it_i |
| POS.IT.I | `pos_it_i()` | static | XOR scanner minimap pixel for (temp1_i, temp2_i) |
| READ.STICK | `read_stick()` | public | Guard (OFF/CRASH → return), call do_stick |
| DO.STICK | `do_stick()` | static | Full joystick handler: demo playback + real input |
| HOVER | `hover()` | public | Every 8 frames nudge chopper angle toward neutral [4,14) |
| READ.TRIG | `read_trig()` | public | Fire button with rising-edge detection + rocket launch |
| DRAW.MAP / DO.X | `draw_map()` | public | Horizontal scroll clamp and sx_f advance |
| DRAW.MAP / DO.Y | `draw_map()` cont. | public | Vertical scroll clamp and sy_f advance |
| COMPUTE.MAP.ADR.I | `compute_map_adr_i()` | public | (temp1_i,temp2_i) → adr1_i lo/hi |
| COMPUTE.MAP.ADR | `compute_map_adr()` | public | (temp1,temp2) → adr1 lo/hi |
| DO.LASER.1 | `do_laser_1()` | public | Every 8 frames: copy/clear LASERS.1 + LASER.3 |
| DO.LASER.2 | `do_laser_2()` | public | Every 8 frames: copy/clear LASERS.2 + LASER.3[16..23] |
| DO.BLOCKS | `do_blocks()` | public | Every 128 frames: clear BLOCK.1-4, random refill |
| DO.ELEVATOR | `do_elevator()` | public | Timer-driven: clear BLOCK.5-8, advance elevator slot |
| ELEVATORS | `elevators[]` | static | 4-entry pointer table → BLOCK.5-8 |
| DO.EXP | `do_exp()` | public | Randomise explosion shape, flash missile colours |
| DO.NUMBERS | `do_numbers()` | public | Guard (NEW_PLAYER/GAME_OVER → return), call do_n |
| DO.N | `do_n()` | static | Draw score/bonus/fuel; decrement on schedule |
| DDIG | `ddig(uint8_t val)` | public | BCD byte → 2 digit calls via draw() |
| DRAW | `draw(uint8_t digit)` | static | One nibble → 2 display bytes at s_adr; advance; ddig_x-- |
| INC.SCORE | `inc_score(uint8_t hi, uint8_t lo)` | public | BCD add hi:lo to score1/2/3 |
| DEMO.STICK | `demo_stick[108]` | static | Pre-recorded demo flight-path joystick table |

---

## Critical Offset Calculations

### DSP.MAP within RAM1.STUFF (= CHR_SET2 + 144 = 0x0C90)

Count bytes from the start of the Z1 data block:
```
.HS 70708070           = 4 bytes   (offset 0-3)
.DA #$44,PANEL         = 3 bytes   (offset 4-6)
.HS 04040404           = 4 bytes   (offset 7-10)
.DA #$44,NAVA.PANEL    = 3 bytes   (offset 11-13)
.DA #$44+$80,PLAY.SCRN = 3 bytes   (offset 14-16)
.HS 502080             = 3 bytes   (offset 17-19)
DSP.MAP:               → offset 20
```
Result: `DSP_MAP_PTR = RAM1_STUFF + 20 = 0x0CA4`

### SCORE.DIG, FUEL.DIG, BONUS.DIG within RAM2.STUFF (= 0x0100)

Counting bytes from the Z2 label in fort8.s:
```
.AT /       /           = 7 bytes    offset 0
.HS B3D3A3C3AFCFB2D2A5C5 = 10 bytes  offset 7
.AT /  /                = 2 bytes    offset 17
SCORE.DIG:              → offset 19

(12 + 9 + 20 + 20 + 2 + 8 + 3 + 12 + 3 + 10 + 4 bytes = 103 bytes)
FUEL.DIG:               → offset 122

(8 + 3 + 12 + 4 bytes = 27 bytes)
BONUS.DIG:              → offset 149
```
Results:
- `SCORE_DIG_PTR = 0x0100 + 19  = 0x0113`
- `FUEL_DIG_PTR  = 0x0100 + 122 = 0x017A`
- `BONUS_DIG_PTR = 0x0100 + 149 = 0x0195`

### CHR_SET2 Sub-Area Offsets (CHR_SET2 = 0x0C00)

| Macro | Offset | Absolute |
|---|---|---|
| LASERS_1 | +8 | 0x0C08 |
| LASERS_2 | +40 | 0x0C28 |
| LASER_3 | +72 | 0x0C48 |
| BLOCK_1 | +80 | 0x0C50 |
| BLOCK_2 | +88 | 0x0C58 |
| BLOCK_3 | +96 | 0x0C60 |
| BLOCK_4 | +104 | 0x0C68 |
| BLOCK_5 | +112 | 0x0C70 |
| BLOCK_6 | +120 | 0x0C78 |
| BLOCK_7 | +128 | 0x0C80 |
| BLOCK_8 | +136 | 0x0C88 |
| EXPLOSION | +256 | 0x0D00 |
| EXPLOSION2 | +504 | 0x0DF8 |
| MISS_CHR_LEFT | +904 | 0x0F88 |
| MISS_CHR_RIGHT | +912 | 0x0F90 |

### MAP−5 Base for compute_map_adr_i

`MAP = 0x1103`, so `MAP − 5 = 0x10FE`.
Low byte = `0xFE`, high byte = `0x10`.

Assembly:
```asm
LDA #MAP-5          ; A = 0xFE
CLC; ADC TEMP1.I    ; add tile X (may carry)
STA ADR1.I
LDA /MAP-5          ; A = 0x10
ADC #0              ; add carry
STA ADR1.I+1
LDA TEMP2.I         ; tile Y = row offset (each row = 256 bytes)
CLC; ADC ADR1.I+1
STA ADR1.I+1
```

C:
```c
uint16_t s = (uint16_t)(0xFEu + temp1_i);
adr1_i_lo = (uint8_t)(s & 0xFFu);
adr1_i_hi = (uint8_t)((uint8_t)(0x10u + (uint8_t)(s >> 8)) + temp2_i);
```
`s >> 8` gives 0 or 1 (the carry from the low-byte addition). Then adds `temp2_i` as the
page (row) offset.

---

## Key Technical Concepts

### POS.IT.I — Scanner Minimap Pixel XOR

```asm
; y*40 via shift-add:
A = temp2_i
A = (A<<2 + A) = A*5    ; ASL; ASL; ADC temp2_i
TEMP3.I = 0
ASL; ROL TEMP3.I         ; ×2
ASL; ROL TEMP3.I         ; ×4
ASL; ROL TEMP3.I         ; ×8 → y*40 in 16-bit TEMP3.I:A

adr1_i = SCANNER + 3 + (x>>3) + y*40
*adr1_i ^= POS_MASK1[x & 7]
```

In C: `offset = 3u + (uint16_t)(x >> 3) + (uint16_t)y * 40u`

**POS_MASK1:** `{0x80,0x80,0x20,0x20,0x08,0x08,0x02,0x02}` — pairs of identical bits.
The scanner uses 2bpp (each character cell = 2 horizontal pixels), so bits come in pairs.

**XOR** toggles the pixel on each call. `pos_chopper()` and `pos_robot()` each call it once
per frame: this erases the previous position (XOR again) then draws the new one.

### DRAW.MAP — Scroll Clamp Structure

The horizontal (DO.X) and vertical (DO.Y) sections are structurally identical: each has
a "soft trigger" (scroll when chopper goes past MIN_RIGHT/MIN_DOWN etc.) and a "hard clamp"
(MAX_DOWN/MAX_UP — pixel boundaries that are enforced even when scrolling is stopped at the map edge).

**DO.X logic:**
- Right trigger: `chopper_x > MIN_RIGHT (130)` → clamp, wrap-check sx, `sx_f--`, tile advance if `(sx_f & 3) == 3`
- Left trigger: `chopper_x < MIN_LEFT (110)` → clamp, wrap-check sx, `sx_f++`, tile advance if `(sx_f & 3) == 0`

**Sub-pixel:** HSCROL uses bits 0-1 of sx_f (4 horizontal sub-pixels per tile). Tile boundary
fires when those 2 bits wrap: 0→3 (right) or 3→0 (left).

**DO.Y logic — X-register detail:** The assembly loads CHOPPER.Y into register X once at `.80:`.
At label `.3` (the up-check), the comparison uses this stale X, not the potentially-clamped
CHOPPER.Y. In C, this is tracked with `y_check = chopper_y` updated only at the initial clamp point.
In practice, clamped values (MIN_DOWN=166, MAX_DOWN=212) are always above MIN_UP=146 and MAX_UP=100
so the stale value never triggers an up-scroll accidentally.

**Map edge stops:** `sy == 24` = bottom of map (no more downward scroll); `sy == 0xFF` = top.

### DDIG / DRAW — Leading-Zero Suppression with BCD Nibbles

`s_flg` persists across all `ddig()` calls within one display group (score/bonus/fuel).

**DDIG logic for each nibble (hi then lo):**
```
if s_flg == 0:
    if nibble != 0:  s_flg = 1  (will show digit)
    else:            mark nibble as 0xA (blank), s_flg stays 0
else:
    s_flg = 1  (stays set; all remaining digits shown)
draw(nibble)
```
The blank marker `0xA` is distinct from `0` (digit zero). `draw()` renders `0xA` as spaces
when `ddig_x != 0` (not the last/ones position), and as `0` when `ddig_x == 0` (ones must always show).

**DRAW rendering:** Assembly `0xF0+128` = `0x70` (assembler 16-bit arithmetic truncated to 8 bits).
- Blank (0xA, non-last): hi=0x00, lo=0x00 (= invisible space in Atari mode-4)
- Digit 0: hi=0x90, lo=0x8A
- Digit n: hi=0x90+n, lo=(0x90+n)&0x8F

**Assembly fall-through:** DDIG does NOT call DRAW via JSR for the lo nibble — the lo-nibble
path falls straight into the DRAW label. In C, both `draw(y >> 4)` and `draw(y & 0x0Fu)`
are explicit calls.

### READ.TRIG — Rising-Edge Detection

Assembly uses a 1-bit flag `TRIG.FLAG`:
- `1` = trigger was up (released) last frame
- `0` = trigger was down (pressed) last frame

Rising-edge = first frame where TRIG0==0 AND trig_flag was 1 (just pressed).
```asm
X = TRIG0
if X != 0:              ; trigger released
    trig_flag = X (=1)
    return
; X == 0 (pressed):
if trig_flag == 0:      ; was already pressed last frame
    return              ; no edge
trig_flag = 0           ; record pressed
; → fire event
```

In C:
```c
uint8_t t = TRIG0_HW;
if (t) { trig_flag = t; return; }      // released: store 1
if (!trig_flag) return;                 // held: no edge
trig_flag = 0u;                         // edge detected
```

### HOVER — Neutral-Range Auto-Level

Every 8 frames: if angle outside `[4, 14)` (too far left or right), nudge back by ±2.
```c
if (FRAME_HW & 7u) return;
if (a >= 4u && a < 14u) return;  // already in neutral
if (a < 8u) { chopper_angle += 2u; chopper_angle += 2u; }  // too far left: lean right
else        { chopper_angle -= 2u; chopper_angle -= 2u; }  // too far right: lean left
```
The assembly uses two `INC`/`DEC` instructions rather than `ADD #2` — identical semantics.

### ELEVATOR.DX Flip via EOR #$FE

In `read_trig`: `elevator_dx ^= 0xFEu` (EOR #-2 = EOR #0xFE, flips bits 7:1).
- `0x01 XOR 0xFE = 0xFF = -1` (forward becomes reverse)
- `0xFF XOR 0xFE = 0x01 = +1` (reverse becomes forward)

Fires every time the player launches a rocket. The elevator reverses direction on each shot.

### Demo Mode Playback

`demo_status == 0` = demo running (attract mode).
`demo_status != 0` = real game.

When demo is active, `do_stick()` overwrites the STICK hardware register with
`demo_stick[demo_count]`. demo_count advances every 16 frames, wraps at 108.
The stick values encode directional bits: `0x0B` = LEFT, `0x09` = LEFT+DOWN, `0x07` = RIGHT, etc.
(active-low: bit clear = that direction pressed).

### Rocket Direction from Chopper Angle

Assembly maps `a = (chopper_angle & 0x1E) >> 1` (= angle/2, range 0-8) to rocket status 1-5:
```
a=0 → 1 (fired up-left)    a=1 → 1
a=2 → 2                     a=3 → 3
a=4 → 3 (force 3)           a=5 → 3
a=6 → 4 (a-2)               a=7 → 5
a=8 → 5 (clamped, a-2=6→5)
```

C:
```c
uint8_t a = (uint8_t)((chopper_angle & 0x1Eu) >> 1u);
uint8_t rs;
if      (a >= 6u) rs = (a >= 8u) ? 5u : (uint8_t)(a - 2u);
else if (a >= 4u) rs = 3u;
else              rs = (a == 0u) ? 1u : a;
```

### BCD Arithmetic — Software SED/CLD Emulation

The 6502 `SED`/`CLD` instructions enable hardware BCD mode. For bonus/fuel decrement
the assembly uses `SED; LDA x; SEC; SBC #1; STA x; LDA y; SBC #0; STA y; CLD`.

In C, a static `bcd_sub` helper per-nibble:
```c
static uint8_t bcd_sub(uint8_t a, uint8_t b, uint8_t *borrow) {
    uint8_t lo_a = a & 0x0Fu, lo_b = (uint8_t)((b & 0x0Fu) + *borrow);
    uint8_t brw = 0u, lo, hi_a, hi_b, hi;
    if (lo_a < lo_b) { lo = (uint8_t)(lo_a + 10u - lo_b); brw = 1u; }
    else             { lo = (uint8_t)(lo_a - lo_b); }
    hi_a = a >> 4; hi_b = (uint8_t)((b >> 4) + brw); brw = 0u;
    if (hi_a < hi_b) { hi = (uint8_t)(hi_a + 10u - hi_b); brw = 1u; }
    else             { hi = (uint8_t)(hi_a - hi_b); }
    *borrow = brw;
    return (uint8_t)((hi << 4) | lo);
}
```
The carry/borrow chains correctly across bytes (score1→score2→score3).

---

## External Dependencies Required by fort4.c

### From fort3.c / fort3.h
- `adr1_i_lo`, `adr1_i_hi` — interrupt pointer for scanner and map addressing
- `temp1_i`, `temp2_i` — interrupt temporaries
- `r_x`, `r_y` — robot tile position
- `robot_status` — robot state
- `rocket_status[3]`, `rocket_x[3]`, `rocket_y[3]` — rocket arrays

### From fort1.c / fort1.h
- All game state: `mode`, `score1/2/3`, `bonus1/2`, `fuel1/2`, `fuel_status`
- `laser_status`, `laser_spd`, `tim1_val`, `tim2_val`
- `chopper_status/x/y/angle`, `chop_x/y`, `sx/sy/sx_f/sy_f`
- `demo_status`, `demo_count`, `elevator_spd/dx/tim`
- `s2_val`, `adr1_lo/hi`, `temp1`, `temp2`, `s_adr_lo/hi`, `s_flg`

### New variables (not yet declared in existing C files)
- `trig_flag` — from fort7.s (`TRIG.FLAG .BS 1` = zero-page)
- `elevator_num` — from fort7.s (`ELEVATOR.NUM .BS 1`)
- `s1_2_val` — from fort.s (`S1.2.VAL .BS 1` = $0221, engine sound frequency)

### From fort6.s (not yet converted)
- `laser_shapes[32]` — declared `extern const uint8_t laser_shapes[32]`

### From fnt1.h
- `POS_MASK1` — scanner XOR bitmask table (8 bytes at fnt1 glyph offset 0x0B)
- `EXP_SHAPE` — explosion mask data (8 bytes at fnt1 glyph offset 0x3C)
