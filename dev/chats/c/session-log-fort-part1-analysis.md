# Session Log: convert fort.s to C — Part 1: Analysis

## Task

Convert `fort.s` (Fort Apocalypse master/linker assembly file) to C.
Output files: `dev/src/fort.h` and `dev/src/fort.c`.

---

## fort.s — Source File Overview

`fort.s` is the **top-level master file** that ties the entire ROM together.
It is 280 lines and contains no 6502 game logic. Its content is entirely:

1. Hardware register `.EQ` equates
2. Memory layout constants
3. Zero-page variable allocation (`.OR` + `.BS` directives)
4. `.IN` include directives (which become the separate fort1–fort8.c files)
5. A cartridge boot vector at `END.CART`

This file is the direct source of truth for every hardware address, every
memory region boundary, and every zero-page scratch variable used across
all eight fort*.s modules.

---

## Content Sections in fort.s

### Hardware equates (`.EQ` directives)

All Atari 400/800 hardware registers used by the game, in five groups:

**OS RAM shadow registers** (CPU-accessible mirrors of hardware):
```
FRAME    .EQ $14       ATTRACT  .EQ $4D
VDSLST   .EQ $200      VVBLKI   .EQ $222      VVBLKD   .EQ $224
SDMCTL   .EQ $22F      SDLST    .EQ $230      PRIOR    .EQ $26F
PCOLR0   .EQ $2C0  ..  PCOLR3   .EQ $2C3
COLOR0   .EQ $2C4  ..  COLOR4   .EQ $2C8
CHBAS    .EQ $2F4      CH       .EQ $2FC      CH2      .EQ $2F2
STICK    .EQ $278
CDTMV1   .EQ $218      CDTMV2   .EQ $21A
CDTMA1   .EQ $226      CDTMA2   .EQ $228
```

**GTIA registers** ($D000–$D01F):
Collision read (M0PF–P3PL), position/size write (HPOSP0–3, HPOSM0–3,
SIZEP0–3, SIZEM), colour (COLPM0–3, COLPF0–3, COLBK), TRIG0, HITCLR,
GRACTL, CONSOL.

Note: GTIA uses the **same addresses** for read (collision) and write
(position/size). fort.s defines both names:
```
M0PF .EQ $D000   ; read: missile 0 / playfield collision
HPOSP0 .EQ $D000 ; write: horizontal position player 0
SIZEP0 .EQ $D008 ; write: player 0 size
M0PL   .EQ $D008 ; read: missile 0 / player collision
SKCTL  .EQ $D20F ; write: serial control
SKSTAT .EQ $D20F ; read: serial status
```
All aliases are preserved in `fort.h`.

**ANTIC registers** ($D400–$D40F):
DMACTL, DLIST, HSCROL, VSCROL, PMBASE, CHBASE, WSYNC, VCOUNT, NMIEN.

**POKEY registers** ($D200–$D20F):
AUDF1–4, AUDC1–4, AUDCTL, KBCODE, RANDOM, SKCTL/SKSTAT.

**OS ROM addresses** (not registers — fixed ROM routine entry points):
```
VVBLKI.RET .EQ $E45F   ; return address for immediate VBlank handlers
VVBLKD.RET .EQ $E462   ; return address for deferred VBlank handlers
```

### Constants

```
MIS     .EQ $300       ; missile data
PL0     .EQ $400       ; player 0 sprite data
PL1..3  .EQ $500–$700
RIGHT   .EQ $8         ; joystick direction bits
LEFT    .EQ $4
DOWN    .EQ $2
UP      .EQ $1
CHECK.SUM .EQ $264C

PLAYER    .EQ $0
PLAY.SCRN .EQ $300
CHR.SET1  .EQ $800     ; game font
CHR.SET2  .EQ $C00     ; graphics font
MAP       .EQ $1100+3  ; game map ($1103)
SLAVES    .EQ $3904
SCANNER   .EQ $39C0

S.LINE1 .EQ CHR.SET1+736    ($0AE0)
S.LINE2 .EQ CHR.SET1+832    ($0B40)
S.LINE3 .EQ CHR.SET1+928    ($0BA0)

LASERS.1..2, LASER.3         (CHR.SET2 sub-regions)
BLOCK.1..8                   (CHR.SET2 sub-regions)
WINDOW.1 .EQ CHR.SET2+712    ($0EC8)
WINDOW.2 .EQ CHR.SET2+720    ($0ED0)

EXPLOSION  .EQ CHR.SET2+256  ($0D00)
EXPLOSION2 .EQ CHR.SET2+504  ($0DF8)
MISS.CHR.LEFT  .EQ CHR.SET2+904
MISS.CHR.RIGHT .EQ CHR.SET2+912

EXP       .EQ $20            ; explosion tile
EXP2      .EQ $3F
MISS.LEFT .EQ $71
MISS.RIGHT .EQ $72
EXP.WALL  .EQ $47+128        ; = $C7

MAX.LEFT  .EQ 48    MAX.RIGHT .EQ 192
MAX.UP    .EQ 100   MAX.DOWN  .EQ 212
MAX.FUEL  .EQ $2000
MIN.LEFT  .EQ 110   MIN.RIGHT .EQ 130
MIN.UP    .EQ 146   MIN.DOWN  .EQ 166
MAX.TANKS .EQ 6
POD.SPEED .EQ 15
```

