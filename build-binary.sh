#!/bin/bash
set -euo pipefail

PRESET=${1:-Release}

mkdir -p build out

cd build
cmake .. --preset=$PRESET
cd $PRESET
make -j$(nproc)

cp -v openmower.elf ../../out/openmower.elf
cp -v openmower.bin ../../out/openmower.bin
