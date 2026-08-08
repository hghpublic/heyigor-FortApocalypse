# Session Log: convert fnt2.s — Part 1: Analysis

## Task
Convert `fnt2.s` (Fort Apocalypse graphics / tile-set font) to C.
Output files: `dev/src/fnt2.h` and `dev/src/fnt2.c`.
Also created a generic bitmap engine: `dev/src/font_bitmap.h` and `dev/src/font_bitmap.c`
with thin wrappers: `dev/src/fnt2_bitmap.h` and `dev/src/fnt2_bitmap.c`.
Source: 752 lines of Atari 400/800 SynAssembler — pure data, no executable code.

---

## fnt2.s — Source File Overview

Like fnt1.s, this file is **a pure data file**: 128 characters × 8 bytes = 1024 bytes total.
No 6502 instructions. A file-level label `FNT2.S` appears at the top as a comment-header.

**Key difference from fnt1.s:** There are **no named labels** embedded in the data.
The array is fully anonymous; no character has a symbolic alias used elsewhere.

**SynAssembler directives:**
- `* CHR $xx` — comment line before each character (asterisk = comment in SynAssembler)
- `.DA #%XXXXXXXX` — single byte from binary literal
- `* CHR $00-$1F BLANK` and `* CHR $73-$7F BLANK` — comment-only blank ranges (no `.DA` entries)

---

## Character Range Breakdown

| Range | Count | Bytes | Content type |
|---|---|---|---|
| $00–$1F | 32 | 256 | Blank (comment only — all zero) |
| $20 | 1 | 8 | Misc graphics tile (mushroom/hat shape) |
| $21–$3A | 26 | 208 | 2BPP text characters with 0x55 border frames |
| $3B–$3E | 4 | 32 | Small arrow/marker tiles |
| $3F | 1 | 8 | Horizontal line (row 6 = 0xFF, rest zero) |
| $40 | 1 | 8 | Heart/diamond symbol |
| $41–$46 | 6 | 48 | Horizontal fill gradient tiles (0xAA pattern) |
| $47 | 1 | 8 | Diagonal stripe (top-left → bottom-right) |
| $48–$4A | 3 | 24 | Partial tiles (few active pixels) |
| $4B | 1 | 8 | Left half-block (0x0F rows) |
| $4C–$4F | 4 | 32 | Fort wall corner/curve tiles |
| $50–$51 | 2 | 16 | Fort entrance top/bottom |
| $52–$53 | 2 | 16 | Rounded junction tiles |
| $54–$56 | 3 | 24 | Directional arrow tiles |
| $57 | 1 | 8 | Horizontal 2BPP line `{0,0,0,0xAA,0xAA,0,0,0}` |
| $58 | 1 | 8 | Ring/hollow circle |
| $59–$5A | 2 | 16 | Symbol tiles |
| $5B–$5F | 5 | 40 | Tiny marker/dot tiles |
| $60 | 1 | 8 | Diamond/gem shape |
| $61–$67 | 7 | 56 | Partial 0x55 ramp tiles (terrain edge gradients) |
| $68–$6B | 4 | 32 | Small indicator tiles |
| $6C–$6E | 3 | 24 | Explosion/blast visual tiles |
| $6F–$70 | 2 | 16 | Curved wall tiles |
| $71–$72 | 2 | 16 | Double-pixel indicator tiles |
| $73–$7F | 13 | 104 | Blank (comment only — all zero) |

**Total: 128 × 8 = 1024 bytes.**

---

## Key Tile Families

### 0x55 Border Pattern (CHR $21–$3A)
```
0x55 = %01010101 → in 2BPP: four 2-bit values of 01 01 01 01
                 → each doubled to 8 pixels, all colour 1 (border/frame colour)
```
Every character in this range starts and ends with a `0x55` row. This creates a
uniform 1-pixel colour-1 border around each game character tile, consistent with
Atari ANTIC mode 4 visual conventions. Interior rows encode letter/symbol shapes
using `0x65`, `0x99`, `0xA5`, `0x69`, `0xA9` etc. (combinations of 2-bit pixel values).

### 0xAA Fill Gradient (CHR $41–$46)
```
0xAA = %10101010 → in 2BPP: four 2-bit values of 10 10 10 10
                 → each doubled to 8 pixels, all colour 2 (cave rock colour)
```
These are **shading tiles** used for the cave ceiling and floor rendering. Each tile
in this group has a different number of 0xAA rows vs zero rows, building a gradient:

