#include "font_bitmap.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    } else {
        /* FONT_2BPP: 4 pixels per byte, each doubled to 8 output columns */
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

uint8_t *font_create_bitmap(const uint8_t *font_data, const uint8_t *chars,
                             int n_chars, FontMode mode,
                             int *out_width, int *out_height)
{
    int width  = n_chars * 8;
    int height = 8;
    uint8_t *buf = malloc((size_t)width * height);
    if (!buf)
        return NULL;

    uint8_t glyph[64];
    for (int i = 0; i < n_chars; i++) {
        font_decode_char(font_data, chars[i], mode, glyph);
        for (int row = 0; row < 8; row++)
            memcpy(&buf[row * width + i * 8], &glyph[row * 8], 8);
    }

    *out_width  = width;
    *out_height = height;
    return buf;
}

uint8_t *font_create_sheet(const uint8_t *font_data, int n_chars, int cols,
                            FontMode mode, int *out_width, int *out_height)
{
    int rows   = (n_chars + cols - 1) / cols;
    int width  = cols * 8;
    int height = rows * 8;
    uint8_t *sheet = calloc((size_t)width * height, 1);
    if (!sheet)
        return NULL;

    uint8_t glyph[64];
    for (int chr = 0; chr < n_chars; chr++) {
        font_decode_char(font_data, (uint8_t)chr, mode, glyph);
        int gc = chr % cols;
        int gr = chr / cols;
        for (int row = 0; row < 8; row++)
            memcpy(&sheet[(gr * 8 + row) * width + gc * 8], &glyph[row * 8], 8);
    }

    *out_width  = width;
    *out_height = height;
    return sheet;
}

/* --- BMP writer ---------------------------------------------------------- */

static void bmp_u16(FILE *f, uint16_t v)
{
    uint8_t b[2] = { v & 0xFF, (v >> 8) & 0xFF };
    fwrite(b, 1, 2, f);
}

static void bmp_u32(FILE *f, uint32_t v)
{
    uint8_t b[4] = { v & 0xFF, (v >> 8) & 0xFF,
                     (v >> 16) & 0xFF, (v >> 24) & 0xFF };
    fwrite(b, 1, 4, f);
}

int font_write_bmp(const char *filename, const uint8_t *pixels,
                   int width, int height, const uint32_t palette[4])
{
    static const uint32_t default_pal[4] = {
        0xFFFFFF, /* 0 = background (white) */
        0x000000, /* 1 = foreground (black) */
        0x808080, /* 2 = mid gray           */
        0x404040, /* 3 = dark gray          */
    };
    if (!palette)
        palette = default_pal;

    int row_bytes  = width * 3;
    int row_padded = (row_bytes + 3) & ~3;
    int pix_bytes  = row_padded * height;

    uint32_t hdr_bytes  = 14 + 40;
    uint32_t file_bytes = hdr_bytes + (uint32_t)pix_bytes;

    FILE *f = fopen(filename, "wb");
    if (!f)
        return -1;

    /* BITMAPFILEHEADER */
    fwrite("BM", 1, 2, f);
    bmp_u32(f, file_bytes);
    bmp_u16(f, 0); bmp_u16(f, 0);   /* reserved */
    bmp_u32(f, hdr_bytes);

    /* BITMAPINFOHEADER */
    bmp_u32(f, 40);
    bmp_u32(f, (uint32_t)width);
    bmp_u32(f, (uint32_t)height);    /* positive = bottom-up storage */
    bmp_u16(f, 1);                   /* color planes */
    bmp_u16(f, 24);                  /* bpp */
    bmp_u32(f, 0);                   /* no compression */
    bmp_u32(f, (uint32_t)pix_bytes);
    bmp_u32(f, 2835); bmp_u32(f, 2835); /* 72 DPI */
    bmp_u32(f, 0);    bmp_u32(f, 0);

    /* Pixel rows, bottom-to-top, BGR order, padded to 4 bytes */
    uint8_t pad[3] = { 0 };
    for (int row = height - 1; row >= 0; row--) {
        for (int col = 0; col < width; col++) {
            uint32_t rgb = palette[pixels[row * width + col] & 0x03];
            uint8_t px[3] = { rgb & 0xFF, (rgb >> 8) & 0xFF, (rgb >> 16) & 0xFF };
            fwrite(px, 1, 3, f);
        }
        fwrite(pad, 1, (size_t)(row_padded - row_bytes), f);
    }

    fclose(f);
    return 0;
}
