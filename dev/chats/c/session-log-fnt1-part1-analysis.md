# Session Log: convert fnt1.s — Part 1: Analysis

## Task
Convert `fnt1.s` (Fort Apocalypse game font / character set data) to C.
Output files: `dev/src/fnt1.h` and `dev/src/fnt1.c`.
Also created visualization helpers: `dev/src/fnt1_bitmap.h` and `dev/src/fnt1_bitmap.c`.
Source: 818 lines of Atari 400/800 SynAssembler — pure data, no executable code.

---

## fnt1.s — Source File Overview

This file is **a pure data file**: 128 characters × 8 bytes = 1024 bytes total.
It contains no 6502 instructions. The label `FNT1` marks the start of the array,
and 7 named labels within the data provide symbolic access to special characters
used by the game at known offsets.

**SynAssembler directives used:**
- `.DA #%XXXXXXXX` — define-address (single byte from binary literal)
- `.AT -/TEXT/` — ASCII text with `-` flag; subtracts 0x20 from each character
   (converts ASCII → Atari internal screen codes)
- Comments only (no `.DA`) for blank characters

---

## Character Range Breakdown

| Range | Count | Mode | Content |
|---|---|---|---|
| $00 | 1 | 1BPP | Blank (implicit — no `.DA` bytes, comment only) |
| $01–$09 | 9 | 1BPP | Special game tiles (some partially defined) |
| $0A | 1 | 1BPP | Unused (all zeros) |
| $0B | 1 | 1BPP | `POS.MASK1` — scanner XOR bitmask |
| $0C | 1 | 1BPP | `FORT.EX1` — explosion frame 1 (dot) |
| $0D | 1 | 1BPP | `FORT.EX2` — explosion frame 2 (ring) |
| $0E | 1 | 1BPP | `FORT.EX3` — explosion frame 3 (ring+cross) |
| $0F | 1 | 1BPP | `FORT.EX4` — explosion frame 4 (all zeros) |
| $10–$19 | 10 | 2BPP | Digits 0–9 |
| $1A–$1F | 6 | 1BPP | Misc symbols |
| $20 | 1 | — | `T.5` — raw ASCII keyboard labels for 5 keys |
| $21–$3A | 26 | 2BPP | Punctuation and game tile characters |
| $3B | 1 | 1BPP | — |
| $3C | 1 | 1BPP | `EXP.SHAPE` — explosion mask for do_exp() |
| $3D–$40 | 4 | 1BPP | Misc tiles |
| $41–$5A | 26 | 2BPP | A–Z uppercase letters |
| $5B–$7F | 37 | — | Blank (comment only, no `.DA` bytes) |

**Total bytes:**
- Characters $00–$5A: 91 × 8 = 728 bytes
- Characters $5B–$7F: 37 × 8 = 296 bytes
- Grand total: 128 × 8 = **1024 bytes**

---

## Named Assembly Labels

Seven labels are embedded within the data block at specific character positions.
These are referenced by name in other assembly source files for game logic.

| Label | CHR | Offset | Hex bytes | Game use |
|---|---|---|---|---|
| `POS.MASK1` | $0B | 88 | `{0x80,0x80,0x20,0x20,0x08,0x08,0x02,0x02}` | Scanner minimap XOR bitmask |
| `FORT.EX1` | $0C | 96 | `{0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00}` | Explosion frame 1 |
| `FORT.EX2` | $0D | 104 | `{0x00,0x18,0x24,0x24,0x18,0x00,0x00,0x00}` | Explosion frame 2 |
| `FORT.EX3` | $0E | 112 | `{0x18,0x24,0x5A,0x5A,0x24,0x18,0x00,0x00}` | Explosion frame 3 |
| `FORT.EX4` | $0F | 120 | `{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}` | Explosion frame 4 |
| `T.5` | $20 | 256 | `{0x31,0x21,0x39,0x29,0x38,0x28,0x32,0x22}` | Keyboard key labels |
| `EXP.SHAPE` | $3C | 480 | `{0x3C,0x3C,0xFF,0xFF,0xFF,0xFF,0x3C,0x3C}` | Explosion shape mask |

**Note:** The byte offset into `fnt1_data[]` is `CHR × 8`. For example,
`POS.MASK1 = &fnt1_data[0x0B * 8]`.

---

## Assembly Encoding Decisions

### CHR $00 — Implicit Blank
The source has only a comment line: `; CHR $00 BLANK`. No `.DA` directives follow.
This means the 8 bytes are implied all-zero. In C: 8 explicit `0x00` entries.

