#include "fnt1_bitmap.h"
#include "fnt1.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void fnt1_decode_char(uint8_t chr, Fnt1Mode mode, uint8_t out[64])
{
    const uint8_t *glyph = (const uint8_t *)&fnt1_data[chr * 8];

    if (mode == FNT1_1BPP) {
        for (int row = 0; row < 8; row++) {
            uint8_t byte = glyph[row];
            for (int col = 0; col < 8; col++)
                out[row * 8 + col] = (byte >> (7 - col)) & 1;
        }
    } else {
        /* FNT1_2BPP: 4 pixels per byte, each doubled to fill 8 output columns */
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

uint8_t *fnt1_create_bitmap(const uint8_t *chars, int n_chars, Fnt1Mode mode,
                              int *out_width, int *out_height)
{
    int width  = n_chars * 8;
    int height = 8;
    uint8_t *buf = malloc((size_t)width * height);
    if (!buf)
        return NULL;

    uint8_t glyph[64];
    for (int i = 0; i < n_chars; i++) {
        fnt1_decode_char(chars[i], mode, glyph);
        for (int row = 0; row < 8; row++)
            memcpy(&buf[row * width + i * 8], &glyph[row * 8], 8);
    }

    *out_width  = width;
    *out_height = height;
    return buf;
}

/* --- BMP writer ---------------------------------------------------------- */

/* Write a little-endian 16-bit value. */
static void bmp_write_u16(FILE *f, uint16_t v)
{
    uint8_t b[2] = { v & 0xFF, (v >> 8) & 0xFF };
    fwrite(b, 1, 2, f);
}

/* Write a little-endian 32-bit value. */
static void bmp_write_u32(FILE *f, uint32_t v)
{
    uint8_t b[4] = { v & 0xFF, (v >> 8) & 0xFF,
                     (v >> 16) & 0xFF, (v >> 24) & 0xFF };
    fwrite(b, 1, 4, f);
}

int fnt1_write_bmp(const char *filename, const uint8_t *pixels,
                   int width, int height, const uint32_t palette[4])
{
    static const uint32_t default_1bpp[4] = { 0xFFFFFF, 0x000000, 0, 0 };
    static const uint32_t default_2bpp[4] = { 0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF };

    if (!palette)
        palette = (width % 8 == 0) ? default_2bpp : default_1bpp;

    /* BMP rows are padded to a 4-byte boundary, stored bottom-to-top. */
    int row_bytes   = width * 3;
    int row_padded  = (row_bytes + 3) & ~3;
    int pixel_bytes = row_padded * height;

    uint32_t header_bytes = 14 + 40;         /* BITMAPFILEHEADER + BITMAPINFOHEADER */
    uint32_t file_bytes   = header_bytes + (uint32_t)pixel_bytes;

    FILE *f = fopen(filename, "wb");
    if (!f)
        return -1;

    /* BITMAPFILEHEADER */
    fwrite("BM", 1, 2, f);
    bmp_write_u32(f, file_bytes);
    bmp_write_u16(f, 0);                     /* reserved */
    bmp_write_u16(f, 0);                     /* reserved */
    bmp_write_u32(f, header_bytes);          /* pixel data offset */

    /* BITMAPINFOHEADER */
    bmp_write_u32(f, 40);                    /* header size */
    bmp_write_u32(f, (uint32_t)width);
    bmp_write_u32(f, (uint32_t)height);      /* positive = bottom-up */
    bmp_write_u16(f, 1);                     /* color planes */
    bmp_write_u16(f, 24);                    /* bits per pixel */
    bmp_write_u32(f, 0);                     /* no compression */
    bmp_write_u32(f, (uint32_t)pixel_bytes);
    bmp_write_u32(f, 2835);                  /* 72 DPI x */
    bmp_write_u32(f, 2835);                  /* 72 DPI y */
    bmp_write_u32(f, 0);
    bmp_write_u32(f, 0);

    /* Pixel data: bottom row first, each pixel as BGR. */
    uint8_t pad[3] = { 0, 0, 0 };
    int pad_len = row_padded - row_bytes;

    for (int row = height - 1; row >= 0; row--) {
        for (int col = 0; col < width; col++) {
            uint8_t idx  = pixels[row * width + col] & 0x03;
            uint32_t rgb = palette[idx];
            uint8_t px[3] = {
                (rgb)       & 0xFF,  /* B */
                (rgb >> 8)  & 0xFF,  /* G */
                (rgb >> 16) & 0xFF,  /* R */
            };
            fwrite(px, 1, 3, f);
        }
        if (pad_len)
            fwrite(pad, 1, pad_len, f);
    }

    fclose(f);
    return 0;
}