### Zero-page variable allocation

```asm
         .OR $15
ADR1     .BS 2         ; 16-bit scratch pointer (lo/hi)
ADR2     .BS 2
TEMP1    .BS 1    ..  TEMP6  .BS 1
TEMP.MODE .BS 1

ADR1.I   .BS 2         ; interrupt-context copies
ADR2.I   .BS 2
TEMP1.I  .BS 1    ..  TEMP4.I  .BS 1
S.ADR    .BS 2
S.TEMP   .BS 1
S.FLG    .BS 1
TANK.START.X .BS MAX.TANKS     ; 6 bytes
TANK.START.Y .BS MAX.TANKS
TIM1.VAL .BS 1    ..  TIM9.VAL .BS 1
SSIZEM   .BS 1

         .OR $43
S1.1.VAL .BS 1   S1.2.VAL .BS 1
S2.VAL   .BS 1   S3.VAL   .BS 1   S4.VAL .BS 1
S5.VAL   .BS 1   S6.VAL   .BS 1
GAME.POINTS .BS 1
DEMO.STATUS .BS 1
DEMO.COUNT  .BS 1

         .OR POD.1         ; = $0FF8 (CHR.SET2+920)
POD.STATUS   .BS MAX.PODS  ; 39 bytes
POD.DX       .BS MAX.PODS

         .OR POD.2         ; = $3925
POD.X        .BS MAX.PODS
POD.Y        .BS MAX.PODS
POD.TEMP1    .BS MAX.PODS
POD.TEMP2    .BS MAX.PODS

         .OR SLAVES        ; = $3904
SLAVE.STATUS .BS 8
SLAVE.X      .BS 8
SLAVE.Y      .BS 8
SLAVE.DX     .BS 8

         .OR $50
         .IN "H1:FORT7.S"   ; fort7.s allocates more vars starting at $50
```

### Include directives

```
.IN "H1:FORT7.S"    (at .OR $50 — extends zero-page layout)
.IN "H1:FNT1.S"
.IN "H1:FNT2.S"
.IN "H1:FORT1.S"  ..  .IN "H1:FORT8.S"
```

All already converted to separate C files (`fort7.c`, `fnt1.c`, etc.).

### Cartridge boot vector (END.CART)

```asm
END.CART
         .BS $BFFA-*       ; pad to $BFFA
         .DA CART.START     ; $BFFC-$BFFD: cold start address
         .HS 00             ; $BFFE: reserved
         .DA #%10000100     ; $BFFF: cartridge flags ($84)
         .DA CART.START
```

Standard Atari 8-bit cartridge header. Not reproduced in the C port.

---

## Survey of Existing C Files

Before writing code, the existing fort*.c and fort*.h files were read to
establish what was already defined and what was missing.

### fort7.c — already defines

`fort7.s` starts at `.OR $50`. Its C equivalent (`fort7.c`) already defines:
`scan_adr1_lo/hi`, `sx`, `sy`, `sx_f`, `sy_f`, `consol_flag`, `trig_flag`,
`level`, `mode`, `land_*`, `chopper_*`, `chop_*`, `robot_*`, `r_*`,
`rocket_*[3]`, `elevator_*`, `score1-3`, `hi1-3`, `bonus1-2`,
`fuel_status`, `fuel_temp`, `fuel1/2`, `bak_color`, `bak2_color`,
`cm_*[MAX_TANKS]`, `tank_*[MAX_TANKS]`,
`pod_num`, `pod_com`, `slave_num`, `slaves_left`, `slaves_saved`,
`fort_status`, `laser_status`, `laser_spd`,
`tank_spd/speed`, `missile_spd/speed`,
`grav_skill/skl`, `pilot_skill/skl`, `chops`, `chop_left`, `opt_num`,
`start_pods`.

