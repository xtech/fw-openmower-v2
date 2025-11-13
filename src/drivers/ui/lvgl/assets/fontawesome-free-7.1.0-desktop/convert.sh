#!/bin/bash
#
# Install lv_font_conv as described here: https://github.com/lvgl/lv_font_conv
#
# Then run this script to convert the specified FontAwesome OTF characters to LVGL .c file
#
INPUT_FONT="otfs/Font Awesome 7 Free-Solid-900.otf"
FONT_SIZE=14
OUTPUT_DIR="."
OUTPUT_FONT_NAME="font_awesome_${FONT_SIZE}"

# Included characters:
# 0xf544 = robot -> ROS
# 0xf5e1 = car-burst -> Wheel-Lift Emergency
# 0xf071 = exclamation-triangle -> General Emergency
# 0xf601 = location-crosshairs -> GPS
# 0xf244 = battery-empty -> Battery empty
# 0xf243 = battery-quarter -> Battery 1/4
# 0xf242 = battery-half -> Battery 1/2
# 0xf241 = battery-three-quarters -> Battery 3/4
# 0xf240 = battery-full -> Battery full
# 0xf5e7 = charging-station -> Docking
lv_font_conv --font "$INPUT_FONT" --size $FONT_SIZE --bpp 1 --format lvgl \
-r "0xf544,0xf5e1,0xf071,0xf601,0xf244,0xf243,0xf242,0xf241,0xf240,0xf5e7" \
-o "${OUTPUT_FONT_NAME}.c" --lv-font-name "${OUTPUT_FONT_NAME}"
