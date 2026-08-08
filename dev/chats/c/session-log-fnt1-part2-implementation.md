# Session Log: convert fnt1.s — Part 2: Implementation

## Files Produced
- `dev/src/fnt1.h` (21 lines)
- `dev/src/fnt1.c` (244 lines)
- `dev/src/fnt1_bitmap.h` (67 lines)
- `dev/src/fnt1_bitmap.c` (133 lines)
- `dev/src/fnt1_test.c` (36 lines)

---

## fnt1.h

```c
#ifndef FNT1_H
#define FNT1_H
#include <stdint.h>

extern const uint8_t fnt1_data[128 * 8];

/* Named pointers — point into fnt1_data at the given CHR×8 offset */
extern const uint8_t *const POS_MASK1; /* CHR $0B: scanner XOR bitmask */
extern const uint8_t *const FORT_EX1;  /* CHR $0C: explosion frame 1   */
extern const uint8_t *const FORT_EX2;  /* CHR $0D: explosion frame 2   */
extern const uint8_t *const FORT_EX3;  /* CHR $0E: explosion frame 3   */
extern const uint8_t *const FORT_EX4;  /* CHR $0F: explosion frame 4   */
extern const uint8_t *const T_5;       /* CHR $20: keyboard labels      */
extern const uint8_t *const EXP_SHAPE; /* CHR $3C: explosion mask       */

#endif
```

---

## fnt1_bitmap.h

```c
#ifndef FNT1_BITMAP_H
#define FNT1_BITMAP_H
#include <stdint.h>

typedef enum { FNT1_1BPP, FNT1_2BPP } Fnt1Mode;

/* Decode one 8×8 glyph into a flat 64-byte pixel array.
 * 1BPP: pixels are 0 or 1.  2BPP: pixels are 0–3, each output pixel doubled. */
void fnt1_decode_char(uint8_t chr, Fnt1Mode mode, uint8_t out[64]);

/* Render a sequence of n_chars glyphs side-by-side into a heap-allocated pixel
 * buffer.  Returns NULL on allocation failure.  Caller must free(). */
uint8_t *fnt1_create_bitmap(const uint8_t *chars, int n_chars, Fnt1Mode mode,
                             int *out_width, int *out_height);

/* Write a 24-bit BMP file.  palette[4] maps pixel values 0-3 to 0x00RRGGBB;
 * pass NULL for built-in defaults (black / dark-grey / light-grey / white). */
int fnt1_write_bmp(const char *filename, const uint8_t *pixels,
                   int width, int height, const uint32_t palette[4]);

#endif
```

---

## fnt1.c Structure

### Array Declaration

```c
const uint8_t fnt1_data[128 * 8] = {
    /* CHR $00 BLANK */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    /* CHR $01 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0,

    /* CHR $02 ... */
    ...

    /* CHR $0B POS.MASK1 */
    0x80, 0x80, 0x20, 0x20, 0x08, 0x08, 0x02, 0x02,

    /* CHR $0C FORT.EX1 */
    0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,

    /* CHR $0D FORT.EX2 */
    0x00, 0x18, 0x24, 0x24, 0x18, 0x00, 0x00, 0x00,

    /* CHR $0E FORT.EX3 */
    0x18, 0x24, 0x5A, 0x5A, 0x24, 0x18, 0x00, 0x00,

    /* CHR $0F FORT.EX4 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    /* ... digits, punctuation ... */

    /* CHR $20 T.5 — original source: .AT -/1!9)8(2"/ stored as raw ASCII */
    0x31, 0x21, 0x39, 0x29, 0x38, 0x28, 0x32, 0x22,

    /* ... more chars ... */

    /* CHR $3C EXP.SHAPE */
    0x3C, 0x3C, 0xFF, 0xFF, 0xFF, 0xFF, 0x3C, 0x3C,

    /* ... A-Z ... */

    /* CHR $5B-$7F BLANK (37 characters × 8 bytes) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* $5B */
    /* ... 36 more ... */
};
```

### Named Pointer Definitions

```c
const uint8_t *const POS_MASK1 = &fnt1_data[0x0B * 8];
const uint8_t *const FORT_EX1  = &fnt1_data[0x0C * 8];
const uint8_t *const FORT_EX2  = &fnt1_data[0x0D * 8];
const uint8_t *const FORT_EX3  = &fnt1_data[0x0E * 8];
const uint8_t *const FORT_EX4  = &fnt1_data[0x0F * 8];
const uint8_t *const T_5       = &fnt1_data[0x20 * 8];
const uint8_t *const EXP_SHAPE = &fnt1_data[0x3C * 8];
```

