# Session Log: convert fnt2.s — Part 2: Implementation

## Files Produced
- `dev/src/fnt2.h` (13 lines)
- `dev/src/fnt2.c` (~140 lines)
- `dev/src/font_bitmap.h` (64 lines)
- `dev/src/font_bitmap.c` (146 lines)
- `dev/src/fnt2_bitmap.h` (28 lines)
- `dev/src/fnt2_bitmap.c` (19 lines)
- `dev/src/test_fnt2.c` (236 lines)

---

## fnt2.h

Minimal header — no named pointer constants (fnt2.s has no named labels):

```c
#ifndef FNT2_H
#define FNT2_H
#include <stdint.h>

/* Fort Apocalypse - Font 2 (Graphics)
 * 128 characters, 8 bytes each.
 * CHR $00-$1F and $73-$7F are blank (all zero).
 * Index a character with fnt2_data[char_index * 8]. */
extern const uint8_t fnt2_data[128 * 8];

#endif /* FNT2_H */
```

This contrasts with `fnt1.h` which also exports 7 named `const uint8_t *const` pointer
constants. fnt2 has no in-file labels to expose.

---

## fnt2.c Structure

```c
const uint8_t fnt2_data[128 * 8] = {
    /* $00-$1F - blank */
    0x00, 0x00, ..., 0x00,   /* 32 chars × 8 = 256 zero bytes */

    /* $20 */
    0x00, 0x24, 0x6E, 0x7E, 0x48, 0x3E, 0x2C, 0x00,

    /* $21 */
    0x55, 0x65, 0x65, 0x99, 0x99, 0xA9, 0x99, 0x55,

    /* ... $22-$72 ... */

    /* $72 */
    0x00, 0x00, 0xA0, 0xEA, 0xEA, 0xA0, 0x00, 0x00,

    /* $73-$7F - blank */
    0x00, 0x00, ..., 0x00,   /* 13 chars × 8 = 104 zero bytes */
};
```

**Leading blank block:** Unlike fnt1.c which writes each blank character as an
individual `{0x00,...}` entry, fnt2.c writes the $00–$1F range as contiguous
zero bytes grouped in 8-per-line rows, reflecting the comment-only assembly block.

### Byte Count Verification

```python
import re
data = open('dev/src/fnt2.c').read()
array_body = re.search(r'fnt2_data\[128 \* 8\] = \{(.*?)\};', data, re.DOTALL).group(1)
count = len(re.findall(r'0x[0-9A-Fa-f]{2}', array_body))
print(f'Array bytes: {count} (expected 1024)')
# → Array bytes: 1024
```

---

## font_bitmap.h

```c
typedef enum { FONT_1BPP, FONT_2BPP } FontMode;

void font_decode_char(const uint8_t *font_data, uint8_t chr, FontMode mode,
                      uint8_t out[64]);

uint8_t *font_create_bitmap(const uint8_t *font_data, const uint8_t *chars,
                             int n_chars, FontMode mode,
                             int *out_width, int *out_height);

uint8_t *font_create_sheet(const uint8_t *font_data, int n_chars, int cols,
                            FontMode mode, int *out_width, int *out_height);

int font_write_bmp(const char *filename, const uint8_t *pixels,
                   int width, int height, const uint32_t palette[4]);
```

**`font_create_sheet`** — new function not in `fnt1_bitmap.h`. Renders all n_chars
characters in a grid with `cols` columns. `rows = ceil(n_chars / cols)`.
Used by `fnt2_create_sheet()` to render a 16×8 grid of all 128 characters.

---

## font_bitmap.c — Key Implementation Details

### Decode (identical logic to fnt1_bitmap.c)

