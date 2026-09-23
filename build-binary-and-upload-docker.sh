#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CACHE_FILE="$SCRIPT_DIR/.build-wizard-cache"

PRESETS=(Release Debug DebugRTT DebugSystemView RelWithDebInfo MinSizeRel)

usage() {
    cat >&2 <<USAGE
Usage: $(basename "$0") [PRESET] [INTERFACE]

Both arguments are optional; anything not given is asked for interactively
and remembered for next time. Passing both runs without prompting, which is
what a non-interactive shell needs.

  PRESET     one of: ${PRESETS[*]}
  INTERFACE  network interface used to reach the board, e.g. tap0
USAGE
}

case "${1:-}" in
    -h | --help)
        usage
        exit 0
        ;;
esac

ARG_PRESET="${1:-}"
ARG_INTERFACE="${2:-}"

if [[ -n "$ARG_PRESET" ]]; then
    valid=0
    for p in "${PRESETS[@]}"; do
        [[ "$p" == "$ARG_PRESET" ]] && valid=1 && break
    done
    if [[ $valid -eq 0 ]]; then
        echo "Error: unknown preset \"$ARG_PRESET\"." >&2
        usage
        exit 1
    fi
fi

load_cache() {
    CACHED_PRESET="" CACHED_INTERFACE=""
    if [[ -f "$CACHE_FILE" ]]; then
        # shellcheck source=/dev/null
        source "$CACHE_FILE"
    fi
}

save_cache() {
    printf 'CACHED_PRESET=%q\nCACHED_INTERFACE=%q\n' \
        "$PRESET" "$INTERFACE" > "$CACHE_FILE"
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

# --- Preset ---
if [[ -n "$ARG_PRESET" ]]; then
    PRESET="$ARG_PRESET"
else
    PRESET=$(select_option "Select build type:" "$CACHED_PRESET" "${PRESETS[@]}")
fi

# --- Interface ---
# ip(8) prints veth and VLAN devices as "eth0@if4" / "eth0.100@eth0"; only the
# part before the @ is an interface name that can be bound.
mapfile -t INTERFACES < <(ip -o link show | awk -F': ' '{print $2}' | cut -d@ -f1 | grep -v '^lo$' | sort -u)
if [[ ${#INTERFACES[@]} -eq 0 ]]; then
    echo "Error: no non-loopback network interfaces found." >&2
    exit 1
fi
if [[ -n "$ARG_INTERFACE" ]]; then
    INTERFACE="$ARG_INTERFACE"
else
    INTERFACE=$(select_option "Select upload interface:" "$CACHED_INTERFACE" "${INTERFACES[@]}")
fi

save_cache

echo ""
echo "Building: preset=$PRESET  interface=$INTERFACE"
echo ""

mkdir -p "$SCRIPT_DIR/build" "$SCRIPT_DIR/out"

cd "$SCRIPT_DIR/build"
cmake .. --preset="$PRESET" -DUPLOAD_INTERFACE="$INTERFACE"
cd "$PRESET"
cmake --build . --target upload -j"$(nproc)"