### CHR $01 — Partial Definition
Seven rows are commented out (shown as `; .DA #%00000000`), and only the last
row has an active byte: `.DA #%11110000` = `0xF0`.
In C: `{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0}`.

### T.5 at CHR $20 — Raw ASCII, Not Screen Codes
The assembly source uses: `.AT -/1!9)8(2"/`

The SynAssembler `.AT` directive normally stores Atari internal screen codes
(ASCII - 0x20). The `-` flag in `.AT -/...` subtracts an additional 0x20 from
each character, converting from Atari screen codes back toward raw ASCII.

Net effect: `.AT -` stores raw ASCII values of the characters listed.
```
'1'=0x31  '!'=0x21  '9'=0x39  ')'=0x29  '8'=0x38  '('=0x28  '2'=0x32  '"'=0x22
```
These are keyboard labels for the number keys on the Atari: each pair
`{0x31,0x21}` = `{'1','!'}`, `{0x39,0x29}` = `{'9',')'}`, etc.

### CHR $5B–$7F — Trailing Blank Block
37 characters with no `.DA` directives — only comment lines.
In C: 37 × 8 = 296 zero bytes, written as explicit `{0x00,...}` entries per
character to preserve clear indexing at a glance.

---

## Display Mode Encoding

**1BPP (ANTIC mode 2):** One bit per pixel, 8 pixels per row byte. MSB = leftmost pixel.
- Used by: explosion frames ($0C–$0F), EXP_SHAPE ($3C), misc special tiles
- Decode: `pixel[col] = (byte >> (7 - col)) & 1`

**2BPP (ANTIC mode 4):** Two bits per pixel, 4 pixels per row byte, hardware-doubled.
- Used by: digits ($10–$19), punctuation ($21–$3A), A–Z ($41–$5A)
- Atari ANTIC mode 4 doubles each pixel horizontally → each 4-pixel row → 8 screen pixels
- Decode: `val = (byte >> (6 - px * 2)) & 0x03; output[px*2] = output[px*2+1] = val`

**POS_MASK1 special case:** The XOR bitmask uses pairs of identical bits:
`{0x80,0x80,0x20,0x20,0x08,0x08,0x02,0x02}`. This is correct for 2BPP scanner mode
where each pixel = 2 horizontal bits.

---

## fnt1_bitmap.c — Visualization Architecture

After converting the raw data, a separate visualization module was added to allow
visual verification of the converted glyphs.

**Rationale:** With 128 characters and 7 special named labels, it was impractical
to verify correctness by inspection alone. A bitmap renderer allows immediate visual
confirmation that:
1. Each character's bytes produce the correct glyph shape
2. The 1BPP vs 2BPP mode selection gives correct rendering for each range
3. The named pointers (POS_MASK1, FORT_EX1–4, etc.) point to the expected glyphs

**Module API:**
```c
typedef enum { FNT1_1BPP, FNT1_2BPP } Fnt1Mode;

void    fnt1_decode_char(uint8_t chr, Fnt1Mode mode, uint8_t out[64]);
uint8_t *fnt1_create_bitmap(const uint8_t *chars, int n_chars, Fnt1Mode mode,
                             int *out_width, int *out_height);
int     fnt1_write_bmp(const char *filename, const uint8_t *pixels,
                       int width, int height, const uint32_t palette[4]);
```

`fnt1_decode_char` fills a 64-byte flat pixel array (8×8, row-major).
`fnt1_create_bitmap` renders a sequence of characters side by side (n × 8 wide, 8 tall).
`fnt1_write_bmp` writes a 24-bit BMP file with an optional 4-colour palette (NULL = defaults).

**BMP format details:**
- Little-endian u16/u32 header helpers (inline functions in the .c file)
- Bottom-up row order (standard BMP convention)
- 24-bit BGR pixel format
- 4-byte row stride padding (rows padded to multiples of 4 bytes)

---

## File Summary (Output)

| File | Lines | Content |
|---|---|---|
| `dev/src/fnt1.h` | 21 | Extern declarations for `fnt1_data[1024]` + 7 named pointers |
| `dev/src/fnt1.c` | 244 | `fnt1_data` initializer + named pointer definitions |
| `dev/src/fnt1_bitmap.h` | 67 | `Fnt1Mode` enum + 3 function declarations |
| `dev/src/fnt1_bitmap.c` | 133 | 1BPP/2BPP decoders + BMP writer |
| `dev/src/fnt1_test.c` | 36 | "FORT" 2BPP render + FORT.EX3 1BPP ASCII-art + BMP write |
