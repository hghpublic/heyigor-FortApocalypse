# Fort Apocalypse — ASM to C Conversion: Complete Session Record
**Branch:** agents/asm-to-c-conversion
**Dates:** 2026-08 (multiple sessions)
**Working directory:** dev/src/

---

## Session 1 — fnt1.s: Font Data Conversion

### User prompt
"convert fn1.s to c"

### Reasoning before writing
The first step was to find the file — the prompt said `fn1.s` but `find` turned up `fnt1.s`. I read the README and the assembly file together.

`fnt1.s` is pure bitmap data: 128 character definitions of 8 bytes each = 1024 bytes total. The SynAssembler format uses `.HS` (hex string) directives and `.DA` (define byte) directives. There are also a few `.AT` (ASCII text) directives and several named labels within the data block.

Named labels found in fnt1.s:
- `POS.MASK1` — at char $0B: player/missile position mask
- `FORT.EX1`–`FORT.EX4` — at chars $0C–$0F: explosion animation frames
- `T.5` — at char $20: used by the title screen display (referenced in fort1.s)
- `EXP.SHAPE` — at char $5B: explosion tile shape

The `.AT -/text/` directive at `T.5` was notable. In SynAssembler, `-` flag means "subtract 0x20 from each ASCII byte" — converting to Atari screen codes. The bytes stored are `0x31-0x20=0x11, 0x21-0x20=0x01, ...` etc. I stored the already-computed values verbatim.

### Byte count verification
After writing `fnt1.c`, ran:
```python
import re
data = open('fnt1.c').read()
# First pass: whole file
bytes_list = re.findall(r'0x[0-9A-Fa-f]{2}', data)
# → 1031 bytes (7 extra from pointer declarations)

# Second pass: array body only
array_body = re.search(r'fnt1_data\[128 \* 8\] = \{(.*?)\};', data, re.DOTALL).group(1)
bytes_list = re.findall(r'0x[0-9A-Fa-f]{2}', array_body)
# → 1024 bytes ✓
```

The 7 extra matches in the first pass came from hex constants in pointer declarations like `const uint8_t *const POS_MASK1 = &fnt1_data[0x0B * 8]`. This showed why scoping the regex to the array body was necessary.

### Files created
- `dev/src/fnt1.h` — declares `fnt1_data[1024]` and 6 named pointer constants
- `dev/src/fnt1.c` — 1024-byte array, all `.HS`/`.DA` data verbatim

---

## Session 2 — fnt1_bitmap: C bitmap renderer for fnt1

### User prompt
"create a c function which creates a bitmap from the font data"

### Reasoning about Atari display modes
Before writing anything I had to understand how the Atari hardware actually renders these characters, because there are two fundamentally different pixel encodings in fnt1.s:

**1bpp (ANTIC mode 2):** Each row byte holds 8 pixels, one bit per pixel. Bit 7 = leftmost pixel. Used by chars $00–$0F (explosion shapes, masks, special chars).

**2bpp (ANTIC mode 4):** Each row byte holds 4 pixels at 2 bits per pixel. But the Atari hardware *doubles each pixel horizontally*, so the effective output is still 8 pixels wide. The 4 pixel values 0–3 map to 4 colors. Used by chars $10–$19 (digits) and $41–$5A (A–Z alphabet).

This distinction is critical for a correct bitmap renderer — the same byte data produces visually different output depending on which mode is active for a given character.

### API design
Three functions:
```c
void     fnt1_decode_char(uint8_t chr, FNT1Mode mode, uint8_t out[64]);
uint8_t *fnt1_create_bitmap(const uint8_t *chars, int n, FNT1Mode mode, int *w, int *h);
int      fnt1_write_bmp(const char *path, const uint8_t *px, int w, int h,
                         const uint8_t palette[4][3]);
```

The `out[64]` array is always 8×8 pixels regardless of mode, with values 0–1 (1bpp) or 0–3 (2bpp). The `create_bitmap` function renders N characters side by side into a heap buffer. The BMP writer writes a 24-bit DIB-format BMP with a 54-byte header (BITMAPFILEHEADER + BITMAPINFOHEADER).

### Test: build and run inline
```c
// Render "FORT" (chars $46 $4F $52 $54) in 2bpp mode
const uint8_t word[] = { 0x46, 0x4F, 0x52, 0x54 };
int w, h;
uint8_t *bmp = fnt1_create_bitmap(word, 4, FNT1_2BPP, &w, &h);
// ASCII dump to verify pixel layout
// Also test FORT.EX3 ($0E) in 1bpp — concentric ring shape
```

Both rendered correctly. "FORT" showed the doubled-pixel 2bpp rendering (32×8 output for 4 chars). FORT.EX3 showed the correct concentric ring shape in 1bpp.

### Files created
- `dev/src/fnt1_bitmap.h`
- `dev/src/fnt1_bitmap.c`

---

## Session 3 — fnt2.s + fnt2_bitmap: Graphics font conversion and bitmap engine

### User prompt (part 1)
"convert fn2.s to c"

### fnt2.s analysis
`fnt2.s` is the graphics/terrain font. 128 chars × 8 bytes = 1024 bytes. Key observations:
- $00–$1F: all zeros (blank)
- $20–$72: defined tile graphics
- $73–$7F: all zeros (blank)
- No named labels within the data (unlike fnt1.s)

Notable tile groups:
- **$21–$3A**: A–Z letters using `0x55` border bytes — these are ANTIC mode 4 characters where `0x55 = %01010101` is the background fill pattern
- **$41–$46**: horizontal gradient fills (`0xAA` partial-fill rows) — cave shading tiles
- **$4C–$4F, $52–$53**: corner and edge tiles with diagonal transitions — fort wall building blocks
- **$61–$67**: partial-fill ramp tiles using the `0x55` fade pattern

Byte count verified: exactly 1024 bytes in array body. Compiled clean with `-Wall -Wextra`.

