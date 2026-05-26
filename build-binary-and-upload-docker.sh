#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CACHE_FILE="$SCRIPT_DIR/.build-wizard-cache"

ROBOTS=(YardForce YardForce_V4 Sabo Lyfco_E1600 Universal Universal5S Universal7S Worx xBot)
PRESETS=(Release Debug DebugRTT DebugSystemView)

load_cache() {
    CACHED_ROBOT="" CACHED_PRESET="" CACHED_INTERFACE=""
    if [[ -f "$CACHE_FILE" ]]; then
        # shellcheck source=/dev/null
        source "$CACHE_FILE"
    fi
}

save_cache() {
    printf 'CACHED_ROBOT=%q\nCACHED_PRESET=%q\nCACHED_INTERFACE=%q\n' \
        "$ROBOT" "$PRESET" "$INTERFACE" > "$CACHE_FILE"
}

select_option() {
    local prompt="$1" default="$2"
    shift 2
    local options=("$@")

    echo "$prompt" >&2
    local i=1
    for opt in "${options[@]}"; do
        if [[ "$opt" == "$default" ]]; then
            printf "  %d) %s  [last used]\n" "$i" "$opt" >&2
        else
            printf "  %d) %s\n" "$i" "$opt" >&2
        fi
        ((i++))
    done

    local default_idx=1
    for i in "${!options[@]}"; do
        if [[ "${options[$i]}" == "$default" ]]; then
            default_idx=$((i+1))
            break
        fi
    done

    local choice
    while true; do
        read -rp "Enter number [1-${#options[@]}] (default: $default_idx): " choice
        choice="${choice:-$default_idx}"
        if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= ${#options[@]} )); then
            echo "${options[$((choice-1))]}"
            return
        fi
        echo "Invalid choice, try again." >&2
    done
}

load_cache

# --- Robot ---
ROBOT=$(select_option "Select robot platform:" "$CACHED_ROBOT" "${ROBOTS[@]}")

# --- Preset ---
PRESET=$(select_option "Select build type:" "$CACHED_PRESET" "${PRESETS[@]}")

# --- Interface ---
mapfile -t INTERFACES < <(ip -o link show | awk -F': ' '{print $2}' | grep -v '^lo$' | sort)
INTERFACE=$(select_option "Select upload interface:" "$CACHED_INTERFACE" "${INTERFACES[@]}")

save_cache

echo ""
echo "Building: robot=$ROBOT  preset=$PRESET  interface=$INTERFACE"
echo ""

mkdir -p "$SCRIPT_DIR/build" "$SCRIPT_DIR/out"

cd "$SCRIPT_DIR/build"
cmake .. --preset="$PRESET" -DROBOT_PLATFORM="$ROBOT" -DUPLOAD_INTERFACE="$INTERFACE"
cd "$PRESET"
cmake --build . --target upload -j"$(nproc)"
