//
// Created by clemens on 05.04.25.
//

#ifndef ROBOT_EX_HPP
#define ROBOT_EX_HPP

#define LINE_EMERGENCY_1 LINE_GPIO13  // Front left wheel lift (Hall)
#define LINE_EMERGENCY_2 LINE_GPIO12  // Front right wheel lift (Hall)
#define LINE_EMERGENCY_3 LINE_GPIO11  // Top stop button (Hall)
#define LINE_EMERGENCY_4 LINE_GPIO10  // Back-handle stop (Capacitive)

// CoverUI
#define SPI_UI SPID1                   // CoverUI is wired to SPI1
#define LINE_UI_SCK LINE_SPI1_SCK      // Clock for HEF4794BT & 74HC165
#define LINE_UI_MISO LINE_SPI1_MISO    // Button from 74HC165
#define LINE_UI_MOSI LINE_SPI1_MOSI    // LED & Button matrix to HEF4794BT
#define LINE_UI_LATCH_LOAD LINE_GPIO9  // HEF4794BT STR or 74HC165 /PL
#define LINE_UI_LED_OE LINE_GPIO8      // HEF4794BT OE
#define LINE_UI_BTN_CE LINE_GPIO1      // 74HC165 /CE

#endif  // ROBOT_EX_HPP