### User prompt (part 2)
"create and test a bitmap converter to c for fnt2"

### Session start issue
When this session opened, attempts to Read `fnt1_bitmap.h` and `fnt1_bitmap.c` failed — those files were in `dev/src/` from the prior session, but the current working directory wasn't set correctly. Ran `ls -la` and `find` to locate the files. Found them at `dev/src/fnt1_bitmap.h` and `dev/src/fnt1_bitmap.c`.

### Architecture decision: generalize to font_bitmap
Rather than copying the fnt1_bitmap code and adjusting for fnt2, I factored out the shared logic into a generic `font_bitmap.h/c` parameterized by `const uint8_t *font_data`. This avoids duplication and makes future font conversions trivial.

The generic engine takes any 1024-byte font array. The font-specific files (`fnt1_bitmap.c`, `fnt2_bitmap.c`) become thin wrappers that pass their respective data pointer.

#### Task tracking
Created three tasks:
1. Create `font_bitmap.h/c` (generic decode + BMP writer)
2. Create `fnt2_bitmap.h/c` (thin wrappers)
3. Write, build, and run `test_fnt2.c`

### font_bitmap API
```c
typedef enum { FONT_1BPP = 0, FONT_2BPP = 1 } FontMode;

void     font_decode_char(const uint8_t *font_data, uint8_t chr,
                           FontMode mode, uint8_t out[64]);
uint8_t *font_create_bitmap(const uint8_t *font_data, const uint8_t *chars,
                             int n, FontMode mode, int *w, int *h);
uint8_t *font_create_sheet(const uint8_t *font_data, int cols,
                            FontMode mode, int *w, int *h);
int      font_write_bmp(const char *path, const uint8_t *px, int w, int h,
                         const uint8_t palette[4][3]);
```

### fnt2_bitmap wrappers
```c
// fnt2_bitmap.h
void     fnt2_decode_char(uint8_t chr, FontMode mode, uint8_t out[64]);
uint8_t *fnt2_create_bitmap(const uint8_t *chars, int n, FontMode mode, int *w, int *h);
uint8_t *fnt2_create_sheet(int cols, FontMode mode, int *w, int *h);
int      fnt2_write_bmp(const char *path, const uint8_t *px, int w, int h,
                         const uint8_t palette[4][3]);
```

### test_fnt2.c — 17 assertions across 9 tile types
Test categories:
1. Blank tile ($00): all pixels must be 0
2. Horizontal line tile: middle rows should have all pixels set
3. Diamond/outline tile: pixels at center of top/bottom rows
4. Solid fill tile (1bpp alternating-bit pattern)
5. Solid fill tile (2bpp color value 2 = `0xAA` pattern)
6. Diagonal stripe tile
7. 2bpp border character with `0x55` background fill
8. Ring/hollow shape tile
9. Multi-char bitmap strip rendering
Plus: full 1bpp and 2bpp character sheets (128×64 BMP files).

### Build and test
```
gcc -Wall -Wextra -std=c11 -o test_fnt2 test_fnt2.c fnt2.c fnt2_bitmap.c font_bitmap.c
./test_fnt2
```
All 17 tests passed.

### BMP header verification
```python
import struct
for name in ['fnt2_demo.bmp', 'fnt2_sheet_1bpp.bmp', 'fnt2_sheet_2bpp.bmp']:
    # Check: signature='BM', file_size==actual_size, pixel_offset==54, bpp==24
```
All three BMPs had valid headers. Sizes:
- `fnt2_demo.bmp` — 48×8 strip (6 chars)
- `fnt2_sheet_1bpp.bmp` — 128×64 (16 cols × 8 rows)
- `fnt2_sheet_2bpp.bmp` — 128×64

### Files created
- `font_bitmap.h`, `font_bitmap.c` — generic engine
- `fnt2_bitmap.h`, `fnt2_bitmap.c` — fnt2 wrappers
- `test_fnt2.c` — 17-assertion test

---

## Session 4 — fort1.s: Game Logic Conversion

### User prompt
"convert fort1.s to c"

### Source files read
Before writing any C, I read:
- `fort1.s` (1232 lines) — the target file
- `fort7.s` — variable declarations
- `fort.s` — hardware equates and memory map (multiple grep passes)
- `fort2.s`, `fort3.s`, `fort4.s`, `fort5.s`, `fort6.s`, `fort8.s` — for external function labels

### fort1.s scope
`fort1.s` contains:
- `START` / `fort1_start` — program entry point
- `SET.FONTS` — copy fnt1/fnt2 data into character sets
- `TITLE` / `T1` / `T2` — title screen: drawing, VBlank handler, demo timer
- `T3` — game color setup and VBlank reinstall
- `MAIN` — main game loop
- `CHECK.LEVEL` / `DO.LEVEL.1`–`DO.LEVEL.3` — level completion checks
- `MOVE.RAMP` — landing pad animation
- `UNPACK` / `GET.BYTE` — RLE decompressor
- `CHECK.MODES` — mode state machine dispatcher
- `M.START` — new game initialization
- `M.NEW.PLAYER` — new pilot setup
- `M.NEW.LEVEL` — level load: unpack map + scanner, place slaves
- `MAKE.CONTURE` — randomize map tile variants, copy rows
- `S.BEGIN` — place 8 slaves on the map
- `M.GAME.OVER` — tally score, display rank, return to title
- `INC.GAME.POINTS` — accumulate rank points
- `CHOP.LEFT` — BCD lives counter

### Key translation decisions

#### SynAssembler directives
- `.DA #$XX` = define byte (hex)
- `.DA #%xxxxxxxx` = define byte (binary)
- `.AT /text/` = ASCII bytes, verbatim
- `.AT -/text/` = ASCII bytes minus 0x20 (Atari screen codes)
- `.HS hexstring` = hex string, one byte per two hex digits
- `.EQ` = equate (constant definition)
- `.BS n` = block storage (n uninitialized bytes)
- `.IN filename` = include file

