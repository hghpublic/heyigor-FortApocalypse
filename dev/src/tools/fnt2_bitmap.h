#ifndef FNT2_BITMAP_H
#define FNT2_BITMAP_H

#include <stdint.h>
#include "font_bitmap.h"

/*
 * Decode a single fnt2 character into a flat 64-byte pixel array.
 * Wraps font_decode_char() using fnt2_data.
 */
void fnt2_decode_char(uint8_t chr, FontMode mode, uint8_t out[64]);

/*
 * Render an array of fnt2 character indices into a pixel buffer.
 * Output is (n_chars*8) wide and 8 tall.
 * Returns a heap-allocated buffer the caller must free(), or NULL on failure.
 */
uint8_t *fnt2_create_bitmap(const uint8_t *chars, int n_chars, FontMode mode,
                             int *out_width, int *out_height);

/*
 * Render all 128 fnt2 characters as a 16-column character sheet.
 * Output is 128 wide (16*8) and 64 tall (8*8).
 * Returns a heap-allocated buffer the caller must free(), or NULL on failure.
 */
uint8_t *fnt2_create_sheet(FontMode mode, int *out_width, int *out_height);

#endif /* FNT2_BITMAP_H */
