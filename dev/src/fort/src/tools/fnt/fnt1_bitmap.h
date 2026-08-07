#ifndef FNT1_BITMAP_H
#define FNT1_BITMAP_H

#include <stdint.h>

/*
 * Pixel encoding modes matching Atari ANTIC display modes used by Fort Apocalypse.
 *
 * FNT1_1BPP  ANTIC mode 2: 8 pixels per row byte, 1 bit per pixel.
 *            Pixel value 0 = background, 1 = foreground.
 *            Characters $00-$0F and some special chars use this mode.
 *
 * FNT1_2BPP  ANTIC mode 4: 4 pixels per row byte, 2 bits per pixel.
 *            The Atari hardware doubles each pixel horizontally, so the
 *            output is stretched to 8 pixels wide.
 *            Pixel values 0-3 map to four palette entries.
 *            Digits ($10-$19), punctuation ($21-$3A), and A-Z ($41-$5A)
 *            use this mode.
 */
typedef enum {
    FNT1_1BPP,
    FNT1_2BPP,
} Fnt1Mode;

/*
 * Decode a single 8x8 character glyph into a flat 64-byte pixel array.
 *
 * chr    character index (0-127)
 * mode   FNT1_1BPP or FNT1_2BPP
 * out    8x8 output, row-major: out[row*8 + col], values 0-1 (1bpp) or 0-3 (2bpp)
 */
void fnt1_decode_char(uint8_t chr, Fnt1Mode mode, uint8_t out[64]);

/*
 * Render an array of character indices into a flat pixel buffer.
 *
 * chars      array of font character indices (0-127)
 * n_chars    number of characters; must be > 0
 * mode       FNT1_1BPP or FNT1_2BPP
 * out_width  receives n_chars * 8
 * out_height receives 8
 *
 * Returns a heap-allocated uint8_t array of (*out_width * *out_height) bytes,
 * row-major, 1 byte per pixel (value 0-1 for 1bpp, 0-3 for 2bpp).
 * The caller must free() it. Returns NULL on allocation failure.
 */
uint8_t *fnt1_create_bitmap(const uint8_t *chars, int n_chars, Fnt1Mode mode,
                             int *out_width, int *out_height);

/*
 * Write pixel data to a 24-bit uncompressed BMP file.
 *
 * filename   output path
 * pixels     buffer from fnt1_create_bitmap()
 * width      image width in pixels
 * height     image height in pixels
 * palette    4-entry array of 0x00RRGGBB colors mapping pixel values 0-3.
 *            Pass NULL to use the built-in defaults:
 *              1bpp: { 0xFFFFFF, 0x000000, 0, 0 }
 *              2bpp: { 0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF }
 *
 * Returns 0 on success, -1 on I/O error.
 */
int fnt1_write_bmp(const char *filename, const uint8_t *pixels,
                   int width, int height, const uint32_t palette[4]);

#endif /* FNT1_BITMAP_H */
