#ifndef FNT2_H
#define FNT2_H

#include <stdint.h>

/* Fort Apocalypse - Font 2 (Graphics)
 * 128 characters, 8 bytes each.
 * CHR $00-$1F and $73-$7F are blank (all zero).
 * Index a character with fnt2_data[char_index * 8].
 */
extern const uint8_t fnt2_data[128 * 8];

#endif /* FNT2_H */