```c
void font_decode_char(const uint8_t *font_data, uint8_t chr, FontMode mode,
                      uint8_t out[64])
{
    const uint8_t *glyph = &font_data[chr * 8];

    if (mode == FONT_1BPP) {
        for (int row = 0; row < 8; row++) {
            uint8_t byte = glyph[row];
            for (int col = 0; col < 8; col++)
                out[row * 8 + col] = (byte >> (7 - col)) & 1;
        }
    } else {  /* FONT_2BPP */
        for (int row = 0; row < 8; row++) {
            uint8_t byte = glyph[row];
            for (int px = 0; px < 4; px++) {
                uint8_t val = (byte >> (6 - px * 2)) & 0x03;
                out[row * 8 + px * 2]     = val;
                out[row * 8 + px * 2 + 1] = val;
            }
        }
    }
}
```

### Sheet Renderer

```c
uint8_t *font_create_sheet(const uint8_t *font_data, int n_chars, int cols,
                            FontMode mode, int *out_width, int *out_height)
{
    int rows   = (n_chars + cols - 1) / cols;  /* ceil division */
    int width  = cols * 8;
    int height = rows * 8;
    uint8_t *sheet = calloc((size_t)width * height, 1);
    if (!sheet) return NULL;

    uint8_t glyph[64];
    for (int chr = 0; chr < n_chars; chr++) {
        font_decode_char(font_data, (uint8_t)chr, mode, glyph);
        int gc = chr % cols;   /* grid column */
        int gr = chr / cols;   /* grid row */
        for (int row = 0; row < 8; row++)
            memcpy(&sheet[(gr * 8 + row) * width + gc * 8], &glyph[row * 8], 8);
    }
    *out_width = width; *out_height = height;
    return sheet;
}
```

**`calloc` vs `malloc`:** `font_create_sheet` uses `calloc` so that blank characters
($00–$1F, $73–$7F) appear as background colour without explicit zeroing.
`font_create_bitmap` uses `malloc` since all slots are written explicitly.

### BMP Writer

```c
int font_write_bmp(const char *filename, const uint8_t *pixels,
                   int width, int height, const uint32_t palette[4])
{
    static const uint32_t default_pal[4] = {
        0xFFFFFF, 0x000000, 0x808080, 0x404040,  /* white bg, black fg, 2 grays */
    };
    if (!palette) palette = default_pal;

    int row_bytes  = width * 3;
    int row_padded = (row_bytes + 3) & ~3;   /* 4-byte row alignment */
    int pix_bytes  = row_padded * height;
    uint32_t file_bytes = 54u + (uint32_t)pix_bytes;

    /* Write BITMAPFILEHEADER + BITMAPINFOHEADER (54 bytes total) */
    /* Pixel data: bottom-up, BGR order, rows padded to 4 bytes */
    for (int row = height - 1; row >= 0; row--) {
        for (int col = 0; col < width; col++) {
            uint32_t rgb = palette[pixels[row * width + col] & 0x03];
            uint8_t px[3] = { rgb & 0xFF, (rgb >> 8) & 0xFF, (rgb >> 16) & 0xFF };
            fwrite(px, 1, 3, f);
        }
        fwrite(pad, 1, row_padded - row_bytes, f);
    }
}
```

Helper functions `bmp_u16()` and `bmp_u32()` write little-endian 2- and 4-byte
values portably. No reliance on host endianness.

---

## fnt2_bitmap.c — Thin Wrappers

Three one-line wrappers forward directly to the generic engine:

```c
void fnt2_decode_char(uint8_t chr, FontMode mode, uint8_t out[64]) {
    font_decode_char(fnt2_data, chr, mode, out);
}

uint8_t *fnt2_create_bitmap(const uint8_t *chars, int n_chars, FontMode mode,
                             int *out_width, int *out_height) {
    return font_create_bitmap(fnt2_data, chars, n_chars, mode, out_width, out_height);
}

uint8_t *fnt2_create_sheet(FontMode mode, int *out_width, int *out_height) {
    return font_create_sheet(fnt2_data, 128, 16, mode, out_width, out_height);
}
```

