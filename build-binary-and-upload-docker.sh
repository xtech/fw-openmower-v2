#!/bin/bash
set -euo pipefail

PRESET=${1:-Release}
ROBOT=${2:-YardForce}

mkdir -p build out

cd build
cmake .. --preset=$PRESET -DROBOT_PLATFORM=$ROBOT
cd $PRESET
cmake --build . --target upload -j$(nproc)
