#!/bin/bash
set -euo pipefail

PRESET=${1:-Release}

mkdir -p build out

cd build
cmake .. --preset=$PRESET
cd $PRESET
cmake --build . --target upload -j$(nproc)