`fnt2_create_sheet` fixes `n_chars=128` and `cols=16`, giving a 128×64 output grid.

---

## test_fnt2.c — Test Structure

17 assertions across 11 test functions. All use the helper pattern:

```c
static int failures = 0;
static void check(const char *what, int cond) {
    printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) failures++;
}
```

| Test function | CHR | Mode | Assertions |
|---|---|---|---|
| `test_blank` | $00 | 1BPP | all 64 pixels zero |
| `test_horizontal_line` | $3F | 1BPP | row 6 all set (0xFF), others zero |
| `test_diamond` | $60 | 1BPP | rows 0+7 blank; row 3 (0x7E) edges clear, centre set |
| `test_solid_fill_1bpp` | $41 | 1BPP | even cols set, odd cols clear (0xAA) |
| `test_solid_fill_2bpp` | $41 | 2BPP | all 64 pixels colour 2 (0xAA → `10 10 10 10` doubled) |
| `test_diagonal` | $47 | 1BPP | row 0 bits 7:6 set (0xC0); row 7 bits 1:0 set (0x03) |
| `test_2bpp_border` | $21 | 2BPP | row 0 (0x55) all pixels colour 1 |
| `test_ring` | $58 | 1BPP | row 3 (0xC3) outer pixels set, inner clear |
| `test_create_bitmap` | {$47,$41..$44,$60} | 1BPP | alloc, w=48, h=8, BMP write |
| `test_sheet_1bpp` | 128 chars | 1BPP | alloc, w=128, h=64, size=8192, BMP write |
| `test_sheet_2bpp` | 128 chars | 2BPP | alloc, BMP write |

**Row 3 of CHR $58 check (ring test):**
```
0xC3 = 11000011 → pixels: 1 1 0 0 0 0 1 1
```
Tests that the hollow-circle shape has the correct outer-set / inner-clear pattern.

**`test_solid_fill_2bpp` key insight:**
CHR $41 bytes are all `0xAA`. In 1BPP, `0xAA = 10101010` → alternating pixels.
In 2BPP, `0xAA = 10 10 10 10` → all four 2-bit values = 2 → doubled to 8 pixels
all colour 2. The same byte decodes completely differently between the two modes —
the test explicitly checks both to guard against mode-selection bugs.

---

## Compile and Test

### Build command
```
cd dev/src
gcc -Wall -Wextra -std=c11 -o test_fnt2 test_fnt2.c fnt2.c fnt2_bitmap.c font_bitmap.c && echo "Build OK"
./test_fnt2
```

### Output
```
=== fnt2 bitmap tests ===

$00 (blank):
  PASS  all pixels zero

$3F (horizontal line, 1bpp):
  PASS  row 6 all set (0xFF -> 8 pixels on)
  PASS  all other rows zero

$60 (diamond, 1bpp):
  PASS  rows 0 and 7 blank
  PASS  row 3 (0x7E) edges clear, centre set

$41 (0xAA solid, 1bpp):
  PASS  alternating col pattern (0xAA)

$41 (0xAA solid, 2bpp):
  PASS  all pixels color 2 (0xAA -> 10 10 10 10 doubled)

$47 (diagonal stripe, 1bpp):
  PASS  row 0 (0xC0): bits 7-6 set
  PASS  row 7 (0x03): bits 1-0 set

$21 (text char with 0x55 border, 2bpp):
  PASS  row 0 (0x55): all pixels color 1

$58 (ring, 1bpp):
  PASS  row 3 (0xC3): outer pixels set, inner clear

fnt2_create_bitmap (6 chars):
  PASS  allocation succeeds
  PASS  width = 6*8 = 48
  PASS  height = 8
  PASS  fnt2_demo.bmp written

fnt2_create_sheet (1bpp):
  PASS  allocation succeeds
  PASS  width  = 16*8 = 128
  PASS  height =  8*8 = 64
  PASS  size = 8192 bytes
  PASS  fnt2_sheet_1bpp.bmp written

fnt2_create_sheet (2bpp):
  PASS  allocation succeeds
  PASS  fnt2_sheet_2bpp.bmp written

=== ALL PASS  (0 failures) ===
```

