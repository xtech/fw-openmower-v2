#!/bin/bash
set -euo pipefail

PRESET=${1:-Release}

mkdir -p build out

cd build
cmake .. --preset=$PRESET
cd $PRESET
make -j$(nproc)

cp -v openmower-with-unsafe-bootloader.bin openmower.bin openmower.elf ../../out/