#### Atari screen code macro
`.AT -/TEXT/` subtracts 0x20 from each ASCII byte. Defined:
```c
#define S(c)  ((uint8_t)((unsigned char)(c) - 0x20u))
```
Used for all title strings, level names, UI labels encoded with `-`.
Raw `.AT /text/` strings (no `-`) stored as literal ASCII.

#### Hardware register access
All Atari I/O registers accessed through volatile byte pointers:
```c
#define REG(a)   (*(volatile uint8_t *)(uintptr_t)(a))
#define TRIG0    REG(0xD010)   /* joystick trigger: 0=pressed, 1=not pressed */
#define CONSOL   REG(0xD01F)   /* console keys: active low */
#define RANDOM_HW REG(0xD20A)  /* POKEY random number generator */
#define FRAME    REG(0x0014)   /* OS frame counter */
/* etc. */
```

#### 16-bit zero-page pointers
ADR1 and ADR2 are 16-bit zero-page pointers stored as two separate bytes in the Atari (lo/hi). In C, declared as pairs of extern uint8_t:
```c
extern uint8_t adr1_lo, adr1_hi;
extern uint8_t adr2_lo, adr2_hi;
```
Reconstructed as C pointers when needed:
```c
uint8_t *p = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
```

#### Interrupt vector installation
The assembly writes two bytes to the OS vector table to install VBlank/DLI handlers:
```asm
LDA #LOW(handler)
STA $0224
LDA #HIGH(handler)
STA $0225
```
In C:
```c
static inline void set_vector(uint16_t addr, void (*fn)(void)) {
    *(volatile uint8_t *)(uintptr_t)(addr)     = (uint8_t)((uintptr_t)fn & 0xFF);
    *(volatile uint8_t *)(uintptr_t)(addr + 1) = (uint8_t)((uintptr_t)fn >> 8);
}
#define SET_VVBLKD(fn)  set_vector(0x0224, (void(*)(void))(fn))
#define SET_VDSLST(fn)  set_vector(0x0200, (void(*)(void))(fn))
```

#### VVBLKD.RET
The assembly VBlank handlers end with `JMP VVBLKD.RET` (= `JMP $E462`), which is the OS VBlank return entry. In C, handlers just `return` from their function.

#### PROT (self-modifying anti-piracy code)
fort1.s contains several instructions that modify the code itself at runtime:
```asm
ROL CHECK.MODES     ; rotate a byte of CHECK.MODES code
DEC MAIN+32         ; decrement a byte inside MAIN
ASL M.NEW.PLAYER    ; shift a byte of M.NEW.PLAYER
ROR SCREEN.OFF      ; rotate a byte of SCREEN.OFF
STA MAIN            ; store to inside MAIN
INC MAIN            ; increment inside MAIN
```
These are anti-piracy checks that corrupt the game if the code has been patched. All omitted in the C conversion (they operate on machine code bytes, not data). Marked with `/* PROT: omitted */` comments where relevant.

#### BCC/BCS pseudo-ops
The SynAssembler uses `BLT` and `BGE` as aliases:
- `BLT` = `BCC` (Branch if Carry Clear = unsigned less-than after CMP)
- `BGE` = `BCS` (Branch if Carry Set = unsigned greater-or-equal after CMP)

#### INX; BNE loop (256 iterations)
`LDX #0; .loop INX; ...; BNE .loop` runs exactly 256 times as X wraps from 0xFF back to 0.

#### Mode constants (from fort7.s)
```c
#define TITLE_MODE       1
#define GO_MODE          2
#define START_MODE       3
#define NEW_LEVEL_MODE   4
#define NEW_PLAYER_MODE  5
#define GAME_OVER_MODE   6
#define STOP_MODE        7
#define PAUSE_MODE       8
#define OPTION_MODE      9
#define HYPERSPACE_MODE  10
```

#### Entity status constants (from fort7.s)
```c
#define STATUS_OFF      1
#define STATUS_ON       2
#define STATUS_FLY      3
#define STATUS_CRASH    4
#define STATUS_EXPLODE  5
#define STATUS_LAND     6
#define STATUS_BEGIN    7
#define STATUS_FULL     8
#define STATUS_EMPTY    9
#define STATUS_REFUEL   10
#define STATUS_PICKUP   11
```

### Memory map
From fort.s equates:
```c
#define PLAYER_BASE    ((uint8_t *)0x0000u)   /* player-missile graphics */
#define PLAY_SCRN      ((uint8_t *)0x0300u)   /* screen buffer */
#define CHR_SET1       ((uint8_t *)0x0800u)   /* character set 1 */
#define CHR_SET2       ((uint8_t *)0x0C00u)   /* character set 2 */
#define MAP_BASE       ((uint8_t *)0x1103u)   /* MAP = $1100+3 */
#define MAP_END_ADDR   (0x1103u + 0x2800u)    /* = $3903 */
#define SCANNER_BASE   ((uint8_t *)0x39C0u)
#define SCANNER_END_ADDR (0x39C0u + 0x0640u)  /* 1600 bytes = 40×40 */
#define RAM1_STUFF     ((uint8_t *)0x0C90u)   /* CHR_SET2 + 144 */
#define RAM2_STUFF     ((uint8_t *)0x0100u)
#define PACKED_MAP_BASE  ((const uint8_t *)0x8000u)
#define PACKED_SCAN_BASE ((const uint8_t *)(0x8000u + 0x0D34u))
#define WINDOW_1  (CHR_SET2 + 712)
#define WINDOW_2  (CHR_SET2 + 720)
```

### RLE decompressor (UNPACK)
The decompressor uses two codebooks:
- `CHR1` (23 bytes): the set of bytes that trigger run-length encoding in CHR1 mode (temp4==0)
- `CHR2` (4 bytes): `{0x00, 0x55, 0xAA, 0xFF}` — used in scanner data decompression (temp4!=0)

