# Session Log: convert fort1.s — Part 1: Analysis

## Task
Convert `fort1.s` (Fort Apocalypse game logic and startup) to C.
Output files: `dev/src/fort1.h` and `dev/src/fort1.c`.
Source: 1231 lines of Atari 400/800 6502 SynAssembler.

---

## fort1.s — Source File Overview

Fort1.s is the largest and most complex file in the conversion project. It contains:
1. **Program entry point** (`START`) — hardware init, font copy, Z1/Z2 copy, interrupt setup
2. **Title screen** (`TITLE`) — animated title display, font rendering, color cycling
3. **VBlank handlers** (`T2`, `T3`) — title animation and game startup
4. **Main game loop** (`MAIN`) — per-frame game system dispatch
5. **Level completion checks** (`CHECK.LEVEL`, `DO.LEVEL.1/2/3`)
6. **Mode transitions** (`CHECK.MODES`, `M.START`, `M.NEW.PLAYER`, `M.NEW.LEVEL`, `M.GAME.OVER`)
7. **Map decompressor** (`UNPACK`) — RLE decompressor with two character tables
8. **Static data** — string constants, data tables, tile replacement charset tables

---

## Sections of fort1.s

### Hardware Addresses (from fort.s .EQ directives)
Fort.s (separate header file) defines Atari hardware addresses. In fort1.c these become
`#define REG(a) (*(volatile uint8_t *)(uintptr_t)(a))` macros:

| Symbol | Address | Description |
|--------|---------|-------------|
| SDMCTL | $022F | DMA control shadow (OS write-through) |
| PRIOR  | $026F | Priority register shadow |
| GRACTL | $D01D | GTIA graphics control |
| SKCTL  | $D20F | POKEY serial keyboard control |
| PMBASE | $D407 | Player-missile base address register |
| CHBAS  | $02F4 | Character set base shadow |
| VDSLST | $0200 | DLI vector lo/hi |
| VVBLKD | $0224 | Deferred VBlank vector lo/hi |
| SDLST  | $0230 | Shadow display list pointer |
| TRIG0  | $D010 | Joystick trigger 0 (active low: 0=pressed) |
| CONSOL | $D01F | Console keys (active low bits: 0=START, 1=SELECT, 2=OPTION) |
| COLOR0-4 | $02C4-$02C8 | Color shadow registers |
| PCOLR0-1 | $02C0-$02C1 | Player 0/1 color shadows |
| FRAME  | $0014 | OS frame counter |
| VCOUNT | $D40B | Vertical scan line counter |
| NMIEN  | $D40E | NMI enable register |

### Memory Map (from fort.s .EQ directives)
```
PLAYER_BASE = $0000  (player-missile graphics buffer, page-aligned)
RAM2.STUFF  = $0100  (panel display buffer, Z2 copied here)
PLAY.SCRN   = $0300  (main play screen buffer)
CHR.SET1    = $0800  (character set 1, 2KB)
CHR.SET2    = $0C00  (character set 2, 2KB)
RAM1.STUFF  = $0C90  (game display list, Z1 copied here = CHR.SET2+144)
MAP         = $1103  (game map: 40×256 bytes, $2800 bytes)
SCANNER     = $39C0  (scanner bitmap: 1600 bytes = $640 bytes)
PACKED.MAP  = $8000  (compressed level data in cartridge ROM)
WINDOW.1    = CHR.SET2+712 (8-byte sub-region for UI effects)
WINDOW.2    = CHR.SET2+720
```

### String Data (`.AT` / `.AT -` directives)
In SynAssembler:
- `.AT /TEXT/` stores raw bytes (same as ASCII)
- `.AT -/TEXT/` subtracts 0x20 from each character (ATARI screen code conversion)

In C: macro `#define S(c) ((uint8_t)((unsigned char)(c) - 0x20u))` handles the `-` variant.

All strings are terminated with `.HS FF` = 0xFF sentinel byte.

