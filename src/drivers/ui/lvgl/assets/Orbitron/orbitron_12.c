/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --bpp 1 --size 12 --no-compress --stride 1 --align 1 --font Orbitron-VariableFont_wght_MOD.ttf --range 32-127
 *--format lvgl -o orbitron_12.c
 ******************************************************************************/

#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef ORBITRON_12
#define ORBITRON_12 1
#endif

#if ORBITRON_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xfc, 0x80,

    /* U+0022 "\"" */
    0xa0,

    /* U+0023 "#" */
    0x8, 0x88, 0x9f, 0xe2, 0x22, 0x21, 0x13, 0xfe, 0x84, 0x44, 0x0,

    /* U+0024 "$" */
    0x8, 0x7f, 0xe2, 0x31, 0x8, 0x87, 0xfc, 0x22, 0x11, 0x88, 0xff, 0xc2, 0x1, 0x0,

    /* U+0025 "%" */
    0xf0, 0x52, 0x1a, 0x46, 0x79, 0x80, 0x60, 0x17, 0xcc, 0x8b, 0x11, 0x43, 0xe0,

    /* U+0026 "&" */
    0xff, 0x10, 0x12, 0x0, 0x60, 0xb, 0x1, 0x19, 0x20, 0xe4, 0x6, 0xff, 0xa0,

    /* U+0027 "'" */
    0x80,

    /* U+0028 "(" */
    0xea, 0xaa, 0xc0,

    /* U+0029 ")" */
    0xd5, 0x55, 0xc0,

    /* U+002A "*" */
    0x25, 0x5c, 0xa5, 0x0,

    /* U+002B "+" */
    0x27, 0xc8, 0x40,

    /* U+002C "," */
    0xc0,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x0, 0x10, 0x84, 0x10, 0x84, 0x20, 0x0,

    /* U+0030 "0" */
    0xff, 0xc0, 0xe0, 0xf0, 0xd8, 0x8d, 0x87, 0x83, 0x81, 0xff, 0x80,

    /* U+0031 "1" */
    0x37, 0x51, 0x11, 0x11, 0x10,

    /* U+0032 "2" */
    0xff, 0xc0, 0x40, 0x20, 0x1f, 0xfc, 0x2, 0x1, 0x0, 0xff, 0x80,

    /* U+0033 "3" */
    0x7f, 0xe0, 0x40, 0x20, 0x13, 0xf8, 0x4, 0x3, 0x81, 0x7f, 0x80,

    /* U+0034 "4" */
    0x6, 0xe, 0x1a, 0x32, 0x62, 0xc2, 0xff, 0x2, 0x2,

    /* U+0035 "5" */
    0xff, 0xc0, 0x20, 0x10, 0xf, 0xf8, 0x4, 0x3, 0x1, 0xff, 0x80,

    /* U+0036 "6" */
    0xfe, 0x40, 0x20, 0x10, 0xf, 0xfc, 0x6, 0x3, 0x1, 0xff, 0x80,

    /* U+0037 "7" */
    0xfe, 0x4, 0x8, 0x10, 0x20, 0x40, 0x81, 0x2,

    /* U+0038 "8" */
    0xff, 0xc0, 0x60, 0x30, 0x1f, 0xfc, 0x6, 0x3, 0x1, 0xff, 0x80,

    /* U+0039 "9" */
    0xff, 0xc0, 0x60, 0x30, 0x1f, 0xf8, 0x4, 0x2, 0x1, 0xff, 0x80,

    /* U+003A ":" */
    0x82,

    /* U+003B ";" */
    0x83, 0x0,

    /* U+003C "<" */
    0x9, 0x99, 0x6, 0xc, 0x20,

    /* U+003D "=" */
    0xfc, 0xf, 0xc0,

    /* U+003E ">" */
    0x87, 0xc, 0x13, 0x62, 0x0,

    /* U+003F "?" */
    0xff, 0x1, 0x1, 0x1, 0x1, 0x3f, 0x20, 0x0, 0x20,

    /* U+0040 "@" */
    0xff, 0xc0, 0x60, 0x37, 0xda, 0x2d, 0x16, 0xff, 0x0, 0xff, 0x80,

    /* U+0041 "A" */
    0xff, 0xc0, 0x60, 0x30, 0x18, 0xf, 0xfe, 0x3, 0x1, 0x80, 0x80,

    /* U+0042 "B" */
    0xff, 0x40, 0xa0, 0x50, 0x2f, 0xf4, 0x6, 0x3, 0x1, 0xff, 0x80,

    /* U+0043 "C" */
    0xff, 0xc0, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1, 0x0, 0xff, 0x80,

    /* U+0044 "D" */
    0xff, 0x40, 0xe0, 0x30, 0x18, 0xc, 0x6, 0x3, 0x3, 0xff, 0x0,

    /* U+0045 "E" */
    0xff, 0x80, 0x80, 0x80, 0xfe, 0x80, 0x80, 0x80, 0xff,

    /* U+0046 "F" */
    0xff, 0x80, 0x80, 0x80, 0xfe, 0x80, 0x80, 0x80, 0x80,

    /* U+0047 "G" */
    0xff, 0xc0, 0x60, 0x10, 0x8, 0x3c, 0x6, 0x3, 0x1, 0xff, 0x80,

    /* U+0048 "H" */
    0x80, 0xc0, 0x60, 0x30, 0x1f, 0xfc, 0x6, 0x3, 0x1, 0x80, 0x80,

    /* U+0049 "I" */
    0xff, 0x80,

    /* U+004A "J" */
    0x0, 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x3, 0x1, 0xff, 0x80,

    /* U+004B "K" */
    0x81, 0x82, 0x84, 0x88, 0xf8, 0x88, 0x84, 0x82, 0x81,

    /* U+004C "L" */
    0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1, 0x0, 0xff, 0x80,

    /* U+004D "M" */
    0xc0, 0xf8, 0x7a, 0x16, 0x49, 0x8c, 0x62, 0x18, 0x6, 0x1, 0x80, 0x40,

    /* U+004E "N" */
    0xc0, 0xf0, 0x6c, 0x32, 0x18, 0x8c, 0x26, 0x1b, 0x7, 0x81, 0x80,

    /* U+004F "O" */
    0xff, 0xc0, 0x60, 0x30, 0x18, 0xc, 0x6, 0x3, 0x1, 0xff, 0x80,

    /* U+0050 "P" */
    0xff, 0xc0, 0x60, 0x30, 0x18, 0xf, 0xfe, 0x1, 0x0, 0x80, 0x0,

    /* U+0051 "Q" */
    0xff, 0xa0, 0x28, 0xa, 0x2, 0x80, 0xa0, 0x28, 0xa, 0x2, 0xff, 0xc0,

    /* U+0052 "R" */
    0xff, 0xc0, 0x60, 0x30, 0x18, 0xf, 0xfe, 0x19, 0x6, 0x81, 0x0,

    /* U+0053 "S" */
    0xff, 0xc0, 0x60, 0x10, 0xf, 0xf8, 0x4, 0x3, 0x1, 0xff, 0x80,

    /* U+0054 "T" */
    0xff, 0x84, 0x2, 0x1, 0x0, 0x80, 0x40, 0x20, 0x10, 0x8, 0x0,

    /* U+0055 "U" */
    0x80, 0xc0, 0x60, 0x30, 0x18, 0xc, 0x6, 0x3, 0x1, 0xff, 0x80,

    /* U+0056 "V" */
    0x40, 0x24, 0x2, 0x20, 0x42, 0xc, 0x10, 0x81, 0x98, 0x9, 0x0, 0x60, 0x6, 0x0,

    /* U+0057 "W" */
    0x43, 0x5, 0xc, 0x24, 0x28, 0x89, 0x26, 0x24, 0x90, 0x91, 0x41, 0x87, 0x6, 0x18, 0x18, 0x20,

    /* U+0058 "X" */
    0x83, 0x42, 0x24, 0x38, 0x10, 0x38, 0x24, 0x42, 0x83,

    /* U+0059 "Y" */
    0xc1, 0xa0, 0x88, 0x86, 0xc1, 0xc0, 0x40, 0x20, 0x10, 0x8, 0x0,

    /* U+005A "Z" */
    0xff, 0x80, 0x80, 0x80, 0x81, 0x81, 0x81, 0x81, 0x80, 0xff, 0x80,

    /* U+005B "[" */
    0xea, 0xaa, 0xc0,

    /* U+005C "\\" */
    0x2, 0x4, 0x8, 0x20, 0x40, 0x81, 0x0,

    /* U+005D "]" */
    0xd5, 0x55, 0xc0,

    /* U+005F "_" */
    0xff,

    /* U+0060 "`" */
    0x80,

    /* U+0061 "a" */
    0xfe, 0x4, 0xf, 0xf8, 0x30, 0x7f, 0x80,

    /* U+0062 "b" */
    0x81, 0x3, 0xfc, 0x18, 0x30, 0x60, 0xc1, 0xfe,

    /* U+0063 "c" */
    0xff, 0x2, 0x4, 0x8, 0x10, 0x3f, 0x80,

    /* U+0064 "d" */
    0x2, 0x7, 0xfc, 0x18, 0x30, 0x60, 0xc1, 0xfe,

    /* U+0065 "e" */
    0xff, 0x6, 0xf, 0xf8, 0x10, 0x3f, 0x80,

    /* U+0066 "f" */
    0xf8, 0xf8, 0x88, 0x88, 0x80,

    /* U+0067 "g" */
    0xff, 0x6, 0xc, 0x18, 0x30, 0x7f, 0x81, 0x2, 0xfc,

    /* U+0068 "h" */
    0x81, 0x3, 0xfc, 0x18, 0x30, 0x60, 0xc1, 0x82,

    /* U+0069 "i" */
    0xbf, 0x80,

    /* U+006A "j" */
    0x8, 0x2, 0x10, 0x84, 0x21, 0x8, 0x43, 0xf0,

    /* U+006B "k" */
    0x81, 0x2, 0x1c, 0x69, 0x9e, 0x26, 0x46, 0x86,

    /* U+006C "l" */
    0x92, 0x49, 0x24, 0xe0,

    /* U+006D "m" */
    0xff, 0xf0, 0x86, 0x10, 0xc2, 0x18, 0x43, 0x8, 0x61, 0x8,

    /* U+006E "n" */
    0xff, 0x6, 0xc, 0x18, 0x30, 0x60, 0x80,

    /* U+006F "o" */
    0xff, 0x6, 0xc, 0x18, 0x30, 0x7f, 0x80,

    /* U+0070 "p" */
    0xff, 0x6, 0xc, 0x18, 0x30, 0x7f, 0xc0, 0x81, 0x0,

    /* U+0071 "q" */
    0xff, 0x6, 0xc, 0x18, 0x30, 0x7f, 0x81, 0x2, 0x4,

    /* U+0072 "r" */
    0xfc, 0x21, 0x8, 0x42, 0x0,

    /* U+0073 "s" */
    0xff, 0x6, 0x7, 0xf0, 0x30, 0x7f, 0x80,

    /* U+0074 "t" */
    0x88, 0xf8, 0x88, 0x88, 0xf0,

    /* U+0075 "u" */
    0x83, 0x6, 0xc, 0x18, 0x30, 0x7f, 0x80,

    /* U+0076 "v" */
    0x40, 0xa0, 0x88, 0x44, 0x41, 0x60, 0xa0, 0x30,

    /* U+0077 "w" */
    0x42, 0x12, 0x28, 0x91, 0x44, 0x5b, 0x42, 0x8a, 0xc, 0x60, 0x41, 0x0,

    /* U+0078 "x" */
    0x84, 0x90, 0xe1, 0x83, 0x89, 0x21, 0x0,

    /* U+0079 "y" */
    0x83, 0x6, 0xc, 0x18, 0x30, 0x7f, 0x81, 0x2, 0xfc,

    /* U+007A "z" */
    0xfe, 0xc, 0x30, 0x86, 0x18, 0x3f, 0x80,

    /* U+007B "{" */
    0x69, 0x28, 0x92, 0x60,

    /* U+007C "|" */
    0xff, 0xf0,

    /* U+007D "}" */
    0xc9, 0x22, 0x92, 0xc0,

    /* U+007E "~" */
    0xc3};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 52, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 42, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 71, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 4, .adv_w = 153, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 15, .adv_w = 151, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 29, .adv_w = 185, .box_w = 11, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 42, .adv_w = 180, .box_w = 11, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 55, .adv_w = 43, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 56, .adv_w = 53, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 59, .adv_w = 53, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 94, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 66, .adv_w = 83, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 69, .adv_w = 37, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 70, .adv_w = 99, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 71, .adv_w = 41, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 100, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 79, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 75, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 95, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 140, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 137, .adv_w = 157, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 127, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 167, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 41, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 37, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 181, .adv_w = 91, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 186, .adv_w = 122, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 189, .adv_w = 91, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 194, .adv_w = 130, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 161, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 158, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 247, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 258, .adv_w = 147, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 267, .adv_w = 139, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 276, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 287, .adv_w = 163, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 41, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 150, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 153, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 150, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 331, .adv_w = 178, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 354, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 365, .adv_w = 152, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 376, .adv_w = 170, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 388, .adv_w = 158, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 399, .adv_w = 158, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 146, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 421, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 432, .adv_w = 193, .box_w = 12, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 446, .adv_w = 226, .box_w = 14, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 462, .adv_w = 156, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 155, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 482, .adv_w = 158, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 493, .adv_w = 53, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 496, .adv_w = 100, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 503, .adv_w = 53, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 506, .adv_w = 159, .box_w = 8, .box_h = 1, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 507, .adv_w = 60, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 508, .adv_w = 133, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 515, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 523, .adv_w = 133, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 538, .adv_w = 133, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 545, .adv_w = 78, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 550, .adv_w = 131, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 559, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 567, .adv_w = 40, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 569, .adv_w = 46, .box_w = 5, .box_h = 12, .ofs_x = -3, .ofs_y = -3},
    {.bitmap_index = 577, .adv_w = 124, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 585, .adv_w = 58, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 589, .adv_w = 188, .box_w = 11, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 599, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 606, .adv_w = 133, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 613, .adv_w = 127, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 622, .adv_w = 127, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 631, .adv_w = 98, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 636, .adv_w = 132, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 643, .adv_w = 79, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 648, .adv_w = 133, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 655, .adv_w = 152, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 663, .adv_w = 206, .box_w = 13, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 675, .adv_w = 133, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 682, .adv_w = 132, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 691, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 698, .adv_w = 55, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 702, .adv_w = 41, .box_w = 1, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 704, .adv_w = 55, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 708, .adv_w = 78, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 3}};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] = {{.range_start = 32,
                                                .range_length = 62,
                                                .glyph_id_start = 1,
                                                .unicode_list = NULL,
                                                .glyph_id_ofs_list = NULL,
                                                .list_length = 0,
                                                .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},
                                               {.range_start = 95,
                                                .range_length = 32,
                                                .glyph_id_start = 63,
                                                .unicode_list = NULL,
                                                .glyph_id_ofs_list = NULL,
                                                .list_length = 0,
                                                .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}};

