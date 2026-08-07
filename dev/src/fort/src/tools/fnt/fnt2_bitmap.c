#include "fnt2_bitmap.h"
#include "fnt2.h"
#include "font_bitmap.h"

void fnt2_decode_char(uint8_t chr, FontMode mode, uint8_t out[64])
{
    font_decode_char((const uint8_t *)fnt2_data, chr, mode, out);
}

uint8_t *fnt2_create_bitmap(const uint8_t *chars, int n_chars, FontMode mode,
                             int *out_width, int *out_height)
{
    return font_create_bitmap((const uint8_t *)fnt2_data, chars, n_chars, mode, out_width, out_height);
}

uint8_t *fnt2_create_sheet(FontMode mode, int *out_width, int *out_height)
{
    return font_create_sheet((const uint8_t *)fnt2_data, 128, 16, mode, out_width, out_height);
}
