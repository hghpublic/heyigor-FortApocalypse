#ifndef FNT_H
#define FNT_H

#include <stdint.h>

static constexpr uint8_t fnt_char_count = 128;
static constexpr uint8_t fnt_char_len = 8;
static constexpr uint16_t fnt_data_len = fnt_char_count * fnt_char_len;
typedef uint8_t fnt_char_t[fnt_char_len];

#endif /* FNT_H */