/*-----------------
 *    KERNING
 *----------------*/

/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] = {
    32, 86, 34, 56, 34, 58, 35, 55, 35, 58, 37, 55, 37, 59, 38, 46, 38, 48, 39, 15, 39, 43, 39, 51, 39, 53, 39, 89,
    40, 56, 44, 34, 44, 41, 45, 55, 45, 56, 45, 58, 46, 67, 48, 55, 48, 56, 48, 57, 49, 34, 49, 43, 49, 68, 49, 86,
    51, 55, 51, 56, 52, 47, 52, 56, 52, 58, 53, 79, 53, 83, 53, 85, 53, 87, 53, 89, 53, 90, 55, 34, 55, 48, 55, 52,
    55, 79, 55, 83, 56, 34, 56, 48, 56, 65, 56, 69, 56, 73, 56, 79, 56, 85, 57, 35, 58, 52, 58, 65, 58, 69, 58, 73,
    58, 79, 58, 80, 58, 83, 58, 85, 59, 58, 65, 66, 65, 67, 65, 76, 65, 77, 65, 78, 65, 82, 65, 83, 65, 86, 65, 87,
    65, 88, 65, 90, 66, 68, 67, 69, 67, 72, 67, 75, 67, 84, 67, 85, 67, 90, 68, 69, 68, 79, 68, 85, 69, 67, 69, 69,
    69, 76, 69, 82, 69, 86, 69, 87, 69, 88, 70, 13, 70, 84, 70, 85, 71, 65, 71, 67, 71, 69, 71, 79, 71, 82, 71, 83,
    71, 85, 71, 87, 72, 65, 72, 70, 72, 73, 73, 65, 73, 67, 73, 70, 73, 78, 73, 80, 73, 81, 73, 86, 73, 88, 74, 69,
    75, 69, 76, 65, 76, 68, 76, 69, 76, 77, 76, 79, 76, 86, 76, 87, 77, 75, 77, 77, 77, 87, 78, 66, 78, 67, 78, 79,
    78, 80, 78, 88, 79, 65, 79, 76, 79, 78, 79, 79, 79, 82, 79, 83, 79, 84, 79, 85, 79, 86, 79, 87, 79, 88, 80, 68,
    80, 73, 80, 83, 81, 85, 82, 13, 82, 15, 82, 79, 83, 67, 83, 69, 83, 75, 83, 78, 83, 85, 83, 86, 83, 88, 83, 90,
    84, 66, 84, 68, 84, 72, 84, 73, 84, 90, 85, 66, 85, 67, 85, 68, 85, 69, 85, 70, 85, 77, 85, 78, 85, 80, 85, 83,
    85, 84, 85, 88, 85, 90, 86, 69, 86, 79, 86, 83, 87, 69, 87, 79, 87, 82, 87, 83, 88, 69, 89, 65, 89, 67, 89, 69,
    89, 76, 89, 78, 89, 79, 90, 65, 90, 69, 90, 76, 90, 84, 90, 90};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] = {
    0,  -6,  -2, -9, -6, -6,  -5,  -6, -6,  -25, -46, 1,   2,   -4,  -5,  -6, -6, -44, -25, -34, -2, -5, -5,  -7,
    2,  -36, 2,  0,  -4, -4,  -4,  -5, -2,  -24, -23, -19, -16, -17, -20, -7, -6, -4,  -6,  -8,  -5, -4, -3,  -7,
    -2, -5,  -4, -7, -7, -19, -23, -4, -23, -19, -22, -19, -2,  -5,  -2,  -2, -4, -3,  -1,  -4,  -4, -6, -3,  -5,
    6,  -5,  -3, -4, -5, -4,  -4,  -4, -4,  -4,  -4,  -3,  -4,  -1,  -4,  -6, -3, -35, 1,   2,   -3, -3, -3,  -4,
    -3, -4,  -5, -4, 1,  2,   1,   -7, -3,  -9,  -3,  -5,  -3,  -1,  -8,  -4, -1, -2,  1,   -3,  -2, -2, -11, -12,
    -2, -2,  -5, -4, -4, -3,  -2,  -5, -2,  -4,  -3,  -4,  -2,  -4,  -4,  -4, -2, -7,  -7,  5,   3,  2,  -5,  -47,
    -4, -1,  -4, -3, -3, -4,  -4,  -4, -7,  -4,  1,   4,   1,   1,   1,   -3, -4, 2,   -3,  -2,  -4, 1,  -3,  -3,
    -3, -3,  -3, -6, -6, -5,  -6,  -4, -4,  -3,  -9,  -4,  -4,  -4,  2,   -3, -4, -5,  -5,  -4,  -4, -4};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs = {
    .glyph_ids = kern_pair_glyph_ids, .values = kern_pair_values, .pair_cnt = 190, .glyph_ids_size = 0};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t orbitron_12 = {
#else
lv_font_t orbitron_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt, /*Function pointer to get glyph's bitmap*/
    .line_height = 14,                              /*The maximum line height required by the font*/
    .base_line = 3,                                 /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .static_bitmap = 0,
    .dsc = &font_dsc, /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};

#endif /*#if ORBITRON_12*/