When a byte matches an entry in the active codebook, the next byte from the stream gives the run count. Otherwise count=1.

```c
void unpack(void) {
    uint8_t *end = (uint8_t *)(uintptr_t)((uint16_t)temp3 << 8 | temp2);
    uint8_t *dst = (uint8_t *)(uintptr_t)((uint16_t)adr2_hi << 8 | adr2_lo);
    while (dst < end) {
        uint8_t b = get_byte();
        uint8_t count = 1;
        if (temp4 == 0) {
            for (uint8_t y = 0; y < CHR1_LEN; y++)
                if (b == chr1[y]) { count = get_byte(); break; }
        } else {
            for (uint8_t y = 0; y < CHR2_LEN; y++)
                if (b == chr2[y]) { count = get_byte(); break; }
        }
        for (uint8_t x = count; x != 0; x--) *dst++ = b;
    }
    adr2_lo = (uint8_t)((uintptr_t)dst & 0xFF);
    adr2_hi = (uint8_t)((uintptr_t)dst >> 8);
}
```

### CHR1 table construction
The assembly:
```
CHR1     .AT " a./01*+,-#'?st"   ; 15 raw ASCII bytes
         .HS 41444858595AD8       ; 7 hex bytes
         .DA #$47+128             ; 1 byte: $C7
```
Total: 15 + 7 + 1 = 23 bytes. The `.AT` uses no `-` flag so bytes are raw ASCII. The `.HS` block starts with `0x41` which is also 'A', but in the context of the CHR1 table these are treated as literal byte values for the RLE codebook, not as screen characters.

### MAKE.CONTURE tile replacement
For each byte in the 10240-byte map ($1103–$3902):
- If byte == `$73` ('s'): replace with random value in `$62`.`$64` (3 variants of that tile)
- If byte == `$74` ('t'): replace with random value in `$65`.`$67` (3 variants)

The random selection retries until non-zero: `do { r = RANDOM_HW & 3; } while (r == 0)`, giving r = 1, 2, or 3.

After tile randomization, MAKE.CONTURE copies 40 rows of 40 bytes each from MAP_BASE to MAP_BASE+215. This wraps the top map rows to the bottom for seamless vertical scrolling.

The assembly uses page-based addressing (INC ADR1+1 / INC ADR2+1 to step through 256-byte pages). In C, `src += 256` and `dst += 256` achieve the same result because the flat C array represents the same contiguous Atari memory region.

### S.BEGIN slave placement
Scans the entire 10240-byte map looking for valid slave spawn points. A spawn point requires:
- Two adjacent cells both containing `$48` at (ADR1+Y) and (ADR1+Y+1)
- The cell directly above (ADR1-256+Y) containing `$1F`
- A random check: `10 <= RANDOM_HW < 50` (about 39% acceptance)

If valid: places slave at that position (X = Y+5, Y = page_index), marks the cell with byte `1`, sets slave_status=ON, slave_dx=0x10.

The outer loop rescans the full map until all 8 slaves are placed. The assembly: `TXA; BPL .9` — if slot index X is still >= 0, restart scan from MAP beginning.

### Skill/difficulty tables
```c
static const uint8_t grav_tab[2]     = { 0x0F, 0x07 };   /* gravity skill: EASY/HARD */
static const uint8_t robot_tab[3]    = { 3, 1, 0 };
static const uint8_t chop_tab[3]     = { 0x07, 0x09, 0x11 }; /* starting lives: 7/9/17 */
static const uint8_t laser_tab[3]    = { 4, 8, 16 };
static const uint8_t pod_tab[3]      = { 12, 25, 38 };    /* 38 = MAX_PODS-1 */
static const uint8_t missile_tab[3]  = { 3, 2, 1 };
static const uint8_t elevator_tab[3] = { 62, 47, 37 };
static const int8_t  m_tab[3]        = { -2, -1, 0 };     /* game_points adjustment */
```

### Level data
Three levels (0 = Vaults of Draconis, 1 = Crystalline Caves, 2 = Fort Apocalypse):
```c
static const uint8_t level_color[3]         = { 0x42, 0xC2, 0x42 };
static const uint8_t level_chop_start[3][2] = { {90,100}, {119,100}, {116,170} };
static const uint8_t level_start[3][2]      = { {0x02,0xFF}, {0x6D,0xFF}, {0x6E,0x18} };

/* Packed map addresses: levels 0 and 2 share the same map */
static const uint8_t *const pack_adr[3] = {
    PACKED_MAP_BASE + 0x000,
    PACKED_MAP_BASE + 0x62B,
    PACKED_MAP_BASE + 0x000,
};
/* Packed scanner addresses */
static const uint8_t *const scan_info[3] = {
    PACKED_SCAN_BASE + 0,
    PACKED_SCAN_BASE + 0x1E9,
    PACKED_SCAN_BASE + 0,
};
```

### Tank starting positions (by level)
```c
static const uint8_t tank_start_x_l1[MAX_TANKS] = { 0x53,0x63,0x90,0xA0,0x59,0xAE };
static const uint8_t tank_start_y_l1[MAX_TANKS] = { 0x12,0x12,0x12,0x12,0x26,0x26 };
static const uint8_t tank_start_x_l2[MAX_TANKS] = { 0x50,0x65,0xA0,0xB5,0x3D,0x54 };
static const uint8_t tank_start_y_l2[MAX_TANKS] = { 0x0C,0x0C,0x0C,0x0C,0x26,0x26 };
```
In `m_new_level`: `if (level == 1) use L1 else use L2`. This is direct from the assembly `CMP #1; BEQ .1` where `.1` uses L1.

