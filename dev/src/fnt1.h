#ifndef FNT1_H
#define FNT1_H

#include <stdint.h>

/* Fort Apocalypse - Font 1 (Alphabet)
 * 128 characters, 8 bytes each.
 * Index a character with fnt1_data[char_index * 8].
 */
extern const uint8_t fnt1_data[128 * 8];

/* Named entries from original assembly labels */
extern const uint8_t *const POS_MASK1; /* $0B - position bitmask */
extern const uint8_t *const FORT_EX1;  /* $0C - explosion frame 1 */
extern const uint8_t *const FORT_EX2;  /* $0D - explosion frame 2 */
extern const uint8_t *const FORT_EX3;  /* $0E - explosion frame 3 */
extern const uint8_t *const FORT_EX4;  /* $0F - explosion frame 4 */
extern const uint8_t *const T_5;       /* $20 - T.5 graphic */
extern const uint8_t *const EXP_SHAPE; /* $3C - explosion shape */

#endif /* FNT1_H */
