#ifndef FNT2_H
#define FNT2_H

#include "fnt.h"

/* Fort Apocalypse - Font 2 (Graphics)
 * 128 characters, 8 bytes each.
 * CHR $00-$1F and $73-$7F are blank (all zero).
 * Index a character with fnt2_data[char_index].
 */
extern const fnt_char_t fnt2_data[fnt_char_count];

#endif /* FNT2_H */
