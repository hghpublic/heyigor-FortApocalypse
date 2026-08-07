#include <stdio.h>
#include <stdlib.h>
#include "fnt1.h"
#include "font_bitmap.h"

int main(void)
{
    const char *out1 = "fnt1_sheet_1bpp.bmp";
    const char *out2 = "fnt1_sheet_2bpp.bmp";
    int w, h, rc;

    /* 1BPP sheet: raw bit patterns — correct for $00-$0F specials, masks */
    uint8_t *s1 = font_create_sheet(fnt1_data, 128, 16, FONT_1BPP, &w, &h);
    if (!s1) { fputs("alloc failed\n", stderr); return 1; }
    rc = font_write_bmp(out1, s1, w, h, NULL);
    printf("1BPP %dx%d → %s: %s\n", w, h, out1, rc == 0 ? "OK" : "FAILED");
    free(s1);

    /* 2BPP sheet: hardware-stretched view — correct for digits $10-$19, A-Z $41-$5A */
    static const uint32_t pal2[4] = { 0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF };
    uint8_t *s2 = font_create_sheet(fnt1_data, 128, 16, FONT_2BPP, &w, &h);
    if (!s2) { fputs("alloc failed\n", stderr); return 1; }
    rc = font_write_bmp(out2, s2, w, h, pal2);
    printf("2BPP %dx%d → %s: %s\n", w, h, out2, rc == 0 ? "OK" : "FAILED");
    free(s2);

    return 0;
}
