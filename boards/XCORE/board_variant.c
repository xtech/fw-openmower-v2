#include "board_variant.h"

#include <stdbool.h>
#include <string.h>

#include "board_phy.h"

static board_variant_t board_variant = BOARD_VARIANT_NOT_YET_DETECTED;

static bool board_id_is(const char *board_id, size_t board_id_len,
                        const char *candidate) {
  // board_id is the ID EEPROM's fixed-width field and is not necessarily
  // NUL-terminated (see board_variant.h), so match candidate's bytes
  // directly instead of relying on strnlen() to find a NUL that may not be
  // there -- that would silently mis-detect a correctly-provisioned
  // xcore-lite as the fallback BOARD_VARIANT_XCORE.
  size_t candidate_len = strlen(candidate);
  return candidate_len <= board_id_len && memcmp(board_id, candidate, candidate_len) == 0;
}

void InitBoardVariant(const char *board_id, size_t board_id_len) {
  if (board_id_is(board_id, board_id_len, "xcore-lite")) {
    board_variant = BOARD_VARIANT_XCORE_LITE;
  } else {
    // InitGlobals() has obtained a checksum-valid, nonempty board identity.
    // Preserve the original-xcore fallback for legacy board identifiers.
    board_variant = BOARD_VARIANT_XCORE;
  }

  // Variant-specific setup goes here, one call per subsystem.
  BoardPhy_Init();
}

board_variant_t GetBoardVariant(void) { return board_variant; }
