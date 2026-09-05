#ifndef BOARD_VARIANT_H
#define BOARD_VARIANT_H

#if !defined(_FROM_ASM_) && !defined(__ASSEMBLER__)

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  // InitBoardVariant() has not run yet. Subsystems that are asked to do
  // variant-specific work before that point have nothing to go on, so they
  // must treat this as "do nothing" rather than guessing a board.
  BOARD_VARIANT_NOT_YET_DETECTED = 0,
  BOARD_VARIANT_XCORE,
  BOARD_VARIANT_XCORE_LITE,
} board_variant_t;

// Identifies the board from the ID EEPROM's board_info.board_id field (not
// necessarily null-terminated, hence the explicit length), then applies every
// piece of setup that differs between the variants.
//
// Call once from main(), after the ID EEPROM has been read. Anything that
// needs to differ per board belongs behind this call rather than in main():
// today that is only the Ethernet PHY, via BoardPhy_Init().
void InitBoardVariant(const char *board_id, size_t board_id_len);

// The variant InitBoardVariant() settled on, or BOARD_VARIANT_NOT_YET_DETECTED
// before it has run.
board_variant_t GetBoardVariant(void);

#ifdef __cplusplus
}
#endif

#endif /* !defined(_FROM_ASM_) && !defined(__ASSEMBLER__) */

#endif  // BOARD_VARIANT_H