### M.GAME.OVER rank calculation
game_points is accumulated from:
- `slaves_saved >> 2` (1 point per 4 slaves rescued)
- +3 if fort destroyed
- +1 if completed level 3
- +1 baseline
- +score3 (hundreds digit of score)
- `m_tab[grav_skill]` (gravity skill penalty: -2, -1, 0)
- `chops - 2` (lives selection penalty: -2, -1, 0)
- `m_tab[pilot_skill]` (pilot skill penalty: -2, -1, 0)

Clamped to 0–15. The rank is derived as:
```c
uint8_t idx = (game_points >> 2) & 3;  /* 0=SPARROW, 1=CONDOR, 2=HAWK, 3=EAGLE */
```

### SET.FONTS function
Copies fnt1/fnt2 data into CHR_SET1/CHR_SET2 with specific offsets:
```c
static void set_fonts(void) {
    for (int x = 0; x < 256; x++) {
        CHR_SET1[15 + x]        = fnt1_data[x];
        CHR_SET1[0x100 + x]     = fnt1_data[0x100 - 15 + x];
        CHR_SET1[0x200 + x]     = fnt1_data[0x200 - 15 + x];

        CHR_SET2[0x100 + 8 + x] = fnt2_data[x];
        CHR_SET2[0x200 + x]     = fnt2_data[0x100 - 8 + x];
        CHR_SET2[0x300 + x]     = fnt2_data[0x200 - 8 + x];
    }
}
```
The offsets (15 for CHR_SET1, 8 for CHR_SET2) come from the assembly `ADC` adjustments in the copy loops.

### MOVE.RAMP
Scrolls 5 rows of map data upward at the current ADR1 position. Used for the landing pad animation in level 0.

Assembly: 5 outer iterations (X=4..0), each copying 6 bytes from (ADR1-256) to ADR1, then ADR1 moves up one page (DEC ADR1+1).

```c
void move_ramp(void) {
    uint8_t *dst = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    for (int x = 4; x >= 0; x--) {
        uint8_t *src = dst - 256;
        for (int y = 5; y >= 0; y--)
            dst[y] = src[y];
        dst -= 256;
    }
    adr1_hi -= 5;
}
```
Note: the caller (do_level_1) restores adr1 from temp3/temp4 after each MOVE.RAMP call, so the hi-byte modification doesn't matter.

### MAIN loop structure
```c
void main_loop(void) {
    for (;;) {
        if (mode == GO_MODE) {
            move_pods(); move_tanks(); move_cruise_missiles();
            move_slaves(); set_scanner(); check_fuel_base();
            check_fort(); check_level();
        }
        check_hyper_chamber();
        check_modes();
        read_user();

        /* demo_status == -1 (0xFF): delay by one frame, then start game */
        if ((int8_t)demo_status < 0) {
            demo_status++;       /* -1 → 0 */
            mode = START_MODE;
        }

        /* In title/option mode: count down tim6_val, restart title on expiry */
        if (mode == TITLE_MODE || mode == OPTION_MODE) {
            if ((FRAME & 0x04) != 0) {
                if (--tim6_val == 0) title();
            }
        }
    }
}
```

### Files created in session 4
- `dev/src/fort1.h` — mode constants, status constants, function declarations
- `dev/src/fort1.c` — full conversion (~1210 lines)

---

## Session 5 — fort1.c Bug Review and Fixes

### Context
Session resumed after a context-limit compaction. The prior session had completed fort1.c but noted one confirmed bug. This session re-read the full file and traced the assembly for every section I was uncertain about.

### Methodology
For each suspicious section: (1) transcribe the assembly, (2) trace it manually for all relevant input values, (3) compare against the C code, (4) verify the fix by re-tracing.

---

### Bug 1: SCANNER_END_ADDR — wrong hex literal

**Location:** `#define SCANNER_END_ADDR`

**Assembly source:** `fort.s`: `SCANNER .EQ $39C0 $640 R`

**Analysis:** `$640` is a hex value. `$640` hex = 1600 decimal. The scanner is 40 columns × 40 rows = 1600 bytes. Scanner ends at `$39C0 + $640 = $4000`.

**Bug:** I had written `0x39C0u + 0x1600u`. Here `0x1600` is the hex representation of 5632, not 1600. I confused the decimal value (1600) with a hex literal and wrote `0x1600` — which is `0x640 × 4 = 4 × the correct value`.

This would cause UNPACK (when decompressing scanner data) to write 5632 bytes starting at $39C0, ending at $4FC0 — overwriting the display list area, sprite data, and OS vectors.

**Fix:** `#define SCANNER_END_ADDR (0x39C0u + 0x0640u)`

---

### Bug 2: t2_vblank — demo path sets mode prematurely

**Location:** `t2_vblank()`, `if (temp1_i == 0xF3)` branch

**Assembly trace:**
```
         LDA TEMP1.I
         CMP #$F3
         BEQ .4          ; demo timeout → go to .4
         [button check code...]
         JMP VVBLKD.RET  ; return from VBlank
.3       STX MODE        ; (button press path) mode = X
         ...
.5       STX DEMO.STATUS ; demo_status = X
         JMP T3
.4       LDX #-1         ; X = 0xFF
         BNE .5          ; always branch (0xFF != 0)
```

The `.4` path goes to `.5` with X=-1. At `.5`: `STX DEMO.STATUS; JMP T3`.
MODE is **never touched** in this path. Mode remains TITLE_MODE.

T3 installs VERTBLKD and calls MAIN. In MAIN on the next frame:
```
LDA DEMO.STATUS
BPL .3          ; skip if >= 0
INC DEMO.STATUS ; -1 → 0
LDA #START.MODE
STA MODE        ; NOW mode gets set, one frame later
```

**Bug in C:**
```c
if (temp1_i == 0xF3) {
    demo_status = (uint8_t)-1;
    mode = START_MODE;  // WRONG: mode shouldn't be set here
    return;             // WRONG: must call t3_game_start()
}
```

