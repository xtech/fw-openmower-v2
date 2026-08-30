#!/bin/bash
set -euo pipefail

PRESET=${1:-Release}

mkdir -p build out

cd build
cmake .. --preset=$PRESET
cd $PRESET
make -j$(nproc)

cp -v openmower-firmware.elf ../../out/openmower-firmware.elf
cp -v openmower-firmware.bin ../../out/openmower-firmware.bin
