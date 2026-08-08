# Session Log: convert fort2.s — Part 1: Analysis

## Task
Convert `fort2.s` (Fort Apocalypse options, pods, missiles, tanks, screen/print utilities)
to C. Output files: `dev/src/fort2.h` and `dev/src/fort2.c`.
Source: 1153 lines of Atari 400/800 6502 SynAssembler.

---

## fort2.s — Source File Overview

Fort2.s is a large multi-purpose file. Unlike fort1.s (entry point and mode dispatch),
fort2.s contains the mid-layer game systems that run every frame. It covers:

1. **Console/keyboard input** (`READ.USER`, `CHECK.OPTIONS`, `PRINT.OPTS`)
2. **Animated fuel pods** (`MOVE.PODS`, `MP1`, `P.BEGIN`, `P.COL`, `P.ERASE`, `P.DRAW`, `P.MOVE`)
3. **Tank-fired cruise missiles** (`MOVE.CRUISE.MISSILES`, `MM1`, `M.BEGIN`, `M.COL`, `M.COL2`, `M.ERASE`, `M.MOVE`, `M.DRAW`)
4. **Hyperspace teleport** (`CHECK.HYPER.CHAMBER`)
5. **Tank movement** (`MOVE.TANKS`, `MT2`, `CHECK.TANK.COL`, `POS.TANK`)
6. **Screen management** (`SCREEN.ON`, `SCREEN.OFF`, `CLEAR.SOUNDS`)
7. **Print utilities** (`CCL`, `PRINT`, `CLEAR.INFO`)
8. **Frame sync + bonus** (`WAIT.FRAME`, `GIVE.BONUS`)
9. **Glyph pixel test** (`CHECK.CHR`)

---

## Function-to-Label Mapping

| Assembly label | C function | Visibility |
|---|---|---|
| READ.USER | `read_user()` | public |
| CHECK.OPTIONS | `check_options()` | public |
| PRINT.OPTS | `print_opts()` | static |
| MOVE.PODS | `move_pods()` | public |
| MP1 | `mp1()` | static |
| P.BEGIN | `p_begin()` | static |
| P.COL | `p_col()` | static |
| P.ERASE | `p_erase()` | static |
| P.DRAW | `p_draw()` | static |
| P.MOVE | `p_move()` | static |
| GET.POD.ADR / GET.POD.VAL / PUT.POD.VAL / POS.POD | `pod_map_ptr()` | static inline |
| MOVE.CRUISE.MISSILES | `move_cruise_missiles()` | public |
| MM1 | `mm1()` | static |
| M.BEGIN | `m_begin()` | static |
| M.COL | `m_col()` | static |
| M.COL2 | `m_col2()` | static |
| M.ERASE | `m_erase()` | static |
| M.MOVE | `m_move()` | static |
| M.DRAW | `m_draw_and_check()` | static |
| GET.MISS.ADR | `cm_map_ptr()` | static inline |
| CHECK.HYPER.CHAMBER | `check_hyper_chamber()` | public |
| CHECK.CHR | `check_chr_byte()` + `check_chr()` stub | static + public |
| POS.TANK | `pos_tank()` | static |
| MOVE.TANKS + MT2 | `move_tanks()` | public |
| CHECK.TANK.COL | `check_tank_col()` | static |
| SCREEN.ON | `screen_on()` | public |
| SCREEN.OFF | `screen_off()` | public |
| CLEAR.SOUNDS | `clear_sounds()` | public |
| CCL | `ccl()` | public |
| PRINT | `print()` | public |
| GIVE.BONUS | `give_bonus()` | public |
| WAIT.FRAME | `wait_frame()` | public |
| CLEAR.INFO | `clear_info()` | public |

---

## Static Data Tables

### Option Screen Strings (raw ASCII, `.AT /TEXT/`, terminated `0xFF`)

```
OPTT1 "OPTIONS"        OPTT2 "OPTION"       OPTT3 "SELECT"
OPT1  "GRAVITY SKILL"  OPT2  "PILOT SKILL"  OPT3  "ROBO PILOTS"
OPT1.1 "WEAK    "  OPT1.2 "NORMAL"  OPT1.3 "STRONG"
OPT2.1 "NOVICE"    OPT2.2 "PRO      " OPT2.3 "EXPERT"
OPT3.1 "SEVEN   "  OPT3.2 "NINE    "  OPT3.3 "ELEVEN"
```

Key: `.AT /TEXT/` = raw ASCII bytes (no ATARI screen code conversion).
Terminated with `.HS FF` = 0xFF.

In C: `static const uint8_t optt1[] = { 'O','P','T','I','O','N','S', 0xFF }` etc.