Two problems:
1. Setting `mode = START_MODE` in the VBlank handler bypasses the intentional one-frame delay before the demo game starts.
2. `return` never calls T3, so VERTBLKD is never installed and MAIN is never entered. The game would loop forever in the title VBlank handler.

**Fix:**
```c
if (temp1_i == 0xF3) {
    demo_status = (uint8_t)-1;   /* MAIN will set mode=START_MODE next frame */
    t3_game_start();             /* install VERTBLKD, enter MAIN — never returns */
    return;
}
```

---

### Bug 3: t2_vblank — TRIG0/CONSOL logic inverted

**Location:** `t2_vblank()`, button-check block

**Atari hardware:**
- `TRIG0 ($D010)`: active low. 0 = joystick trigger pressed. 1 = not pressed.
- `CONSOL ($D01F)`: active low. Bit 0 = START, bit 1 = SELECT, bit 2 = OPTION.
  - `$07` = `0b111` = no console buttons pressed
  - `$06` = `0b110` = START pressed (bit 0 cleared)
  - `$05` = `0b101` = SELECT pressed
  - `$03` = `0b011` = OPTION pressed

**Assembly trace:**
```
         LDX #START.MODE          ; X = 3 (preset)
         LDA TRIG0
         BEQ .3                   ; TRIG0 == 0 (pressed) → jump to .3 with X=START_MODE
         ; TRIG0 != 0: trigger not pressed
         LDA CONSOL
         CMP #6
         BEQ .3                   ; CONSOL == 6 (START pressed) → .3 with X=START_MODE
         ; CONSOL != 6
         LDX #OPTION.MODE         ; X = 9
         CMP #7
         BNE .3                   ; CONSOL != 7 (other button) → .3 with X=OPTION_MODE
         ; CONSOL == 7: no buttons
         JMP VVBLKD.RET           ; nothing pressed → return
.3       STX MODE
         ...
```

Four cases:
| Condition | Mode | Action |
|---|---|---|
| TRIG0 == 0 (trigger pressed) | START_MODE | start game |
| TRIG0 != 0 && CONSOL == 6 (START button) | START_MODE | start game |
| TRIG0 != 0 && CONSOL != 6 && != 7 (SELECT/OPTION) | OPTION_MODE | go to options |
| TRIG0 != 0 && CONSOL == 7 (nothing) | — | return, do nothing |

**Bug in C:**
```c
uint8_t new_mode = START_MODE;
if (TRIG0 != 0) {            // "trigger not pressed" — correct outer check
    uint8_t con = CONSOL;
    if (con == 6) {
        /* fire → start */   // comment wrong: 6 = START button, not fire trigger
    } else if (con == 7) {
        new_mode = OPTION_MODE;  // WRONG: 7 = no buttons → should return
    } else {
        return;              // WRONG: else = SELECT/OPTION → should be OPTION_MODE
    }
}
```

Cases 3 and 4 were swapped. Pressing SELECT/OPTION would do nothing. Pressing nothing at all would set OPTION_MODE and launch into the options menu.

**Fix:**
```c
uint8_t new_mode = START_MODE;
if (TRIG0 != 0) {               /* trigger NOT pressed */
    uint8_t con = CONSOL;
    if (con != 6) {             /* START button not pressed */
        if (con == 7) return;   /* no console buttons: do nothing */
        new_mode = OPTION_MODE; /* SELECT or OPTION button pressed */
    }
    /* con == 6: START pressed → keep new_mode = START_MODE */
}
mode = new_mode;
opt_num = 0;
demo_status = 1;
t3_game_start();
```

---

### Bug 4: m_new_player — BCD decrement misses 0x00 → 0x99 wrap

**Location:** `m_new_player()`, CHOP.LEFT decrement

**Assembly:**
```
SED              ; enable 6502 BCD mode
LDA CHOP.LEFT
SEC              ; carry set = no borrow into SBC
SBC #1           ; BCD subtraction: A = A - 1
STA CHOP.LEFT
CLD              ; disable BCD mode
;        LDA CHOP.LEFT   ; (commented out — CMP uses A from SBC)
CMP #$99
BNE .1           ; not wrapped: continue
```

In 6502 BCD mode with SEC before SBC, the subtraction is purely decimal:
- `$07` → `$06`
- `$10` → `$09` (units borrows from tens: 0→9, tens decrements)
- `$20` → `$19`
- `$00` → `$99` (wraparound: BCD -1 = 99)

After the result is `$99`, the `CMP #$99; BNE .1` check triggers GAME_OVER_MODE.

**Bug in C:**
```c
if ((chop_left & 0x0F) == 0)
    chop_left = (uint8_t)((chop_left - 0x10) | 0x09);
else
    chop_left--;
```

For `chop_left == 0x00`:
- Units nibble is 0 → takes top branch
- `0x00 - 0x10 = 0xF0` (8-bit unsigned underflow!)
- `0xF0 | 0x09 = 0xF9`
- Result: `0xF9` — **wrong**, should be `0x99`

The formula `(val - 0x10) | 0x09` is correct when the tens digit is non-zero (normal borrow from tens digit). But when both digits are zero, the tens digit cannot be decremented — BCD wraps to `$99`.

**Fix:**
```c
if ((chop_left & 0x0F) == 0) {
    if ((chop_left & 0xF0) == 0)
        chop_left = 0x99;                              /* BCD 00 - 1 = 99 */
    else
        chop_left = (uint8_t)((chop_left - 0x10) | 0x09);
} else {
    chop_left--;
}
```

**Verification of all starting-lives values:**

| Input | Condition | Output | Expected |
|---|---|---|---|
| `0x07` | units=7≠0 | `0x06` | `0x06` ✓ |
| `0x09` | units=9≠0 | `0x08` | `0x08` ✓ |
| `0x10` | units=0, tens=1 | `(0x10-0x10)\|0x09 = 0x09` | `0x09` ✓ |
| `0x11` | units=1≠0 | `0x10` | `0x10` ✓ |
| `0x17` | units=7≠0 | `0x16` | `0x16` ✓ |
| `0x20` | units=0, tens=2 | `(0x20-0x10)\|0x09 = 0x19` | `0x19` ✓ |
| `0x00` | units=0, tens=0 | `0x99` | `0x99` ✓ |

