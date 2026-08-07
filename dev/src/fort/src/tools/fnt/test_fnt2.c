#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fnt2_bitmap.h"
#include "font_bitmap.h"

static int failures = 0;

/* ---------- helpers ------------------------------------------------------ */

static void print_glyph(const char *label, const uint8_t px[64], int max_val)
{
    /* max_val 1 = 1bpp (show . / #), max_val 3 = 2bpp (show 0-3) */
    printf("  %s:\n", label);
    for (int row = 0; row < 8; row++) {
        printf("    ");
        for (int col = 0; col < 8; col++) {
            uint8_t v = px[row * 8 + col];
            putchar((max_val > 1) ? ('0' + v) : (v ? '#' : '.'));
        }
        putchar('\n');
    }
}

static void check(const char *what, int cond)
{
    if (cond) {
        printf("  PASS  %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        failures++;
    }
}

/* ---------- per-character tests ------------------------------------------ */

static void test_blank(void)
{
    puts("$00 (blank):");
    uint8_t px[64];
    fnt2_decode_char(0x00, FONT_1BPP, px);
    int all_zero = 1;
    for (int i = 0; i < 64; i++) all_zero &= (px[i] == 0);
    check("all pixels zero", all_zero);
}

static void test_horizontal_line(void)
{
    /* $3F: rows 0-5,7 are 0x00; row 6 is 0xFF (8 pixels set) */
    puts("$3F (horizontal line, 1bpp):");
    uint8_t px[64];
    fnt2_decode_char(0x3F, FONT_1BPP, px);
    print_glyph("1bpp", px, 1);

    int row6_full = 1, others_zero = 1;
    for (int col = 0; col < 8; col++)
        row6_full &= (px[6 * 8 + col] == 1);
    for (int row = 0; row < 8; row++) {
        if (row == 6) continue;
        for (int col = 0; col < 8; col++)
            others_zero &= (px[row * 8 + col] == 0);
    }
    check("row 6 all set (0xFF -> 8 pixels on)", row6_full);
    check("all other rows zero", others_zero);
}

static void test_diamond(void)
{
    /* $60: 0x00 0x18 0x3C 0x7E 0x7E 0x3C 0x18 0x00 — classic diamond */
    puts("$60 (diamond, 1bpp):");
    uint8_t px[64];
    fnt2_decode_char(0x60, FONT_1BPP, px);
    print_glyph("1bpp", px, 1);

    /* row 0 and row 7 must be blank */
    int r0_blank = 1, r7_blank = 1;
    for (int col = 0; col < 8; col++) {
        r0_blank &= (px[0 * 8 + col] == 0);
        r7_blank &= (px[7 * 8 + col] == 0);
    }
    /* row 3 (0x7E = 01111110) — 6 centre pixels set, edge 2 clear */
    int r3_ok = (px[3*8+0]==0 && px[3*8+1]==1 && px[3*8+6]==1 && px[3*8+7]==0);
    check("rows 0 and 7 blank", r0_blank && r7_blank);
    check("row 3 (0x7E) edges clear, centre set", r3_ok);
}

static void test_solid_fill_1bpp(void)
{
    /* $41: all bytes 0xAA = %10101010 → alternating columns */
    puts("$41 (0xAA solid, 1bpp):");
    uint8_t px[64];
    fnt2_decode_char(0x41, FONT_1BPP, px);
    print_glyph("1bpp", px, 1);

    int alt_ok = 1;
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            alt_ok &= (px[row * 8 + col] == (col % 2 == 0 ? 1 : 0));
    check("alternating col pattern (0xAA)", alt_ok);
}

static void test_solid_fill_2bpp(void)
{
    /* $41: all bytes 0xAA = %10 10 10 10 → all pixels color 2, doubled to 8 wide */
    puts("$41 (0xAA solid, 2bpp):");
    uint8_t px[64];
    fnt2_decode_char(0x41, FONT_2BPP, px);
    print_glyph("2bpp", px, 3);

    int all_2 = 1;
    for (int i = 0; i < 64; i++) all_2 &= (px[i] == 2);
    check("all pixels color 2 (0xAA -> 10 10 10 10 doubled)", all_2);
}

static void test_diagonal(void)
{
    /* $47: 0xC0 0xE0 0x70 0x38 0x1C 0x0E 0x07 0x03
       Top-left to bottom-right diagonal stripe */
    puts("$47 (diagonal stripe, 1bpp):");
    uint8_t px[64];
    fnt2_decode_char(0x47, FONT_1BPP, px);
    print_glyph("1bpp", px, 1);

    /* Row 0: 0xC0 = 11000000 → cols 0,1 set */
    int r0_ok = (px[0*8+0]==1 && px[0*8+1]==1 && px[0*8+2]==0);
    /* Row 7: 0x03 = 00000011 → cols 6,7 set */
    int r7_ok = (px[7*8+6]==1 && px[7*8+7]==1 && px[7*8+5]==0);
    check("row 0 (0xC0): bits 7-6 set", r0_ok);
    check("row 7 (0x03): bits 1-0 set", r7_ok);
}

static void test_2bpp_border(void)
{
    /* $21: first and last byte are 0x55 = %01 01 01 01 → all pixels color 1 */
    puts("$21 (text char with 0x55 border, 2bpp):");
    uint8_t px[64];
    fnt2_decode_char(0x21, FONT_2BPP, px);
    print_glyph("2bpp", px, 3);

    /* All pixels in row 0 should be color 1 (0x55 -> 01 01 01 01 doubled) */
    int r0_all1 = 1;
    for (int col = 0; col < 8; col++) r0_all1 &= (px[0*8+col] == 1);
    check("row 0 (0x55): all pixels color 1", r0_all1);
}

static void test_ring(void)
{
    /* $58: 0x3C 0xFF 0xFF 0xC3 0xC3 0xFF 0xFF 0x3C — ring with hollow centre */
    puts("$58 (ring, 1bpp):");
    uint8_t px[64];
    fnt2_decode_char(0x58, FONT_1BPP, px);
    print_glyph("1bpp", px, 1);

    /* 0xC3 = 11000011: outer cols set, inner clear */
    int r3_ok = (px[3*8+0]==1 && px[3*8+1]==1 &&
                 px[3*8+2]==0 && px[3*8+5]==0 &&
                 px[3*8+6]==1 && px[3*8+7]==1);
    check("row 3 (0xC3): outer pixels set, inner clear", r3_ok);
}

/* ---------- bitmap / sheet tests ----------------------------------------- */

static void test_create_bitmap(void)
{
    puts("fnt2_create_bitmap (6 chars):");
    const uint8_t chars[] = { 0x47, 0x41, 0x42, 0x43, 0x44, 0x60 };
    int w, h;
    uint8_t *bmp = fnt2_create_bitmap(chars, 6, FONT_1BPP, &w, &h);
    check("allocation succeeds", bmp != NULL);
    check("width = 6*8 = 48", w == 48);
    check("height = 8",        h == 8);

    if (bmp) {
        static const uint32_t bw[4] = { 0xFFFFFF, 0x000000, 0x808080, 0x404040 };
        int rc = font_write_bmp("fnt2_demo.bmp", bmp, w, h, bw);
        check("fnt2_demo.bmp written", rc == 0);
        free(bmp);
    }
}

static void test_sheet_1bpp(void)
{
    puts("fnt2_create_sheet (1bpp):");
    int w, h;
    uint8_t *sheet = fnt2_create_sheet(FONT_1BPP, &w, &h);
    check("allocation succeeds", sheet != NULL);
    check("width  = 16*8 = 128", w == 128);
    check("height =  8*8 = 64",  h == 64);
    check("size = 8192 bytes", w * h == 8192);

    if (sheet) {
        int rc = font_write_bmp("fnt2_sheet_1bpp.bmp", sheet, w, h, NULL);
        check("fnt2_sheet_1bpp.bmp written", rc == 0);
        free(sheet);
    }
}

static void test_sheet_2bpp(void)
{
    puts("fnt2_create_sheet (2bpp):");
    int w, h;
    uint8_t *sheet = fnt2_create_sheet(FONT_2BPP, &w, &h);
    check("allocation succeeds", sheet != NULL);

    if (sheet) {
        static const uint32_t pal[4] = { 0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF };
        int rc = font_write_bmp("fnt2_sheet_2bpp.bmp", sheet, w, h, pal);
        check("fnt2_sheet_2bpp.bmp written", rc == 0);
        free(sheet);
    }
}

/* ---------- main --------------------------------------------------------- */

int main(void)
{
    puts("=== fnt2 bitmap tests ===\n");

    test_blank();           putchar('\n');
    test_horizontal_line(); putchar('\n');
    test_diamond();         putchar('\n');
    test_solid_fill_1bpp(); putchar('\n');
    test_solid_fill_2bpp(); putchar('\n');
    test_diagonal();        putchar('\n');
    test_2bpp_border();     putchar('\n');
    test_ring();            putchar('\n');
    test_create_bitmap();   putchar('\n');
    test_sheet_1bpp();      putchar('\n');
    test_sheet_2bpp();      putchar('\n');

    printf("=== %s  (%d failure%s) ===\n",
           failures == 0 ? "ALL PASS" : "FAILED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