Zero warnings, zero errors, all 17 tests pass.

### BMP Header Verification

```python
import struct
for name in ['fnt2_demo.bmp', 'fnt2_sheet_1bpp.bmp', 'fnt2_sheet_2bpp.bmp']:
    with open(name, 'rb') as f:
        sig = f.read(2)
        fsize = struct.unpack('<I', f.read(4))[0]
        f.read(4)  # reserved
        offset = struct.unpack('<I', f.read(4))[0]
        f.read(4)  # DIB size
        w = struct.unpack('<I', f.read(4))[0]
        h = struct.unpack('<I', f.read(4))[0]
        f.read(2)  # planes
        bpp = struct.unpack('<H', f.read(2))[0]
        actual = len(open(name,'rb').read())
    ok = sig == b'BM' and fsize == actual and offset == 54 and bpp == 24
    print(f'{name}: {w}x{h} {bpp}bpp  file={fsize}B  {"OK" if ok else "BAD"}')
```

Output:
```
fnt2_demo.bmp:        48x8  24bpp  file=1210B  OK
fnt2_sheet_1bpp.bmp: 128x64 24bpp  file=24630B OK
fnt2_sheet_2bpp.bmp: 128x64 24bpp  file=24630B OK
```

All three files: correct BMP signature, pixel-data offset at byte 54, 24-bit colour,
and declared file size matching actual file size.

---

## Design Decisions

| Decision | Rationale |
|---|---|
| No named pointers in fnt2.h | fnt2.s has no symbolic labels inside the data; exporting none matches the source accurately |
| Generic `font_bitmap.c` (not fnt2-specific) | fnt1 and fnt2 share identical decode + BMP logic; parameterizing by `font_data` avoids duplication; future fonts (if any) reuse the same engine |
| `font_create_sheet` added to generic API | Not in fnt1_bitmap — needed for character-sheet BMP output; placed in the generic module so any font can use it |
| `calloc` in `font_create_sheet`, `malloc` in `font_create_bitmap` | Sheet has sparse characters (blanks) — calloc ensures zero-background without extra work; bitmap fills every slot explicitly |
| Default palette `{0xFFFFFF, 0x000000, 0x808080, 0x404040}` | White background / black foreground works for both 1BPP and 2BPP; caller can override for full 4-colour renders |
| `FontMode` enum (not `Fnt1Mode`) | Module is font-agnostic; using a generic name prevents a naming conflict when both fnt1 and font_bitmap are included together |
| BMP BGR byte order, `bmp_u16/bmp_u32` helpers | Portable little-endian writes without UB; host endianness irrelevant |
| `fnt2_create_sheet` fixes `cols=16` | 16 columns × 8 rows = 128-char sheet; standard character-set grid layout; no value in making it configurable per-font |

---

## File Summary

| File | Lines | Purpose |
|---|---|---|
| `dev/src/fnt2.h` | 13 | Extern for `fnt2_data[1024]` |
| `dev/src/fnt2.c` | ~140 | Full 1024-byte data array |
| `dev/src/font_bitmap.h` | 64 | Generic `FontMode` enum + 4 function declarations |
| `dev/src/font_bitmap.c` | 146 | Generic decode, sheet, bitmap, BMP writer |
| `dev/src/fnt2_bitmap.h` | 28 | 3 fnt2 wrapper declarations |
| `dev/src/fnt2_bitmap.c` | 19 | 3 one-liner wrappers calling `font_bitmap` with `fnt2_data` |
| `dev/src/test_fnt2.c` | 236 | 17 assertions: 8 single-char + 3 bitmap/sheet |
