#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
g++ -std=gnu++17 -O2 -Wall -Wextra \
    -I ../../src/drivers/sound \
    main.cpp ../../src/drivers/sound/sound_synth.cpp \
    -o sound_synth_tester
echo "built: $(pwd)/sound_synth_tester"
