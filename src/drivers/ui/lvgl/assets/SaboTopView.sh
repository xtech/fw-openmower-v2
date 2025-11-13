#!/bin/bash
#
# Run this script to convert the specified 1 bpp image to LVGL .c file
#
IMAGE_NAME="SaboTopView"

./LVGLImage.py --ofmt C --cf I1 -o . --name "${IMAGE_NAME}" "${IMAGE_NAME}.png"
