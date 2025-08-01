/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --bpp 1 --size 16 --no-compress --stride 1 --align 1 --font Orbitron-Bold.ttf --range 32-127 --format lvgl -o
 *orbitron_16b.c
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

#ifndef ORBITRON_16B
#define ORBITRON_16B 1
#endif

#if ORBITRON_16B

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0x3c,

    /* U+0022 "\"" */
    0xde, 0xf6,

    /* U+0023 "#" */
    0xc, 0x60, 0xc6, 0x7f, 0xf7, 0xff, 0x18, 0xc1, 0x98, 0xff, 0xef, 0xfe, 0x33, 0x6, 0x30, 0x63, 0x0,

    /* U+0024 "$" */
    0x6, 0x0, 0x60, 0x7f, 0xef, 0xff, 0xc6, 0x3c, 0x60, 0xff, 0xe7, 0xff, 0x6, 0x30, 0x63, 0xc6, 0x3f, 0xff, 0x7f, 0xe0,
    0x60,

    /* U+0025 "%" */
    0x78, 0x1b, 0xf0, 0xec, 0xc7, 0x3f, 0x38, 0x79, 0xc0, 0x1e, 0x0, 0xf7, 0x87, 0x3f, 0x38, 0xcc, 0xc3, 0xf2, 0x7,
    0x80,

    /* U+0026 "&" */
    0x3f, 0xc1, 0xff, 0x6, 0x6, 0x18, 0x0, 0x70, 0x3, 0xf0, 0xc, 0xf6, 0x30, 0xf8, 0xc0, 0xf3, 0xff, 0xf7, 0xfc, 0xc0,

    /* U+0027 "'" */
    0xfc,

    /* U+0028 "(" */
    0x7f, 0x6d, 0xb6, 0xdd, 0x80,

    /* U+0029 ")" */
    0xdd, 0xb6, 0xdb, 0x7f, 0x0,

    /* U+002A "*" */
    0x18, 0x5a, 0x7e, 0x7e, 0x3c, 0x7e, 0x0,

    /* U+002B "+" */
    0x30, 0x63, 0xff, 0xf3, 0x6, 0x0,

    /* U+002C "," */
    0xfe,

    /* U+002D "-" */
    0xff, 0xfc,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x1, 0x3, 0x7, 0xe, 0xc, 0x18, 0x30, 0x70, 0xe0, 0xc0, 0x80,

    /* U+0030 "0" */
    0x7f, 0xdf, 0xff, 0x7, 0xe1, 0xfc, 0x77, 0x9c, 0xf7, 0x1f, 0xc3, 0xf0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+0031 "1" */
    0x1c, 0xf3, 0xdb, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc0,

    /* U+0032 "2" */
    0x7f, 0xdf, 0xff, 0x1, 0x80, 0x30, 0x6, 0xff, 0xff, 0xf6, 0x0, 0xc0, 0x1f, 0xff, 0xff, 0x80,

    /* U+0033 "3" */
    0x7f, 0xcf, 0xfe, 0xc0, 0x60, 0x6, 0x3f, 0xe3, 0xfe, 0x0, 0x30, 0x3, 0xc0, 0x3f, 0xff, 0x7f, 0xe0,

    /* U+0034 "4" */
    0x1, 0x80, 0xf0, 0x3e, 0xe, 0xc3, 0x98, 0xe3, 0x3f, 0xff, 0xff, 0x1, 0x80, 0x30, 0x6, 0x0,

    /* U+0035 "5" */
    0xff, 0xff, 0xff, 0x0, 0x60, 0xf, 0xfd, 0xff, 0xc0, 0x18, 0x3, 0xc0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+0036 "6" */
    0x7f, 0x9f, 0xf3, 0x0, 0x60, 0xf, 0xfd, 0xff, 0xf0, 0x1e, 0x3, 0xc0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+0037 "7" */
    0xff, 0xbf, 0xf0, 0xc, 0x3, 0x0, 0xc0, 0x30, 0xc, 0x3, 0x0, 0xc0, 0x30, 0xc,

    /* U+0038 "8" */
    0x7f, 0xdf, 0xff, 0x1, 0xe0, 0x3f, 0xff, 0xff, 0xf0, 0x1e, 0x3, 0xc0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+0039 "9" */
    0x7f, 0xdf, 0xff, 0x1, 0xe0, 0x3c, 0x7, 0xff, 0xdf, 0xf8, 0x3, 0x0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+003A ":" */
    0xf0, 0x3, 0xc0,

    /* U+003B ";" */
    0xf0, 0x3, 0xf8,

    /* U+003C "<" */
    0x6, 0x3c, 0xf7, 0x8e, 0x1e, 0xf, 0x7, 0x6,

    /* U+003D "=" */
    0xff, 0xff, 0x0, 0xff, 0xff,

    /* U+003E ">" */
    0x81, 0xc1, 0xe1, 0xf0, 0xe7, 0xbe, 0x70, 0x80,

    /* U+003F "?" */
    0xff, 0xbf, 0xf0, 0xc, 0x3, 0x0, 0xc7, 0xf3, 0xf8, 0xc0, 0x0, 0xc, 0x3, 0x0,

    /* U+0040 "@" */
    0x7f, 0xdf, 0xff, 0x1, 0xe7, 0xbd, 0xff, 0xb3, 0xf7, 0xfe, 0x7f, 0xc0, 0x1f, 0xfd, 0xff, 0x80,

    /* U+0041 "A" */
    0x7f, 0xdf, 0xff, 0x1, 0xe0, 0x3c, 0x7, 0xff, 0xff, 0xfe, 0x3, 0xc0, 0x78, 0xf, 0x1, 0x80,

    /* U+0042 "B" */
    0xff, 0xdf, 0xff, 0x1, 0xe0, 0x3f, 0xff, 0xff, 0xf0, 0x1e, 0x3, 0xc0, 0x7f, 0xff, 0xff, 0x0,

    /* U+0043 "C" */
    0x7f, 0xff, 0xff, 0xc0, 0xc, 0x0, 0xc0, 0xc, 0x0, 0xc0, 0xc, 0x0, 0xc0, 0xf, 0xff, 0x7f, 0xf0,

    /* U+0044 "D" */
    0xff, 0xdf, 0xff, 0x1, 0xe0, 0x3c, 0x7, 0x80, 0xf0, 0x1e, 0x3, 0xc0, 0x7f, 0xff, 0xff, 0x0,

    /* U+0045 "E" */
    0xff, 0xff, 0xff, 0x0, 0x60, 0xf, 0xf9, 0xff, 0x30, 0x6, 0x0, 0xc0, 0x1f, 0xff, 0xff, 0x80,

    /* U+0046 "F" */
    0xff, 0xff, 0xff, 0x0, 0x60, 0xf, 0xf9, 0xff, 0x30, 0x6, 0x0, 0xc0, 0x18, 0x3, 0x0, 0x0,

    /* U+0047 "G" */
    0x7f, 0xdf, 0xff, 0x1, 0xe0, 0xc, 0x1, 0x87, 0xf0, 0xfe, 0x3, 0xc0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+0048 "H" */
    0xc0, 0x3c, 0x3, 0xc0, 0x3c, 0x3, 0xff, 0xff, 0xff, 0xc0, 0x3c, 0x3, 0xc0, 0x3c, 0x3, 0xc0, 0x30,

    /* U+0049 "I" */
    0xff, 0xff, 0xfc,

    /* U+004A "J" */
    0x0, 0x60, 0xc, 0x1, 0x80, 0x30, 0x6, 0x0, 0xc0, 0x18, 0x3, 0xc0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+004B "K" */
    0xc0, 0xf8, 0x3b, 0xe, 0x63, 0x8f, 0xe1, 0xfc, 0x31, 0xc6, 0x1c, 0xc1, 0xd8, 0x1b, 0x3, 0x80,

    /* U+004C "L" */
    0xc0, 0xc, 0x0, 0xc0, 0xc, 0x0, 0xc0, 0xc, 0x0, 0xc0, 0xc, 0x0, 0xc0, 0xf, 0xff, 0xff, 0xf0,

    /* U+004D "M" */
    0xe0, 0x3f, 0x83, 0xfc, 0x1f, 0xf1, 0xfd, 0xdd, 0xe7, 0xcf, 0x1c, 0x78, 0x43, 0xc0, 0x1e, 0x0, 0xf0, 0x6,

    /* U+004E "N" */
    0xe0, 0x7c, 0xf, 0xc1, 0xfc, 0x3d, 0xc7, 0x9c, 0xf1, 0xde, 0x1f, 0xc1, 0xf8, 0x1f, 0x3, 0x80,

    /* U+004F "O" */
    0x7f, 0xdf, 0xff, 0x1, 0xe0, 0x3c, 0x7, 0x80, 0xf0, 0x1e, 0x3, 0xc0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+0050 "P" */
    0xff, 0xdf, 0xff, 0x1, 0xe0, 0x3c, 0x7, 0xff, 0xff, 0xf6, 0x0, 0xc0, 0x18, 0x3, 0x0, 0x0,

    /* U+0051 "Q" */
    0x7f, 0xc7, 0xff, 0x30, 0x19, 0x80, 0xcc, 0x6, 0x60, 0x33, 0x1, 0x98, 0xc, 0xc0, 0x67, 0xff, 0xdf, 0xfe,

    /* U+0052 "R" */
    0xff, 0xdf, 0xff, 0x1, 0xe0, 0x3c, 0x7, 0xff, 0xff, 0xf6, 0x1c, 0xc1, 0x98, 0x1b, 0x3, 0x80,

    /* U+0053 "S" */
    0x7f, 0xdf, 0xff, 0x1, 0xe0, 0xf, 0xfc, 0xff, 0xc0, 0x18, 0x3, 0xc0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+0054 "T" */
    0xff, 0xff, 0xff, 0x6, 0x0, 0x60, 0x6, 0x0, 0x60, 0x6, 0x0, 0x60, 0x6, 0x0, 0x60, 0x6, 0x0,

    /* U+0055 "U" */
    0xc0, 0x78, 0xf, 0x1, 0xe0, 0x3c, 0x7, 0x80, 0xf0, 0x1e, 0x3, 0xc0, 0x7f, 0xfd, 0xff, 0x0,

    /* U+0056 "V" */
    0xc0, 0xf, 0xc0, 0x39, 0xc0, 0x61, 0x81, 0xc3, 0x83, 0x3, 0xc, 0x7, 0x38, 0x7, 0x60, 0x7, 0xc0, 0xf, 0x0, 0xc, 0x0,

    /* U+0057 "W" */
    0xc1, 0xc1, 0xe0, 0xe1, 0xd8, 0xf0, 0xcc, 0x7c, 0x67, 0x36, 0x71, 0xbb, 0xb0, 0xd8, 0xf8, 0x7c, 0x78, 0x1c, 0x3c,
    0xe, 0xe, 0x3, 0x6, 0x0,

    /* U+0058 "X" */
    0xe0, 0xec, 0x18, 0xc6, 0x1d, 0xc1, 0xf0, 0x1c, 0x7, 0xc1, 0xdc, 0x31, 0x8c, 0x1b, 0x83, 0x80,

    /* U+0059 "Y" */
    0xe0, 0x76, 0x6, 0x30, 0xc3, 0x9c, 0x1f, 0x80, 0xf0, 0x6, 0x0, 0x60, 0x6, 0x0, 0x60, 0x6, 0x0,

    /* U+005A "Z" */
    0xff, 0xff, 0xff, 0x0, 0xf0, 0x1c, 0x3, 0x80, 0xf0, 0x1c, 0x3, 0x80, 0xf0, 0xf, 0xff, 0xff, 0xf0,

    /* U+005B "[" */
    0xff, 0x6d, 0xb6, 0xdf, 0x80,

    /* U+005C "\\" */
    0x80, 0xc0, 0xe0, 0x70, 0x30, 0x18, 0xc, 0xe, 0x7, 0x3, 0x1,

    /* U+005D "]" */
    0xfd, 0xb6, 0xdb, 0x7f, 0x80,

    /* U+005F "_" */
    0xff, 0xff, 0xfc,

    /* U+0060 "`" */
    0xd9, 0x80,

    /* U+0061 "a" */
    0xff, 0x7f, 0xc0, 0x7f, 0xff, 0xfe, 0xf, 0x7, 0xff, 0x7f, 0x80,

    /* U+0062 "b" */
    0xc0, 0x60, 0x30, 0x1f, 0xef, 0xfe, 0xf, 0x7, 0x83, 0xc1, 0xe0, 0xff, 0xff, 0xe0,

    /* U+0063 "c" */
    0x7f, 0xff, 0xfc, 0x3, 0x0, 0xc0, 0x30, 0xc, 0x3, 0xff, 0x7f, 0xc0,

    /* U+0064 "d" */
    0x1, 0x80, 0xc0, 0x6f, 0xff, 0xfe, 0xf, 0x7, 0x83, 0xc1, 0xe0, 0xff, 0xef, 0xf0,

    /* U+0065 "e" */
    0x7f, 0x7f, 0xf0, 0x78, 0x3f, 0xff, 0xff, 0x1, 0xff, 0x7f, 0x80,

    /* U+0066 "f" */
    0x7f, 0xfc, 0x3f, 0xff, 0xc, 0x30, 0xc3, 0xc, 0x30,

    /* U+0067 "g" */
    0x7f, 0x7f, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7, 0xff, 0x7f, 0x80, 0xc0, 0x6f, 0xf7, 0xf0,

    /* U+0068 "h" */
    0xc0, 0x60, 0x30, 0x1f, 0xef, 0xfe, 0xf, 0x7, 0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x30,

    /* U+0069 "i" */
    0xf3, 0xff, 0xff,

    /* U+006A "j" */
    0xc, 0x30, 0x3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0x3f, 0xfe,

    /* U+006B "k" */
    0xc0, 0x30, 0xc, 0x3, 0x7, 0xc3, 0xb1, 0xcc, 0xe3, 0xf0, 0xfc, 0x33, 0x8c, 0x73, 0x7,

    /* U+006C "l" */
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xf7,

    /* U+006D "m" */
    0xff, 0xfb, 0xff, 0xfc, 0x30, 0xf0, 0xc3, 0xc3, 0xf, 0xc, 0x3c, 0x30, 0xf0, 0xc3, 0xc3, 0xc,

    /* U+006E "n" */
    0xff, 0x7f, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7, 0x83, 0xc1, 0x80,

    /* U+006F "o" */
    0x7f, 0x7f, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7, 0xff, 0x7f, 0x0,

    /* U+0070 "p" */
    0xff, 0x7f, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7, 0xff, 0xff, 0x60, 0x30, 0x18, 0xc, 0x0,

    /* U+0071 "q" */
    0x7f, 0xff, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7, 0xff, 0x7f, 0x80, 0xc0, 0x60, 0x30, 0x18,

    /* U+0072 "r" */
    0x7f, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,

    /* U+0073 "s" */
    0x7f, 0x7f, 0xf0, 0x18, 0xf, 0xf3, 0xfc, 0x7, 0xff, 0x7f, 0x0,

    /* U+0074 "t" */
    0xc3, 0xc, 0x3f, 0xff, 0xc, 0x30, 0xc3, 0xf, 0xdf,

    /* U+0075 "u" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7, 0xff, 0x7f, 0x0,

    /* U+0076 "v" */
    0x60, 0x37, 0x7, 0x30, 0x63, 0x8e, 0x18, 0xc1, 0xd8, 0xf, 0x80, 0x70, 0x7, 0x0,

    /* U+0077 "w" */
    0xc3, 0x87, 0x87, 0xd, 0x9f, 0x33, 0x36, 0x67, 0xef, 0xc7, 0x8f, 0xf, 0x1e, 0xe, 0x1c, 0x18, 0x30,

    /* U+0078 "x" */
    0xe1, 0xdc, 0xe3, 0xf0, 0x78, 0x1c, 0x7, 0x83, 0xf1, 0xce, 0xe1, 0xc0,

    /* U+0079 "y" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7, 0xff, 0x7f, 0x80, 0xc0, 0x6f, 0xf7, 0xf0,

    /* U+007A "z" */
    0xff, 0xff, 0xc1, 0xe1, 0xc1, 0xc1, 0xc3, 0xc1, 0xff, 0xff, 0x80,

    /* U+007B "{" */
    0x37, 0x66, 0xec, 0x66, 0x67, 0x30,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+007D "}" */
    0xce, 0x66, 0x73, 0x66, 0x6e, 0xc0,

    /* U+007E "~" */
    0xe3, 0xf1, 0xc0};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 78, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 56, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 100, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 6, .adv_w = 204, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 202, .box_w = 12, .box_h = 14, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 44, .adv_w = 247, .box_w = 14, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 240, .box_w = 14, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 84, .adv_w = 62, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 85, .adv_w = 74, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 74, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 95, .adv_w = 130, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 102, .adv_w = 115, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 108, .adv_w = 58, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 109, .adv_w = 132, .box_w = 7, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 111, .adv_w = 58, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 133, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 214, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 100, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 212, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 164, .adv_w = 211, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 187, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 197, .adv_w = 212, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 210, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 229, .adv_w = 169, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 243, .adv_w = 214, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 259, .adv_w = 212, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 275, .adv_w = 61, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 62, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 281, .adv_w = 121, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 163, .box_w = 8, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 294, .adv_w = 122, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 302, .adv_w = 174, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 316, .adv_w = 210, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 332, .adv_w = 214, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 348, .adv_w = 213, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 364, .adv_w = 210, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 214, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 397, .adv_w = 196, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 413, .adv_w = 185, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 429, .adv_w = 212, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 445, .adv_w = 218, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 462, .adv_w = 55, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 465, .adv_w = 200, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 481, .adv_w = 204, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 497, .adv_w = 199, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 514, .adv_w = 238, .box_w = 13, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 532, .adv_w = 213, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 548, .adv_w = 212, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 564, .adv_w = 202, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 580, .adv_w = 226, .box_w = 13, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 598, .adv_w = 211, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 614, .adv_w = 211, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 630, .adv_w = 194, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 647, .adv_w = 212, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 663, .adv_w = 257, .box_w = 15, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 684, .adv_w = 302, .box_w = 17, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 708, .adv_w = 208, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 724, .adv_w = 206, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 741, .adv_w = 210, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 758, .adv_w = 70, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 763, .adv_w = 133, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 774, .adv_w = 71, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 779, .adv_w = 212, .box_w = 11, .box_h = 2, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 782, .adv_w = 73, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 10},
    {.bitmap_index = 784, .adv_w = 178, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 795, .adv_w = 171, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 809, .adv_w = 178, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 821, .adv_w = 171, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 835, .adv_w = 177, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 846, .adv_w = 110, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 855, .adv_w = 175, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 870, .adv_w = 171, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 884, .adv_w = 57, .box_w = 2, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 887, .adv_w = 61, .box_w = 6, .box_h = 16, .ofs_x = -3, .ofs_y = -4},
    {.bitmap_index = 899, .adv_w = 165, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 914, .adv_w = 83, .box_w = 4, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 920, .adv_w = 250, .box_w = 14, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 936, .adv_w = 178, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 947, .adv_w = 177, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 958, .adv_w = 170, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 973, .adv_w = 170, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 988, .adv_w = 134, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 997, .adv_w = 176, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1008, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1017, .adv_w = 178, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1028, .adv_w = 202, .box_w = 12, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1042, .adv_w = 271, .box_w = 15, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1059, .adv_w = 177, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1071, .adv_w = 175, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1086, .adv_w = 179, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1097, .adv_w = 74, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1103, .adv_w = 55, .box_w = 2, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1107, .adv_w = 74, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1113, .adv_w = 103, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 3}};

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
    9,  53, 34, 56, 34, 58, 35, 55, 35, 58, 35, 72, 36, 69, 37, 46, 37, 55, 37, 56, 37, 59, 37, 73, 38, 46, 38, 48,
    39, 15, 39, 43, 39, 51, 39, 53, 39, 89, 40, 56, 40, 58, 40, 68, 42, 43, 42, 44, 42, 53, 42, 65, 42, 66, 42, 78,
    42, 79, 42, 80, 42, 81, 42, 82, 42, 83, 42, 86, 42, 87, 42, 88, 43, 69, 44, 34, 44, 41, 44, 65, 44, 79, 44, 89,
    45, 55, 45, 56, 45, 58, 45, 76, 46, 65, 46, 67, 47, 69, 48, 55, 48, 56, 48, 57, 48, 67, 48, 71, 48, 90, 49, 34,
    49, 43, 49, 68, 51, 55, 51, 56, 52, 47, 52, 56, 52, 58, 52, 87, 53, 79, 53, 83, 53, 85, 53, 87, 53, 89, 53, 90,
    54, 83, 54, 86, 54, 87, 54, 88, 55, 34, 55, 48, 55, 52, 55, 69, 55, 73, 55, 79, 55, 83, 56, 34, 56, 48, 56, 65,
    56, 69, 56, 72, 56, 73, 56, 79, 56, 85, 56, 89, 57, 35, 57, 65, 57, 69, 58, 52, 58, 65, 58, 69, 58, 73, 58, 79,
    58, 80, 58, 83, 58, 85, 59, 58, 59, 69, 65, 66, 65, 67, 65, 71, 65, 72, 65, 75, 65, 76, 65, 77, 65, 78, 65, 82,
    65, 83, 65, 86, 65, 87, 65, 88, 65, 90, 66, 65, 66, 68, 66, 85, 66, 86, 66, 87, 67, 42, 67, 65, 67, 66, 67, 67,
    67, 69, 67, 72, 67, 75, 67, 79, 67, 82, 67, 84, 67, 85, 67, 90, 68, 69, 68, 79, 68, 82, 68, 85, 68, 87, 68, 89,
    69, 67, 69, 69, 69, 71, 69, 72, 69, 74, 69, 76, 69, 79, 69, 82, 69, 83, 69, 86, 69, 87, 69, 88, 69, 89, 70, 13,
    70, 65, 70, 68, 70, 72, 70, 74, 70, 76, 70, 78, 70, 79, 70, 84, 70, 85, 71, 65, 71, 67, 71, 69, 71, 70, 71, 71,
    71, 77, 71, 79, 71, 82, 71, 83, 71, 85, 71, 87, 71, 89, 71, 90, 72, 65, 72, 69, 72, 70, 72, 73, 73, 65, 73, 67,
    73, 68, 73, 69, 73, 70, 73, 72, 73, 75, 73, 76, 73, 78, 73, 80, 73, 81, 73, 82, 73, 83, 73, 84, 73, 86, 73, 87,
    73, 88, 73, 90, 74, 65, 74, 69, 74, 74, 74, 85, 75, 69, 75, 73, 76, 65, 76, 66, 76, 68, 76, 69, 76, 70, 76, 73,
    76, 76, 76, 77, 76, 78, 76, 79, 76, 85, 76, 86, 76, 87, 76, 89, 76, 90, 77, 65, 77, 66, 77, 67, 77, 69, 77, 71,
    77, 75, 77, 77, 77, 78, 77, 80, 77, 87, 78, 66, 78, 67, 78, 69, 78, 71, 78, 79, 78, 80, 78, 83, 78, 88, 79, 65,
    79, 66, 79, 76, 79, 78, 79, 79, 79, 82, 79, 83, 79, 84, 79, 85, 79, 86, 79, 87, 79, 88, 80, 68, 80, 73, 80, 74,
    80, 83, 81, 85, 82, 13, 82, 15, 82, 65, 82, 66, 82, 68, 82, 69, 82, 70, 82, 71, 82, 72, 82, 73, 82, 76, 82, 78,
    82, 79, 82, 83, 82, 85, 82, 86, 82, 90, 83, 67, 83, 69, 83, 71, 83, 75, 83, 78, 83, 79, 83, 85, 83, 86, 83, 87,
    83, 88, 83, 90, 84, 15, 84, 65, 84, 66, 84, 68, 84, 72, 84, 73, 84, 77, 84, 82, 84, 83, 84, 84, 84, 86, 84, 89,
    84, 90, 85, 66, 85, 67, 85, 68, 85, 69, 85, 70, 85, 71, 85, 75, 85, 77, 85, 78, 85, 79, 85, 80, 85, 83, 85, 84,
    85, 87, 85, 88, 85, 90, 86, 65, 86, 69, 86, 73, 86, 79, 86, 83, 86, 86, 87, 65, 87, 67, 87, 69, 87, 71, 87, 72,
    87, 73, 87, 76, 87, 78, 87, 79, 87, 82, 87, 83, 87, 84, 87, 85, 87, 89, 88, 69, 88, 85, 89, 65, 89, 67, 89, 69,
    89, 75, 89, 76, 89, 78, 89, 79, 89, 80, 89, 82, 89, 83, 89, 87, 90, 65, 90, 69, 90, 72, 90, 76, 90, 84, 90, 90};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] = {
    2,   -9, -5, -14, -10, -2,  -2,  -4,  -9,  -5,  -7,  -2,  -6,  -5,  -10, -53, 1,   3,   -2,  -8,  -3,  2,
    5,   2,  4,  4,   2,   4,   2,   4,   4,   2,   2,   8,   2,   4,   -2,  -6,  -6,  -2,  -5,  -2,  -54, -30,
    -43, 2,  -2, -4,  -2,  -8,  -8,  -8,  -4,  -2,  -4,  1,   -20, 1,   -7,  -7,  -7,  -8,  -1,  -5,  -14, -13,
    -8,  -7, -7, -8,  -2,  -4,  -2,  -2,  -5,  -3,  -2,  -9,  2,   -11, -13, -11, -10, -12, -17, -5,  -8,  -14,
    -12, -4, -7, -4,  -4,  -3,  -11, -18, -2,  -18, -11, -18, -11, -1,  -5,  -7,  -7,  -2,  -2,  -2,  -6,  -7,
    -3,  -5, -6, -8,  -10, -5,  -7,  -2,  2,   -2,  -2,  -4,  -4,  -2,  -2,  -2,  -6,  -6,  -7,  -2,  -2,  -7,
    -6,  -7, -6, -6,  -2,  -6,  -2,  -4,  -6,  -6,  -2,  -2,  2,   -6,  -4,  -2,  -2,  -12, -10, -5,  -2,  -32,
    2,   2,  2,  4,   4,   4,   2,   6,   3,   -6,  -5,  -5,  -2,  -4,  -2,  -6,  -6,  -5,  -6,  -9,  -4,  -2,
    1,   0,  1,  1,   -3,  1,   4,   2,   0,   4,   2,   5,   6,   2,   8,   5,   7,   4,   1,   4,   0,   2,
    2,   -2, 7,  2,   -1,  2,   1,   1,   6,   1,   2,   5,   2,   -1,  2,   1,   2,   -5,  -5,  4,   2,   -5,
    -5,  -7, -7, -8,  -11, -11, -2,  -10, -14, -7,  -6,  -5,  -2,  -6,  -6,  -4,  -6,  -6,  -2,  -6,  -6,  -6,
    -6,  -6, -5, -7,  -6,  -15, -7,  2,   1,   2,   1,   -5,  -47, -2,  4,   2,   5,   2,   4,   2,   2,   4,
    2,   4,  1,  4,   4,   2,   2,   -5,  -5,  -2,  -6,  -6,  -4,  -7,  -9,  -5,  -7,  -6,  2,   5,   2,   10,
    4,   1,  5,  2,   2,   2,   1,   2,   4,   -6,  -6,  1,   -6,  -5,  -2,  -2,  -6,  1,   -2,  -6,  -5,  -6,
    -2,  -5, -6, -6,  -8,  2,   -4,  -2,  2,   -5,  -5,  -8,  -9,  -4,  -4,  -2,  -5,  -12, -12, -11, -1,  -4,
    -4,  -8, -4, -6,  -6,  -6,  -4,  -1,  -6,  -6,  -2,  -4,  -2,  -2,  -7,  -6,  -2,  -6,  -6,  -7};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs = {
    .glyph_ids = kern_pair_glyph_ids, .values = kern_pair_values, .pair_cnt = 350, .glyph_ids_size = 0};

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
const lv_font_t orbitron_16b = {
#else
lv_font_t orbitron_16b = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt, /*Function pointer to get glyph's bitmap*/
    .line_height = 17,                              /*The maximum line height required by the font*/
    .base_line = 4,                                 /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .static_bitmap = 0,
    .dsc = &font_dsc, /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};

#endif /*#if ORBITRON_16B*/