String constants found in fort1.s:
| Assembly | C name | Content |
|----------|--------|---------|
| T.1 | `T_1` | "FORT  APOCALYPSE" (`.AT -`) |
| T.2 | `T_2` | "BY  STEVE  HALES" (`.AT -`) |
| T.3 | `T_3` | "COPYRIGHT" (`.AT -`) |
| T.4 | `T_4` | "SYNAPSE  SOFTWARE" (`.AT -`) |
| T.5 | inline | 8 bytes from fnt1_data[$20*8] |
| NEW.PILOT | `new_pilot` | "GET  READY  PILOT" (`.AT -`) |
| PILOTS.LEFT | `pilots_left` | "PILOTS  LEFT" (`.AT -`) |
| ENTER | `str_enter` | "ENTERING" (`.AT -`) |
| LVL.1 | `lvl_1` | "VAULTS  OF  DRACONIS" (`.AT -`) |
| LVL.2 | `lvl_2` | "CRYSTALLINE  CAVES" (`.AT -`) |
| G.1/A/C/2/3 | `G_1`..`G_3` | mission status strings (raw `.AT`) |
| R.1–R.4 | `R_1`..`R_4` | rank rating strings |
| HS | `HS` | "HIGH  SCORE" (`.AT -`) |

### Data Tables
| Assembly | C name | Type | Notes |
|----------|--------|------|-------|
| GRAV.TAB | `grav_tab[2]` | uint8_t | gravity per skill level |
| ROBOT.TAB | `robot_tab[3]` | uint8_t | robot speed per pilot skill |
| CHOP.TAB | `chop_tab[3]` | uint8_t | starting lives per chops setting |
| LASER.TAB | `laser_tab[3]` | uint8_t | laser speed per pilot skill |
| POD.TAB | `pod_tab[3]` | uint8_t | starting pods per pilot skill |
| TANK.TAB | `tank_tab[1]` | uint8_t | tank speed (only 1 entry) |
| MISSILE.TAB | `missile_tab[3]` | uint8_t | missile speed per pilot skill |
| ELEVATOR.TAB | `elevator_tab[3]` | uint8_t | elevator speed per pilot skill |
| M.TAB | `m_tab[3]` | int8_t | game-points modifier per grav skill |
| LEVEL.COLOR | `level_color[3]` | uint8_t | background color per level |
| LEVEL.CHOP.START | `level_chop_start[3][2]` | uint8_t | chopper start {x,y} per level |
| LEVEL.START | `level_start[3][2]` | uint8_t | scroll start {x,y} per level |
| SCAN.INFO | `scan_info[3]` | const uint8_t * | packed scanner data ptrs per level |
| PACK.ADR | `pack_adr[3]` | const uint8_t * | packed map data ptrs per level |
| TANK.START.X/Y.L1 | `tank_start_x/y_l1[MAX_TANKS]` | uint8_t | level 0 tank starts |
| TANK.START.X/Y.L2 | `tank_start_x/y_l2[MAX_TANKS]` | uint8_t | level 1 tank starts |
| CHR1 / CHR1.L | `chr1[23]` | uint8_t | RLE charset 1 for map decompressor |
| CHR2 / CHR2.L | `chr2[4]` | uint8_t | RLE charset 2 for scanner decompressor |

---

## Key Translation Challenges

### 1. Interrupt Vectors as Function Pointers
The assembly sets VVBLKD and VDSLST as 16-bit address pairs. In C, a helper:
```c
static inline void set_vector(uint16_t addr, void (*fn)(void)) {
    *(volatile uint8_t *)(uintptr_t)(addr)     = (uint8_t)((uintptr_t)fn & 0xFF);
    *(volatile uint8_t *)(uintptr_t)(addr + 1) = (uint8_t)((uintptr_t)fn >> 8);
}
#define SET_VVBLKD(fn)  set_vector(0x0224, (void(*)(void))(fn))
#define SET_VDSLST(fn)  set_vector(0x0200, (void(*)(void))(fn))
#define SET_SDLST(p)    /* lo/hi write to $0230/$0231 */
```