### Hardware register access in existing fort*.c files

Each existing fort*.c file **redeclares `REG()` and hardware register
macros locally** rather than sharing them. Typical pattern in fort2.c:

```c
#define REG(a)      (*(volatile uint8_t *)(uintptr_t)(a))
#define CONSOL_HW   REG(0xD01F)
#define TRIG0_HW    REG(0xD010)
...
```

Fort5.c uses a similar pattern with different name suffixes.
The existing files are not updated to use `fort.h` — that refactor is a
separate task. `fort.h` provides the canonical shared source of truth
going forward.

### Variables declared `extern` in fort1.c but NOT in fort7.c

Cross-referencing fort1.c's extern block against fort7.c's definitions
identified the missing definitions:

| Variable | fort.s source | Status |
|---|---|---|
| `adr1_lo`, `adr1_hi` | ADR1 @ $15 | **missing** |
| `adr2_lo`, `adr2_hi` | ADR2 @ $17 | **missing** |
| `temp1`–`temp6` | TEMP1–6 @ $19 | **missing** |
| `temp_mode` | TEMP.MODE @ $1F | **missing** |
| `adr1_i_lo/hi`, `adr2_i_lo/hi` | ADR1.I/ADR2.I @ $20 | **missing** |
| `temp1_i`–`temp4_i` | TEMP1.I–4.I @ $24 | **missing** |
| `s_adr_lo/hi`, `s_temp`, `s_flg` | S.ADR/S.TEMP/S.FLG | **missing** |
| `tank_start_x/y[6]` | TANK.START.X/Y | **missing** |
| `tim1_val`–`tim9_val` | TIM1–9.VAL | **missing** |
| `ssizem` | SSIZEM | **missing** |
| `s1_1_val`, `s1_2_val`, `s2_val`–`s6_val` | S1.1.VAL etc. | **missing** |
| `game_points`, `demo_status`, `demo_count` | GAME.POINTS etc. | **missing** |
| `pod_status[39]`, `pod_dx[39]` | POD.STATUS/DX | **missing** |
| `pod_x/y/temp1/temp2[39]` | POD.X etc. | **missing** |
| `slave_status/x/y/dx[8]` | SLAVE.STATUS etc. | **missing** |

All of these need definitions in `fort.c`.

---

## Design Decisions

| Decision | Rationale |
|---|---|
| `fort.h` centralises hardware register `#define`s | fort.s is the authoritative source for these addresses; the per-file redeclarations in fort2.c/fort5.c etc. are a maintenance problem that this header resolves going forward |
| Keep both alias names for shared addresses | fort.s explicitly names both `M0PF`/`HPOSP0` (same $D000), `SKCTL`/`SKSTAT` (same $D20F) etc.; both names are preserved for readability at call sites |
| No `ORG` or linker script in C | The C port runs on a host system; fixed Atari addresses are cast constants, not linker-placed |
| Cartridge boot vector not reproduced | Atari ROM boot header ($BFFA–$BFFF) is hardware-specific; irrelevant on a hosted build |
| Don't modify existing fort*.c files | They compile cleanly as-is; centralising into fort.h is a future refactor, not part of this task |
| Variables NOT in fort7.c go in fort.c | fort7.c covers the $50-onward zero-page range; fort.c covers $15–$4F plus the POD/SLAVE fixed-address arrays |
| `VVBLKI_RET`/`VVBLKD_RET` as numeric constants | These are OS ROM addresses used as interrupt return destinations, not register accesses |

---

## File Summary (Input)

| Property | Value |
|---|---|
| Source file | `fort.s` |
| Lines | 280 |
| Content type | Master equates + variable layout + includes + cart vector |
| Hardware equates | ~60 named register addresses |
| Memory constants | ~35 named regions / sub-regions |
| Zero-page variables | ~43 byte-sized + 4 arrays of MAX_TANKS + 6 pod arrays + 4 slave arrays |
| Include directives | 10 (FNT1, FNT2, FORT1–FORT8) |