### POD.CHR (8 bytes, animation frame tile pairs)
```
.HS 4000 5B5C 5D5E 005F
→ {0x40, 0x00, 0x5B, 0x5C, 0x5D, 0x5E, 0x00, 0x5F}
```
Index 0→pair (0x40,0x00), index 2→pair (0x5B,0x5C), etc.
Selected by `pod_com >> 3` (gives 0, 2, 4, or 6 — always even).

### Hyperspace Exit Tables (4 entries each)
```
H.XF: {0xDD, 0x76, 0x10, 0x4B}   H.YF: {0x7A, 0x7B, 0xB8, 0xB8}
H.X:  {0x22, 0xBC, 0x55, 0x87}   H.Y:  {0x0F, 0x0F, 0x18, 0x18}
H.CX: {0x73, 0x78, 0x76, 0x75}   H.CY: {0x8C, 0x89, 0xAF, 0xAF}
```
Random index 0–3 from `RANDOM & 3` selects exit position, scroll position (SX/SY/SXF/SYF),
and chopper spawn coordinates (CHOPPER.X/Y).

---

## Key Translation Challenges

### 1. PRINT Calling Convention Mismatch

**Assembly PRINT** entry convention: X=lo(string), Y=hi(string), TEMP1=col, TEMP2=row.
It calls CCL internally, which sets ADR2 to the screen address; the string is
in ADR2 for the printing loop.

**Problem:** fort1.c's existing `print_str()` wrapper (already written before this
conversion) puts the string pointer in ADR1, not ADR2 — the opposite convention.

**Resolution:** Match the fort1.c convention throughout fort2.c.
- `print()` reads the string from ADR1 (not ADR2)
- `print()` computes the screen address internally from temp1/temp2
- All assembly `PRINT` call sites become `print_str()` wrapper calls that set ADR1

CCL is still public (called directly by other modules) but print() no longer
calls ccl() — it computes the screen address directly to avoid the redundant
ADR1 overwrite.

### 2. WAIT.FRAME Stack Reset Pattern

Assembly end-of-WAIT.FRAME when mode changes:
```asm
LDX #$FF
TXS          ; reset 6502 stack pointer to $01FF (empty stack)
JMP MAIN     ; jump to MAIN — abandons all return addresses
```

This is a hardcoded longjmp: the entire call chain is discarded. In C there is
no instruction for this. The C equivalent: call `main_loop()` which is an
infinite `for(;;)` loop and never returns. This correctly abandons the call
chain by never unwinding it.

```c
if (mode != saved) {
    main_loop();   /* never returns — equivalent to LDX #$FF; TXS; JMP MAIN */
}
```

### 3. POD.COM Global Animation Counter

`POD.COM` is a single zero-page byte shared across all 39 pods.

Each call to `mp1()` advances `pod_com` for the pod being processed. Since 15
pods are processed per frame and all share the same counter, pods naturally
drift out of animation phase with each other — pod 0 gets counter value N, pod 1
gets N+some-advance, pod 2 gets further still. This creates organic staggering
without any explicit per-pod phase offset.

In C: `pod_com` is an extern `uint8_t`, preserving this shared-state behaviour.

### 4. CM_LEFT == STATUS_CRASH Numeric Conflict

From fort7.s:
```
CM.LEFT     .EQ 4   (cruise missile going left)
STATUS.CRASH .EQ 4  (tank crashed)
```

Same value, entirely different semantics. The cm_status array uses CM_LEFT/CM_RIGHT;
the tank_status array uses STATUS_CRASH (via fort1.h). In C, both are correct
because the arrays are typed separately:
```c
#define CM_LEFT   4u
#define CM_RIGHT  8u
```
`STATUS_CRASH` comes from fort1.h and is only used on `tank_status[]` comparisons.

### 5. MAP ROW ABOVE Addressing (`ADR1 - 256 + 1`)

The tank direction indicator tile (the 'o'/'p' glyph) lives one row above the
tank body and one column to the right. In PLAY_SCRN layout, each row is exactly
256 bytes (one page). So "one row above, col+1" from a given map pointer is:
```c
mp[-256 + 1] = indicator_byte;
```
This is valid pointer arithmetic since PLAY_SCRN is a contiguous 768-byte array
and tanks never occupy row 0 (where -256 would access below PLAY_SCRN).

### 6. TANK.SHAPE is a Label Inside HIT.LIST

In fort3.s:
```asm
HIT.LIST:     .HS 40 5B5C5D5E5F 3B3C3D3E494A     ; 12 bytes
TANK.SHAPE:   .HS 4C4D4E4F50 7172                ; continues here
HIT.LIST2.LEN .EQ *-HIT.LIST-1   ; = 21
```

TANK.SHAPE is not a separate variable — it is just a label inside HIT.LIST
marking byte offset 12. In C:
```c
extern const uint8_t hit_list[];
#define HIT_LIST2_LEN  21u
#define TANK_SHAPE     (hit_list + 12)
```

