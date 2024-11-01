//
// Created by clemens on 01.10.24.
//

#include "globals.h"
struct board_info board_info = {0};
struct carrier_board_info carrier_board_info = {0};

void InitGlobals() {
  ID_EEPROM_GetBoardInfo(&board_info);
  ID_EEPROM_GetCarrierBoardInfo(&carrier_board_info);
}