**Pointer pattern:** Each is a `const uint8_t *const` — both the pointer and
the pointed-to data are const. No data is duplicated; the pointer is simply an
alias into the flat array. This exactly mirrors the assembly where labels are
addresses into the same data block.

---

## fnt1_bitmap.c — Decode Algorithms

### 1BPP Decode

```c
case FNT1_1BPP:
    for (int row = 0; row < 8; row++) {
        uint8_t byte = fnt1_data[(uint8_t)chr * 8 + row];
        for (int col = 0; col < 8; col++) {
            out[row * 8 + col] = (byte >> (7 - col)) & 1u;
        }
    }
```

MSB of each byte = leftmost pixel. `7 - col` shifts the relevant bit to position 0.
Output pixel values are 0 (background) or 1 (foreground).

### 2BPP Decode

```c
case FNT1_2BPP:
    for (int row = 0; row < 8; row++) {
        uint8_t byte = fnt1_data[(uint8_t)chr * 8 + row];
        for (int px = 0; px < 4; px++) {
            uint8_t val = (byte >> (6 - px * 2)) & 0x03u;
            out[row * 8 + px * 2]     = val;
            out[row * 8 + px * 2 + 1] = val;  /* doubled: ANTIC mode 4 */
        }
    }
```

Each byte contains 4 × 2-bit pixel values. The most significant pair
(bits 7:6) = leftmost pixel, least significant pair (bits 1:0) = rightmost.
Each decoded pixel is written twice to produce the hardware-doubled width.

**Bit extraction:** `(byte >> (6 - px * 2)) & 0x03`:
- `px=0`: `byte >> 6` → bits 7:6
- `px=1`: `byte >> 4` → bits 5:4
- `px=2`: `byte >> 2` → bits 3:2
- `px=3`: `byte >> 0` → bits 1:0

---

## fnt1_bitmap.c — BMP Writer

```c
int fnt1_write_bmp(const char *filename, const uint8_t *pixels,
                   int width, int height, const uint32_t palette[4])
{
    /* BMP file format: 14-byte file header + 40-byte DIB header */
    /* Pixel data: 24-bit BGR, bottom-up row order, 4-byte aligned */
    int row_bytes = width * 3;
    int row_padded = (row_bytes + 3) & ~3;  /* round up to 4 */
    uint32_t pixel_size = (uint32_t)(row_padded * height);
    uint32_t file_size  = 54u + pixel_size;

    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    put_u32(hdr + 2, file_size);
    put_u32(hdr + 10, 54u);         /* pixel data offset */
    put_u32(hdr + 14, 40u);         /* DIB header size */
    put_u32(hdr + 18, (uint32_t)width);
    put_u32(hdr + 22, (uint32_t)height);
    put_u16(hdr + 26, 1u);          /* colour planes */
    put_u16(hdr + 28, 24u);         /* bits per pixel */
    /* biCompression=0 (BI_RGB), biXPelsPerMeter=0, biYPelsPerMeter=0, etc. */

    /* Rows written bottom-up: row height-1 first, row 0 last */
    for (int row = height - 1; row >= 0; row--) {
        for (int col = 0; col < width; col++) {
            uint8_t p = pixels[row * width + col];
            uint32_t rgb = pal[p];      /* 0x00RRGGBB */
            fwrite BGR bytes...
        }
        fwrite padding bytes (row_padded - row_bytes)...
    }
}
```

**Little-endian helpers:** `put_u32` and `put_u16` write 4- and 2-byte values
in little-endian byte order to a byte buffer. No reliance on endianness of the
host platform.

---

## fnt1_test.c