### 2. ADR1/ADR2 16-bit Pointer Reconstruction
The assembly uses zero-page ADR1/ADR2 as 16-bit addresses. In C:
```c
uint8_t *p = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
```
Write-back: `adr1_lo = (uint8_t)(...); adr1_hi = (uint8_t)(...>> 8);`

### 3. Atari Active-Low Hardware
- `TRIG0 = 0` means trigger IS pressed (active low)
- `CONSOL`: bits are 0 when button pressed, 1 when released
  - 7 = `0b111` = no buttons
  - 6 = START pressed
  - 5 = SELECT pressed
  - 3 = OPTION pressed

### 4. 6502 BCD Arithmetic
`SED; SEC; SBC #1; CLD` = BCD decrement. Must be emulated in C nibble-by-nibble.

### 5. PROT Code (Self-Modifying Copy Protection)
Assembly contains self-modifying instructions like `DEC MAIN+32` and `ASL M.NEW.PLAYER`.
These are copy-protection bytes that corrupt code if run on an unauthorized copy.
In C: omitted with comment `/* PROT: omitted */`.

### 6. VVBLKD.RET Jump in VBlank Handler
Assembly exits VBlank handler via `JMP VVBLKD.RET ($E462)` — the OS VBlank return address.
In C: simply `return` from the handler function.

### 7. BLT/BGE Pseudo-ops
SynAssembler uses `BLT` (branch if less-than) = `BCC` (unsigned <) and `BGE` (branch if ≥) = `BCS`.
These compare unsigned bytes: `BLT` = `<`, `BGE` = `>=`.

### 8. T.5 Data
`T.5` is referenced as 8 bytes at `PLAY.SCRN+426`. In fort1.s it actually refers to 8 bytes of fnt1 character $20's glyph data. In C: `const uint8_t *t5 = &fnt1_data[0x20 * 8]`.

---

## Functions Identified in fort1.s

| Assembly label | C function | Description |
|---|---|---|
| START | `fort1_start()` | Entry point: hardware init, font setup, display init |
| TITLE | `title()` | Title screen setup and display loop entry |
| T1 | `t1_loop()` | Raster color animation infinite loop |
| T2 | `t2_vblank()` | Title screen VBlank handler |
| INC.CHR | `inc_chr()` (static) | Cycle temp3 through chars $3B-$3D |
| T3 | `t3_game_start()` | Install game VBlank, enter MAIN loop |
| MAIN | `main_loop()` | Main game loop |
| CHECK.LEVEL | `check_level()` | Dispatch to level completion check |
| DO.LEVEL.1 | `do_level_1()` | Level 0→1 transition |
| MOVE.RAMP | `move_ramp()` (visible) | Animate landing ramp |
| DO.LEVEL.2 | `do_level_2()` | Level 1→2 transition |
| DO.LEVEL.3 | `do_level_3()` | Level 2→3 transition |
| UNPACK | `unpack()` | RLE map/scanner decompressor |
| GET.BYTE | `get_byte()` (static) | Read byte from ADR1, advance ADR1 |
| INC.GAME.POINTS | `inc_game_points(n)` | Add n to game_points |
| CHECK.MODES | `check_modes()` | Dispatch mode transitions |
| M.START | `m_start()` | Initialize new game |
| M.NEW.PLAYER | `m_new_player()` | New pilot setup |
| M.NEW.LEVEL | `m_new_level()` | New level setup, map unpack |
| MAKE.CONTURE | `make_conture()` (static) | Replace 's'/'t' map tiles randomly |
| S.BEGIN | `s_begin()` (static) | Place slaves on map |
| M.GAME.OVER | `m_game_over()` | Game over, score tally, return to title |
| SET.FONTS | `set_fonts()` (static) | Copy FNT1/FNT2 into character sets |
