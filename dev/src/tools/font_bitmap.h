#ifndef FONT_BITMAP_H
#define FONT_BITMAP_H

#include <stdint.h>

/*
 * Pixel encoding modes matching Atari ANTIC display modes.
 *
 * FONT_1BPP  ANTIC mode 2: 8 pixels per row byte, 1 bit per pixel.
 *            Pixel value 0 = background, 1 = foreground.
 *
 * FONT_2BPP  ANTIC mode 4: 4 pixels per row byte, 2 bits per pixel.
 *            The Atari hardware doubles each pixel horizontally, so output
 *            is stretched to 8 pixels wide. Pixel values 0-3 map to four
 *            palette entries.
 */
typedef enum {
    FONT_1BPP,
    FONT_2BPP,
} FontMode;

/*
 * Decode a single 8x8 character into a flat 64-byte pixel array.
 *
 * font_data  base of the font table (128 chars * 8 bytes)
 * chr        character index (0-127)
 * mode       FONT_1BPP or FONT_2BPP
 * out        row-major 8x8: out[row*8+col], values 0-1 (1bpp) or 0-3 (2bpp)
 */
void font_decode_char(const uint8_t *font_data, uint8_t chr, FontMode mode,
                      uint8_t out[64]);

/*
 * Render an array of character indices into a flat pixel buffer.
 *
 * Characters are placed side-by-side: output is (n_chars*8) wide and 8 tall.
 * Returns a heap-allocated buffer the caller must free(), or NULL on failure.
 */
uint8_t *font_create_bitmap(const uint8_t *font_data, const uint8_t *chars,
                             int n_chars, FontMode mode,
                             int *out_width, int *out_height);

/*
 * Render all n_chars characters arranged in `cols` columns.
 *
 * Rows = ceil(n_chars / cols). Output: (cols*8) wide, (rows*8) tall.
 * Returns a heap-allocated buffer the caller must free(), or NULL on failure.
 */
uint8_t *font_create_sheet(const uint8_t *font_data, int n_chars, int cols,
                            FontMode mode, int *out_width, int *out_height);

/*
 * Write pixel data to a 24-bit uncompressed BMP file.
 *
 * palette  4-entry 0x00RRGGBB array: index 0 = darkest, 3 = brightest.
 *          Pass NULL to use { 0xFFFFFF, 0x000000, 0x808080, 0x404040 }
 *          (white bg / black fg / two grays — works for both 1bpp and 2bpp).
 *
 * Returns 0 on success, -1 on I/O error.
 */
int font_write_bmp(const char *filename, const uint8_t *pixels,
                   int width, int height, const uint32_t palette[4]);

#endif /* FONT_BITMAP_H */
