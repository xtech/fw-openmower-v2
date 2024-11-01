//
// Created by clemens on 01.10.24.
//

#ifndef GLOBALS_H
#define GLOBALS_H
#ifdef __cplusplus
extern "C" {
#endif
#include <id_eeprom.h>

extern struct board_info board_info;
extern struct carrier_board_info carrier_board_info;

void InitGlobals(void);
#ifdef __cplusplus
}
#endif
#endif  // GLOBALS_H