```c
int main(void)
{
    /* Render "FORT" (chars $46 $4F $52 $54) in 2BPP mode */
    const uint8_t word[] = { 0x46, 0x4F, 0x52, 0x54 };
    int w, h;
    uint8_t *bmp = fnt1_create_bitmap(word, 4, FNT1_2BPP, &w, &h);
    if (!bmp) { fputs("alloc failed\n", stderr); return 1; }

    /* ASCII-art dump for terminal verification */
    printf("FORT (%dx%d):\n", w, h);
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++)
            putchar(bmp[row * w + col] ? '#' : '.');
        putchar('\n');
    }

    int rc = fnt1_write_bmp("/tmp/fort_test.bmp", bmp, w, h, NULL);
    printf("BMP write: %s\n", rc == 0 ? "ok" : "error");
    free(bmp);

    /* Single character decode: FORT.EX3 ($0E) in 1BPP */
    uint8_t glyph[64];
    fnt1_decode_char(0x0E, FNT1_1BPP, glyph);
    printf("\nFORT.EX3 ($0E) 1bpp:\n");
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++)
            putchar(glyph[row * 8 + col] ? '#' : '.');
        putchar('\n');
    }
    return 0;
}
```

**Build command used:**
```
gcc -Wall -Wextra -o /tmp/fnt1_test \
    /tmp/fnt1_test.c dev/src/fnt1.c dev/src/fnt1_bitmap.c \
    -I dev/src
```

**Expected FORT.EX3 output (1BPP):**
```
...##...
..#..#..
.#.##.#.
.#.##.#.
..#..#..
...##...
........
........
```
Matches `{0x18, 0x24, 0x5A, 0x5A, 0x24, 0x18, 0x00, 0x00}` decoded:
- `0x18` = `00011000` → `...##...`
- `0x24` = `00100100` → `..#..#..`
- `0x5A` = `01011010` → `.#.##.#.`

---

## Byte Count Verification

Verified with Python:

```python
import re
data = open('dev/src/fnt1.c').read()
array_body = re.search(r'fnt1_data\[128 \* 8\] = \{(.*?)\};', data, re.DOTALL).group(1)
count = len(re.findall(r'0x[0-9A-Fa-f]{2}', array_body))
print(f'Array bytes: {count} (expected 1024)')
# → Array bytes: 1024
```

The 7 pointer declarations (`0x0B * 8`, etc.) are outside the array initializer
and are not counted by the search.

---

## Compile Test

### Command
```
gcc -Wall -Wextra -o /tmp/fnt1_test \
    /tmp/fnt1_test.c dev/src/fnt1.c dev/src/fnt1_bitmap.c \
    -I dev/src && /tmp/fnt1_test
```

### Result
```
FORT (32x8):
(ascii art of FORT in 2bpp doubled pixels)
BMP write: ok

FORT.EX3 ($0E) 1bpp:
...##...
..#..#..
.#.##.#.
.#.##.#.
..#..#..
...##...
........
........
```

Zero warnings, zero errors.

---

## Design Decisions

| Decision | Rationale |
|---|---|
| Flat `fnt1_data[1024]` array, not 128 sub-arrays | Assembly is one contiguous block. Flat array preserves the memory layout and enables pointer arithmetic for named labels |
| `const uint8_t *const` named pointers | Both pointer and data are immutable. Matches assembly where labels are read-only addresses into ROM data |
| No data duplication for named pointers | `POS_MASK1 = &fnt1_data[0x0B * 8]` — zero overhead, no sync risk |
| T.5 stored as raw ASCII `{0x31,...}` | `.AT -` negates the screen-code conversion; net result is literal ASCII character codes. Storing these verbatim is most faithful and avoids a silent translation |
| CHR $5B–$7F written as 37 explicit entries | Comment-only in assembly = zero bytes. Written explicitly in C for clarity and to ensure exact 1024-byte count |
| Bitmap module in separate file | Visualization code is not part of the game; keeping it separate prevents recompilation of fnt1.c when testing changes |
| 1BPP and 2BPP modes as enum (not boolean) | Future font files (fnt2.s) also use two modes; enum allows extension |
| BMP bottom-up row order | Standard BMP format; any viewer handles it correctly without rotation |

---

## File Summary

| File | Lines | Purpose |
|---|---|---|
| `dev/src/fnt1.h` | 21 | `fnt1_data[1024]` extern + 7 named pointer externs |
| `dev/src/fnt1.c` | 244 | Full 1024-byte data array + named pointer definitions |
| `dev/src/fnt1_bitmap.h` | 67 | `Fnt1Mode` enum + decode/bitmap/BMP API |
| `dev/src/fnt1_bitmap.c` | 133 | 1BPP/2BPP decoders + little-endian BMP writer |
| `dev/src/fnt1_test.c` | 36 | "FORT" 2BPP strip + FORT.EX3 1BPP ASCII-art + BMP file |