| CHR | Bytes | Description |
|---|---|---|
| $41 | 8× 0xAA | Full solid fill |
| $42 | 0,0,0xAA×6 | Top 2 rows blank, then full fill |
| $43 | 0×4, 0xAA×4 | Top half blank, bottom half solid |
| $44 | 0×6, 0xAA×2 | Only bottom 2 rows solid |
| $45 | 8× 0xAA | Full solid fill (same as $41) |
| $46 | 0xAA×4, 0×4 | Top half solid, bottom half blank |

Together these six tiles let the renderer create smooth transitions between
open air and cave rock by varying the tile index at each depth level.

### Fort Wall Corner Tiles (CHR $4C–$4F)
These use mixed byte values with progressive diagonal transitions:
- $4C: NW corner (top-right fill expanding outward from top-left)
- $4D: NE corner
- $4E: SW corner (mirror of $4C, rows reversed)
- $4F: SE corner (mirror of $4D, rows reversed)

Each tile transitions from a full-width fill row (`0xAA...`) at one end to a
narrow fill at the other, creating the rounded fort walls visible during gameplay.

### Explosion/Blast Tiles (CHR $6C–$6E)
```
$6C: {0x00, 0xC0, 0xF0, 0xFF, 0x55, 0x22, 0x2A, 0x08}
$6D: {0x3C, 0xFF, 0xFF, 0xFF, 0x55, 0x22, 0xAA, 0x88}
$6E: {0x00, 0x00, 0x00, 0xF0, 0x55, 0x22, 0xAA, 0x88}
```
These combine dense fill at top with sparse scattered bits below — visual burst shape.

### Terrain Ramp Tiles (CHR $61–$67)
```
$61: all 8 rows = 0x55   (solid 1/4-density fill)
$62: 0×1, sparse×2, 0x55×5    (fade-in from top)
$63: 0×4, sparse×2, 0x55×2    (deeper fade-in)
$64: 0×6, sparse×1, 0x55×1    (trace only)
$65: mirror of $62 (fade-out from bottom)
$66: mirror of $63
$67: mirror of $64
```
Used at terrain boundary rows to smooth the visual transition from open air
to the 0x55-density cave rock surface.

---

## Byte Pattern Quick-Reference

| Pattern | Binary | 2BPP meaning |
|---|---|---|
| `0x55` | `01 01 01 01` | 4 doubled pixels, all colour 1 (border) |
| `0xAA` | `10 10 10 10` | 4 doubled pixels, all colour 2 (fill/rock) |
| `0xFF` | `11 11 11 11` | 4 doubled pixels, all colour 3 |
| `0x00` | `00 00 00 00` | 4 doubled pixels, all colour 0 (background) |
| `0x99` | `10 01 10 01` | alternating colour 2 / colour 1 pairs |
| `0xA5` | `10 10 01 01` | two colour-2 then two colour-1 |

---

## font_bitmap.c — Rationale for Generic Module

When building the bitmap system for fnt2, the existing `fnt1_bitmap.c` was
examined as a reference. Rather than creating a parallel `fnt2_bitmap.c` with
duplicate decode and BMP logic, the decode and writer code was extracted into
`font_bitmap.c`, parameterized by `const uint8_t *font_data`:

```c
void font_decode_char(const uint8_t *font_data, uint8_t chr, FontMode mode, uint8_t out[64]);
uint8_t *font_create_bitmap(const uint8_t *font_data, ...);
uint8_t *font_create_sheet(const uint8_t *font_data, int n_chars, int cols, ...);
int font_write_bmp(const char *filename, ...);
```

The `fnt2_bitmap.c` wrappers then become three one-liners that pass `fnt2_data`
as the first argument. `fnt1_bitmap.c` could be refactored the same way in a
later session.

**Additional function added:** `font_create_sheet()` — renders all n_chars characters
in a grid of `cols` columns. Not in `fnt1_bitmap.h`. Enables the full-128-character
character sheet BMP output in one call.

---

## File Summary (Output)

| File | Lines | Content |
|---|---|---|
| `dev/src/fnt2.h` | 13 | Minimal header: `fnt2_data[1024]` extern only |
| `dev/src/fnt2.c` | ~140 | Full 1024-byte data array |
| `dev/src/font_bitmap.h` | 64 | `FontMode` enum + 4 generic functions |
| `dev/src/font_bitmap.c` | 146 | 1BPP/2BPP decoders + BMP writer |
| `dev/src/fnt2_bitmap.h` | 28 | 3 fnt2-specific function declarations |
| `dev/src/fnt2_bitmap.c` | 19 | 3 thin wrappers over `font_bitmap` |
| `dev/src/test_fnt2.c` | 236 | 11 test functions, 17 assertions |