---

### Bug 5: m_new_level — level name string selection inverted

**Location:** `m_new_level()`, level title print

**Assembly:**
```
         LDA #2
         STA TEMP1      ; col = 2
         LDA #8
         STA TEMP2      ; row = 8
         LDY LEVEL
         DEY            ; Y = LEVEL - 1
         BEQ .0         ; if LEVEL == 1: branch to .0 (print LVL.2)
         ; LEVEL != 1:
.5       LDX #LVL.1     ; "VAULTS OF DRACONIS"
         LDY /LVL.1
         JSR PRINT
         JMP .1
.0       ; LEVEL == 1:
         LDX #LVL.2     ; "CRYSTALLINE CAVES"
         LDY /LVL.2
         JSR PRINT
.1       ...
```

Trace:
- LEVEL=0: `DEY` → Y=$FF ≠ 0, BEQ not taken → print LVL.1
- LEVEL=1: `DEY` → Y=0, BEQ taken → print LVL.2
- LEVEL=2: `DEY` → Y=1 ≠ 0, BEQ not taken → print LVL.1

Game design rationale: levels 0 and 2 use the same packed map (`PACKED_MAP_BASE+0x000`). Level 1 uses a different map (`PACKED_MAP_BASE+0x62B`). The level_color array also reflects this: `{0x42, 0xC2, 0x42}` — levels 0 and 2 have the same color, level 1 is different. Level 2 is a deeper run through the same Vaults map, not a new location.

**Bug in C:**
```c
if (level == 0) {
    print_str(2, 8, lvl_2);   // prints CRYSTALLINE CAVES for level 0 — WRONG
} else {
    print_str(2, 8, lvl_1);   // prints VAULTS for levels 1 and 2 — WRONG
}
```

Level 0 would show "CRYSTALLINE CAVES". Level 1 would show "VAULTS OF DRACONIS". Level 2 accidentally showed the correct string (but by wrong logic).

**Fix:**
```c
/* LDY LEVEL; DEY; BEQ .0 — branches to LVL.2 only when LEVEL == 1 */
if (level == 1) {
    print_str(2, 8, lvl_2);    /* CRYSTALLINE CAVES */
} else {
    print_str(2, 8, lvl_1);    /* VAULTS OF DRACONIS (levels 0 and 2) */
}
```

---

### Bug 6: m_game_over — chops penalty formula off by one

**Location:** `m_game_over()`, chops adjustment to game_points

**Assembly:**
```
LDA #2
CLC              ; carry CLEAR (sets up borrow-in for SBC)
SBC CHOPS        ; A = 2 - CHOPS - (1-C) = 2 - CHOPS - 1 = 1 - CHOPS
EOR #-1          ; EOR #$FF = bitwise NOT: ~(1-CHOPS)
JSR INC.GAME.POINTS
```

Computing `~(1 - CHOPS)` in 8-bit arithmetic for each valid CHOPS value:
- CHOPS=0 (7 lives, hardest): `~(1-0)` = `~1` = `0xFE` → add -2 (penalty for most lives chosen)
- CHOPS=1 (9 lives): `~(1-1)` = `~0` = `0xFF` → add -1
- CHOPS=2 (17 lives, easiest): `~(1-2)` = `~0xFF` = `0x00` → add 0