### 7. M.ERASE Restore Filter Logic

Assembly M.ERASE/M.COL2: the map byte saved under a missile is restored only if it
is an "ordinary" background tile. Several categories must NOT be restored:

| Category | Condition | Reason |
|---|---|---|
| Reverse-video tiles | byte >= 0xE0 | Already rendered foreground chars |
| Pod tile (0x40) | byte == CHR_POD_TILE | Would erase live pod |
| Pod/tank anim tiles | 0x5B–0x5F | Animation characters |

Assembly implements this via nested BEQ/BLT/BGE branches. C equivalent:
```c
if (!(saved >= 0xE0u || saved == CHR_POD_TILE ||
      (saved >= 0x5Bu && saved < 0x60u))) {
    mp[0] = saved;
}
```

### 8. CHECK.CHR Register-Passing Convention

Assembly `CHECK.CHR` takes its input in the 6502 A register — whatever the caller
computed last. There is no clean C analog for register passing.

Two-part solution:
- `check_chr_byte(uint8_t chr_byte)` — the real implementation, used by `m_draw_and_check()`
- `check_chr()` — zero-argument public stub, for forward-compatibility with fort3.s
  callers that will be converted later

The stub is needed because fort3.c (when converted) has call sites to CHECK.CHR.
Those callers will be adapted to call `check_chr_byte()` with an explicit argument
when their own conversion is done. The stub prevents linker errors in the interim.

### 9. GIVE.BONUS BCD Add

`SED; CLC; ADC #2; CLD` adds 2 to `chop_left` in BCD mode. This means
`0x09 + 2 = 0x11` (BCD eleven), not `0x0B` (hex eleven).

Only the units nibble needs a carry check for +2. C emulation:
```c
uint8_t lo = chop_left & 0x0Fu;
uint8_t hi = chop_left & 0xF0u;
lo = (uint8_t)(lo + 2u);
if (lo >= 10u) { lo = (uint8_t)(lo - 10u); hi = (uint8_t)(hi + 0x10u); }
chop_left = hi | lo;
```

Hundreds-carry (if chop_left were 0x99) is not handled, matching the assembly.

### 10. CONSOL Active-Low Mapping

```
CONSOL $D01F: bits are 1 when released, 0 when pressed
  0b111 = 7 = no button
  0b110 = 6 = START only
  0b101 = 5 = SELECT only
  0b011 = 3 = OPTION only
```

In READ.USER the assembly uses `CMP #6` (BEQ = START), `CMP #3` (BEQ = OPTION),
`CMP #5` (BEQ = SELECT). The C mirrors this exactly with `== 6`, `== 3`, `== 5`.

---

## External Variables Declared in fort2.c

From fort7.s (verified by direct read of fort7.s):
- `consol_flag`, `tim5_val`, `tim6_val`, `tim7_val`, `temp_mode`
- `opt_num`, `grav_skill`, `pilot_skill`, `chops`
- `chop_x`, `chop_y`, `chopper_x`, `chopper_y`, `chopper_angle`, `chopper_col`
- `chopper_status`, `robot_status`, `r_status`
- `sx`, `sy`, `sx_f`, `sy_f`
- `bak2_color`; sound registers `s1_1_val`, `s1_2_val`, `s2_val`..`s6_val`
- Pod arrays: `pod_num`, `pod_com`, `pod_status[39]`, `pod_x/y/dx/temp1/temp2[39]`
- Tank arrays: `tank_status/x/y/dx/temp/start_x/start_y[6]`, `tank_spd`, `tank_speed`
- Cruise missile: `cm_status/x/y/time/temp[6]`, `missile_spd`, `missile_speed`
- Rocket: `rocket_status/x/temp/tempx/tempy[3]`
- `bonus1`, `bonus2`, `chop_left`, `demo_status`

From fort8.s: `dsp_lst1[]`, `dsp_lst2[]`
From fort3.s: `hit_list[]` (22 bytes, includes TANK.SHAPE at offset 12)

---

## Hardware Registers Added in fort2.c

| Macro | Address | Description |
|---|---|---|
| CONSOL_HW | $D01F | Console buttons (active low) |
| TRIG0_HW | $D010 | Joystick trigger (0=pressed) |
| KBCODE_HW | $D209 | Keyboard scan code |
| SKSTAT_HW | $D20F | Bit 2: 1=no key available |
| RANDOM_HW | $D20A | POKEY noise output |
| AUDC1-4_HW | $D201,$D203,$D205,$D207 | Audio control |
| FRAME_HW | $0014 | OS frame counter |
| SDLST_LO/HI | $0230/$0231 | Display list pointer shadow |
