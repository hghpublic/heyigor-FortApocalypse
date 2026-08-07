#include <stdio.h>
#include <stdlib.h>
#include "fnt1_bitmap.h"
#include "fnt1.h"

int main(void)
{
    /* Render "FORT" (chars $46 $4F $52 $54) in 2bpp mode */
    const uint8_t word[] = { 0x46, 0x4F, 0x52, 0x54 };
    int w, h;
    uint8_t *bmp = fnt1_create_bitmap(word, 4, FNT1_2BPP, &w, &h);
    if (!bmp) { fputs("alloc failed\n", stderr); return 1; }

    /* Dump pixels as ASCII to verify */
    printf("FORT (%dx%d):\n", w, h);
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++)
            putchar(bmp[row * w + col] ? '#' : '.');
        putchar('\n');
    }

    int rc = fnt1_write_bmp("/tmp/fort_test.bmp", bmp, w, h, NULL);
    printf("BMP write: %s\n", rc == 0 ? "ok" : "error");
    free(bmp);

    /* Also test a single char decode */
    uint8_t glyph[64];
    fnt1_decode_char(0x0E, FNT1_1BPP, glyph); /* FORT.EX3 explosion ring */
    printf("\nFORT.EX3 ($0E) 1bpp:\n");
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++)
            putchar(glyph[row * 8 + col] ? '#' : '.');
        putchar('\n');
    }
    return 0;
}