Alternative identity: `~(1-CHOPS)` = `CHOPS - 2` in 8-bit (two's complement).

Verification:
- CHOPS=0: `0 - 2 = 0xFE` ✓
- CHOPS=1: `1 - 2 = 0xFF` ✓
- CHOPS=2: `2 - 2 = 0x00` ✓

**Bug in C:**
```c
inc_game_points((uint8_t)(int8_t)(2 - (int8_t)chops) ^ 0xFF);
```

This computes `(2 - chops) ^ 0xFF`:
- CHOPS=0: `(2-0) ^ 0xFF = 2 ^ 0xFF = 0xFD` — wrong, should be `0xFE`
- CHOPS=1: `(2-1) ^ 0xFF = 1 ^ 0xFF = 0xFE` — wrong, should be `0xFF`
- CHOPS=2: `(2-2) ^ 0xFF = 0 ^ 0xFF = 0xFF` — wrong, should be `0x00`

Every value was wrong. The error: I used `(2 - chops)` before XOR when I should have used `(1 - chops)`, because `CLC; SBC` subtracts one extra (the borrow).

**Fix:**
```c
/* CLC; A=2; SBC CHOPS = 1-CHOPS; EOR #$FF = ~(1-CHOPS) = chops-2 */
inc_game_points((uint8_t)(chops - 2));
```

---

### Bug 7: s_begin — outer rescan loop exits after one pass

**Location:** `s_begin()`, outer `for(;;)` loop

**Assembly:**
```
LDX #7
.9   LDA #MAP         ; reset ADR1 to map start
     STA ADR1
     LDA /MAP
     STA ADR1+1
     LDY #0
.10  [scan byte at ADR1+Y for spawn pattern]
     [if found and random OK: place slave, DEX, BMI .12]
.11  INY
     BNE .10          ; Y wraps to 0 → exit inner loop
     INC ADR1+1       ; advance to next page
     LDA ADR1+1
     CMP /MAP+$2800   ; reached end of map?
     BNE .10          ; no: keep scanning
     ; full map scanned:
     TXA
     BPL .9           ; X >= 0 (more slots): RESCAN from beginning
.12  LDA #NEW.PLAYER.MODE
     STA MODE
     RTS
```

The `TXA; BPL .9` is the key: after scanning the full map, if X (slot index) is still >= 0, RESTART the scan. This is necessary because the random check (10 ≤ r < 50) rejects ~61% of valid spawn points. A second (or third) pass may find the remaining slots.

**Bug in C:**
```c
for (;;) {
    uint8_t *mp = MAP_BASE;
    for (;;) {
        for (int y = 0; y < 256; y++) {
            // scan...
            if (--slot < 0) goto done;  // all placed: exit
        }
        mp += 256;
        if (mp >= (uint8_t *)MAP_END_ADDR) break;  // end of map
    }
    if (slot >= 0) break;   // BUG: this is ALWAYS true when reached
}
done:
```

The `goto done` fires when `slot < 0` (all 8 slaves placed). The `if (slot >= 0) break` is therefore only reachable when a full-map scan completed with `slot >= 0` (unplaced slaves remaining). Since `slot < 0` always exits via `goto done`, the `slot >= 0` condition is ALWAYS true when the code reaches `if (slot >= 0) break`. This means the outer loop ALWAYS exits after exactly one full scan pass — even if not all slaves were placed.

The assembly would loop indefinitely (effectively matching `for(;;) { scan(); }`). The C code mistakenly had the break condition inverted: it should loop on `slot >= 0`, not break on it.

**Fix:** Remove the `if (slot >= 0) break` line entirely.
```c
for (;;) {
    uint8_t *mp = MAP_BASE;
    for (;;) {
        for (int y = 0; y < 256; y++) {
            // scan...
            if (--slot < 0) goto done;
        }
        mp += 256;
        if (mp >= (uint8_t *)MAP_END_ADDR) break;
    }
    /* rescan from beginning — matches TXA; BPL .9 in assembly */
    /* slot < 0 is already handled by goto done above */
}
done:
```

---

### Sections verified as correct

The following sections were re-traced against the assembly and found correct in the C code.

**MOVE.RAMP:** `dst -= 256` and `src = dst - 256` correctly model the `DEY on ADR2+1` pattern. Five-iteration loop with 6-byte copy per iteration is accurate. Caller restores ADR1 from temp3/temp4 so the hi-byte modification is harmless.

**MAKE.CONTURE tile loop:** `do { r = RANDOM_HW & 3; } while (r == 0)` correctly models the retry-on-zero pattern. `b = (uint8_t)(r + 0x62 - 1)` gives the right range $62–$64 for 's', and `r + 0x65 - 1` gives $65–$67 for 't'.

**MAKE.CONTURE row copy:** `src += 256` / `dst += 256` correctly steps through the 256-byte pages. The initial `dst = MAP_BASE + 215` matches the assembly `LDA #MAP+255-40` (215 = 255-40). The flat C array model handles page-crossing correctly because the Atari map occupies a contiguous $2800-byte region.

**M.GAME.OVER rank display:** `rank_num = ((game_points & 3) ^ 3) + 1` gives range 1–4. `row[12] = rank_num | 0x90` (bits 7+4 set), `row[13] = row[12] & 0x8F` (bit 4 cleared) matches the assembly `ORA #$90` / `AND #$8F` pair.

**M.NEW.LEVEL scanner copy:** 40 rows × 13 bytes (Y=12..0, BPL loop), both pointers advancing by 40 bytes per row, destination offset by $1B bytes. Correct.

**M.NEW.PLAYER refuel check:** `if (fuel_status == STATUS_EMPTY) { fuel_status = STATUS_FULL; fuel1 = 0; fuel2 = 1; }` matches assembly.

**M.NEW.LEVEL tank table selection:** `if (level == 1) use L1 else use L2` matches `CMP #1; BEQ .1` in assembly.

**M.GAME.OVER tim6_val:** Assembly after rating print: `LDA #-1; STA TIM6.VAL; LDA #TITLE.MODE (=1); STA MODE; STA DEMO.STATUS`. My C: `tim6_val = (uint8_t)-1; demo_status = 1; mode = TITLE_MODE;`. Correct — both MODE and DEMO.STATUS get value 1 = TITLE_MODE.

---

## Summary of all files created or modified

| File | Session | Status |
|---|---|---|
| `dev/src/fnt1.h` | 1 | Created |
| `dev/src/fnt1.c` | 1 | Created |
| `dev/src/fnt1_bitmap.h` | 2 | Created |
| `dev/src/fnt1_bitmap.c` | 2 | Created |
| `fnt2.h` | 3 | Created |
| `fnt2.c` | 3 | Created |
| `font_bitmap.h` | 3 | Created |
| `font_bitmap.c` | 3 | Created |
| `fnt2_bitmap.h` | 3 | Created |
| `fnt2_bitmap.c` | 3 | Created |
| `test_fnt2.c` | 3 | Created (17 tests pass) |
| `dev/src/fort1.h` | 4 | Created |
| `dev/src/fort1.c` | 4+5 | Created then 7 bugs fixed |

---

## Next conversions pending

Per `fort.s` `.IN` include order after `fort1.s`:
1. `fort2.s` — MOVE.PODS, MOVE.TANKS, MOVE.CRUISE.MISSILES, SCREEN.ON/OFF, PRINT, DDIG, WAIT.FRAME, CLEAR.INFO, GIVE.BONUS
2. `fort3.s` — VERTBLKD (normal VBlank), SAVE.POS, UPDATE.CHOPPER, DO.CHECKSUM2
3. `fort4.s` — HOVER, COMPUTE.MAP.ADR, DDIG, DRAW.MAP, joystick read
4. `fort5.s` — MOVE.SLAVES, SET.SCANNER, CHECK.FUEL.BASE, CHECK.FORT, DO.SOUNDS, LINE1 (DLI handler)
5. `fort6.s` — display lists (DSP.LST2/DSP.LST3), chopper shapes, laser shapes, CART.START, INIT.OS
6. `fort7.s` — variable declarations (already read; minimal C needed — mostly extern declarations)
7. `fort8.s` — Z1/Z2 display list init blocks
